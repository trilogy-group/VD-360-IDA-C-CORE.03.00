#include "IdaWebConnection.h"
#include "IdaWebInterface.h"
#include "IdaXmlHelper.h"
#include <idatraceman.h>
#include <IdaDecls.h>
#include <pcpprocess.h>
#include <sys/commannorouter.h>

WebConnection::WebConnection(WebInterface& webInterface, StreamSocketBase& streamSocket) :
  _webInterface(webInterface),
  _streamSocket(streamSocket),
  _requestId(0),
  _timerId(-1),
  _requestCompleted(false)
{
}

WebConnection::~WebConnection()
{
}

ReturnStatus WebConnection::init()
{
  ComManNoRouter* comManNoRouter = Process::getComManNoRouter();
  PcpEventDispatcher* eventDispatcher = comManNoRouter->getEventDispatcher();
  if ( isNotOk == eventDispatcher->checkIn(_streamSocket.getSockFd(), 
#ifdef _WINDOWS
    pcpReadMask | pcpAcceptMask | pcpCloseMask,
# else
    pcpReadMask,
# endif
    this ) )
  {
    idaTrackFatal(("checkInFd(streamSocket...) failed!"));
    #ifdef MONITORING
      cout << "*** checkInFd(streamSocket...) failed" << endl;
    #endif
    return isNotOk;
  }

  // Neue Verbindung eintragen
  idaTrackExcept(("***** streamSocket checked in"));
  return isOk;
}

Void WebConnection::dispose()
{
  ComManNoRouter* comManNoRouter = Process::getComManNoRouter();
  PcpEventDispatcher* eventDispatcher = comManNoRouter->getEventDispatcher();
  if ( isNotOk == eventDispatcher->checkOut ( _streamSocket.getSockFd() ) )
  {
    idaTrackFatal(("***** stream socket check out failed !"));
  }

  // 24.09.2009, cha:
  // On Windows you must either call shutdown() on the socket, or
  // set the SO_LINGER option with l_onoff=0 to achieve a proper
  // ("graceful") shutdown of the socket with closesocket(). If
  // you set the SO_LINGER option to l_onoff=1 with a non-zero
  // l_linger value (e. g. 5 seconds), closesocket() will either
  // reset or terminate the socket without a TCP FIN packet being
  // sent. This would cause an error on the client side (IDA servlet).
  // The InputStreamReader in the IDA servlet (see OsaConnection.java)
  // would not return null, causing improper processing of the
  // sent response.
#ifdef _WINDOWS
  _streamSocket.setLinger(false);
  _streamSocket.setBlocking(false);
 // ::shutdown(_streamSocket.getSockFd(), SD_BOTH); (should be done by streamSocket.closeSocket)
#else
  _streamSocket.setLinger ( true, 5 );
  _streamSocket.setBlocking(false);
#endif

  _streamSocket.closeSocket();
  idaTrackExcept(("***** socket checked out, closed !"));

  if ( _timerId != -1 )
  {
    stopTimer( _timerId );
    _timerId = -1;
  }
  
  _webInterface.removeConnection( this );
  delete this;
}

PcpHandle WebConnection::getSockFd() const
{
  return _streamSocket.getSockFd();
}

Void WebConnection::setRequestId(UShort requestId)
{
  _requestId = requestId;
}

UShort WebConnection::getRequestId() const
{
  return _requestId;
}

ReturnStatus WebConnection::handleEvent( PcpReturnEvent & event)
{
  TRACE_FUNCTION("WebConnection::handleEvent(...)");
  PcpHandle fd = event.getSignaledHandle();
  if ( fd == _streamSocket.getSockFd() )
  {
    handleStreamSocketEvent();
  }
  return isOk;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Wenn wir ein Timer-Event erhalten bedeutet das immer, dass ein Fehler 
//	aufgetreten ist. Eine XML_Dokument vom TdfAccess muﬂ immer vollst‰ndig
//	innerhalb der geforderten Zeit eingetroffen sein, sonst wird es verworfen
//
Void WebConnection::timerBox(const RefId id)
{
  TRACE_FUNCTION("WebConnection::timerBox(...)");
  if ( id == _timerId )
  {
    // No further timer event expected (timer repeat times is "once").
    _timerId = -1;

    idaTrackExcept(("Query timed out"));
    _webInterface.reportEvent(iDAMinRepClass, 102, "");
    
    // Fehler Dokument erzeugen
    String msgString;
    XmlHelper::formatXMLErrorResponse(msgString, "webProcess", "timeout", "timeout message received");

    // Fehler Dokument senden
    sendViaSocketAndClose(msgString);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Wenn die Verbindung zum Client steht l‰sst das senden der Daten (vom Client zum WebInterface) 
//	ein Event f¸r den Stream-Socket aus der hier behandelt wird
//
Void WebConnection::handleStreamSocketEvent()
{
    TRACE_FUNCTION("WebConnection::handleStreamSocketEvent(...)");
  
  
  // ---------------------------------------------------------------------
  // Request-String (XML) vom Socket lesen
  // Hier wird gepr¸ft, dass die Ergebnisl‰nge kleiner als die Bufferl‰nge ist
  ssize_t	receivedBytes;
//svc	_streamSocket.setBlocking(false);
  _streamSocket.setBlocking(true);
  char buffer[Types::MAX_LEN_QUERY_STRING];
  if (_streamSocket.readStream(buffer, Types::MAX_LEN_QUERY_STRING - 1, receivedBytes) != isOk) 
  {
    idaTrackExcept(("socket.readStream() failed. Client closed connection ?"));
    idaTrackTrace(("socket.readStream() receivedBytes = %d", receivedBytes)); 
    _webInterface.reportProblem(iDAMinRepClass, 103, "");
    dispose();
    return;
  }

  idaTrackTrace(("***** reading from stream ok !"));
  // Bei EOF ist die L‰nge 0
  // (Der Client hat den Socket geschlossen)
  if (receivedBytes == 0)
  {
    idaTrackExcept(("***** 0 bytes received, = transmition end"));
    dispose();
    return;
  }

  // Null-Byte Begrenzung
  buffer[receivedBytes] = 0;
  if (!_requestCompleted)
  {
    // Der n‰chste Teil der Nachricht wird angeh‰ngt
    _requestString.cat(String(buffer));
    // Wenn der Request komplett ist ...
    // (Kriterium hierf¸r ist die Existenz des schlieﬂenden XML-Tags)
    UInt index;
    if ( _requestString.isSubStrInStr(XmlHelper::requestTerminationTag1, index)
      || _requestString.isSubStrInStr(XmlHelper::requestTerminationTag2, index)
      || _requestString.isSubStrInStr(XmlHelper::requestTerminationTag3, index)
      || _requestString.isSubStrInStr(XmlHelper::requestTerminationTag4, index)
      || _requestString.isSubStrInStr(XmlHelper::requestTerminationTag5, index)
      )
    {
      idaTrackData(("Request received:\n%s", _requestString.cString()));
      _webInterface.reportEvent(iDAMinRepClass, 204, "");
      // Der Request ist vollst‰ndig
      _requestCompleted = true;
      // Request weiter schicken
      
      ULong timeOut = 0;
      _webInterface.sendRequestToTdfClient(_requestString, *this, timeOut);
      _requestString.clear();

	  if (timeOut > 0)
	  {
		startTimer(_timerId, timeOut, PcpTimerInterface::once);
		idaTrackTrace(("***** start timer !"));
	  }
    }
    else if ( _requestString.isSubStrInStr(XmlHelper::requestStartTag1, index)
      || _requestString.isSubStrInStr(XmlHelper::requestStartTag2, index)
      || _requestString.isSubStrInStr(XmlHelper::requestStartTag3, index)
      || _requestString.isSubStrInStr(XmlHelper::requestStartTag4, index)
      || _requestString.isSubStrInStr(XmlHelper::requestStartTag5, index)
      )
    {
      // OK
    }
    else // no Request Start tag found, illegal request -> close
    {
      idaTrackExcept(("no valid request start tag found -> close"));
      dispose();
    }
  }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Wie der Name der Methode schon sagt: Erst werden die Daten ¸ber den noch offenen Socket 
//	gesende, dann wird der Socket geschlossen. Die "Session" ist damit beendet.
//
ReturnStatus WebConnection::sendViaSocketAndClose(const String& sendString)
{
  TRACE_FUNCTION("WebConnection::sendViaSocketAndClose(...)");

  idaTrackData(("Sending Response with length %d via Socket:\n%s",
                sendString.cStringLen(),
                sendString.cString() ));

  // -------------------------------------------------------------------------
  // Senden der Daten via Socket
  ssize_t bytesSend  = 0;
  size_t  sendLength = sendString.cStringLen();
  char*   str        = (char*)sendString.cString();
  
  _streamSocket.setBlocking(true);

  idaTrackData(("***** writing data within loop in blocking mode"));
  while (sendLength > 0)
  {
    idaTrackData(("***** loop start"));
    if (_streamSocket.writeStreamFixLen(str, sendLength, bytesSend) == isNotOk)
    {
      idaTrackExcept(("***** writeStreamFixLen failed ! (may be ok)"));
      StreamSocketBase::Errors err = _streamSocket.getError();

      if ( ( err == StreamSocketBase::tryAgain ) || ( err == StreamSocketBase::interruptOccured ) )
      {
        // Senden hat nicht direkt geklappt, kann aber beim n‰chsten
        // Mal funktionieren
        idaTrackExcept(("***** err: tryAgain or interruptOccured"));

        PcpHandle handle;
        waitForEvent ( handle, 20);
        continue;
      }

      idaTrackExcept(("Error in method writeStreamFixLen: %d", err));
   
      // Nicht vergessen, dass der Socket irgendwann geschlossen werden muss.
      break;
    }

    idaTrackData(("sendLength=%d, bytesSend=%d", sendLength, bytesSend));
    idaTrackData(("error=%d", (int)(_streamSocket.getError())));

    sendLength -= bytesSend;
    str        += bytesSend;
  }


  {
    PcpTime time;

    idaTrackData(("*** Response written to Socket at  %d:%d:%d,%d\n", time.getHour(), time.getMinute(), time.getSec(), time.getMilliSec()));
  }

  //// -------------------------------------------------------------------------
  //// Es sollen keine Events den StreamSocket betreffend gesendet werden
  //eventDispatcher->checkOut(_streamSocket.getSockFd());

  //idaTrackData(("Zeitmessung"));
  //{
  //  PcpTime t0;

  //  // Eine kleine Pause

  //   PcpHandle handle;
  //   waitForEvent ( handle, 100);
  //   
  //  RelTime delta = PcpTime() - t0;
  //  idaTrackData(("***** sleeping = %d", delta.inMilliSeconds()));
  //}

  // Checkout, close socket, delete this
  dispose();

////	shutdown(_streamSocket.getSockFd(), 0);
//
//	if (_streamSocket.closeSocket() == isNotOk)
//	{
//		// Fehler beim Schlieﬂen des Sockets
//		idaTrackExcept(("WebInterface::sendViaSocketAndClose failed (closeSocket)"));
//		return isNotOk;
//	}
//	idaTrackTrace(("***** socket closed !"));
//	connectionFlag = false;
//
//	#ifdef MONITORING
//		{
//			++gesamtCounter;
//			RelTime delta = Time() - timeStamp1;
//			ULong dauer = delta.inMilliSeconds();
//			cout << dauer << "ms)" << endl;
//			gesamtDauer += dauer;
//			if (dauer < gesamtDauerMin) gesamtDauerMin = dauer;
//			if (dauer > gesamtDauerMax) gesamtDauerMax = dauer;
//			if (gesamtCounter >= 100)
//			{
//				cout << "\tGesamt   min/avg/max [ms] : " 
//					 << gesamtDauerMin << "/"
//					 << gesamtDauer / gesamtCounter << "/"
//					 << gesamtDauerMax
//					 << endl;
//				gesamtDauerMin = 1000000;
//				gesamtDauerMax = 0;
//				gesamtDauer = 0;
//				gesamtCounter = 0;
//			}
//		}
//	#endif
//	#ifdef MONITORING
//		{
//			++responseCounter;
//			RelTime delta = Time() - timeStamp2;
//			ULong dauer = delta.inMilliSeconds();
//			responseDauer += dauer;
//			if (dauer < responseDauerMin) responseDauerMin = dauer;
//			if (dauer > responseDauerMax) responseDauerMax = dauer;
//			if (responseCounter >= 100)
//			{
//				cout << "\tResponse min/avg/max [ms] : " 
//					 << responseDauerMin << "/"
//					 << responseDauer / responseCounter << "/"
//					 << responseDauerMax
//					 << endl;
//				responseDauerMin = 1000000;
//				responseDauerMax = 0;
//				responseDauer = 0;
//				responseCounter = 0;
//			}
//		}
//	#endif


  return isOk;
}
