//CB>-------------------------------------------------------------------
//
//   File:      IdaWebInterface.cc
//   Revision:  1.3
//   Date:      28-MAY-2010 10:54:02
//
//   DESCRIPTION:
//
//<CE-------------------------------------------------------------------
static const char * SCCS_Id_IdaWebInterface_cc = "@(#) IdaWebInterface.cc 1.3";

#include <stdafx.h>
#include <IdaDecls.h>
#include <stdlib.h>
#include <string.h>
#include <decstring.h>
#ifdef _WINDOWS
#include <stdafx.h>
#include <iostream>
#else // not defined _WINDOWS
#include <stdlib.h>
#include <unistd.h>
#include <iostream.h>
#endif
#include <apptimer.h>
#include <idatraceman.h>
#include <pcpdefs.h>
#include <pcpeventdispatcher.h>

#include <IdaFunctionTrace.h>
#include <pcpstring.h>
#include <pcptime.h>
#include <toolbox.h>
#include <pcpostrstream.h>


#include <IdaTypes.h>
#include <IdaDatabaseList.h>
#include "IdaTdfAccess.h"
#include "IdaWebConnection.h"
#include "IdaWebInterface.h"
#include "IdaXmlHelper.h"

#include <IdaXmlTags.h>


// Wenn die Ausgaben nach "cout" unterbleiben sollen, dann diese Zeile auskommentieren !
#ifdef ALLOW_STDOUT
	#define MONITORING
#endif


#define PROBLEM(a,b,c,d) reporter()->reportProblem((a),(b),(c),(d))
#define ALARM(a,b,c,d) reporter()->reportAlarm((a),(b),(c),(d))
#define EVENT(a,b,c,d) reporter()->reportEvent((a),(b),(c),(d))


////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Konstruktor
//
WebInterface::WebInterface(
	UShort					webInterfacePort,
	ULong					sTimeout)
:
	_webInterfacePort    (webInterfacePort),
	searchTimeout	(sTimeout),
	_requestCounter	(0)
{
	idaTrackTrace(("Constructor WebInterface -IN-"));
	
   // content moved to init method since it can fail!
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Destruktor
//
WebInterface::~WebInterface()
{
	idaTrackTrace(("Destructor WebInterface -IN-"));

	EVENT(0, iDAMinRepClass, 202," IdaWebInterface");

	idaTrackTrace(("Destructor WebInterface -OUT-"));
}

ReturnStatus WebInterface:: init ()
{
	idaTrackTrace(("WebInterface: init -IN-"));
	
	// -------------------------------------------------------------------------
	// Einchecken des Prozesses beim Kommunikations-Manager
	// damit wir Messages vom Dispatcher bekommen ( via messageBox() )
	comManNoRouter = Process::getComManNoRouter();
	
  // Init Socket
	idaTrackData(("Socket port #: %d", _webInterfacePort));

  serverSocket.setBackLog(10);
  if (serverSocket.initForConnect(_webInterfacePort) == isNotOk)
	{
		idaTrackFatal(("socket.initForConnect failed!"));
		return isNotOk;
	}

	eventDispatcher =  comManNoRouter->getEventDispatcher();
	// ab diesem Zeitpunkt kann die Methode eventBox() aufgerufen werden
	// Fd bedeutet FileDescriptor
	// "pollIn" : beobachte Pollin-Ereignisse


# ifdef _WINDOWS
	eventDispatcher->checkIn(serverSocket.getListenFd(), pcpAcceptMask, this);
#else   
   eventDispatcher->checkIn(serverSocket.getListenFd(), pcpReadMask, this);
#endif
   
	// -------------------------------------------------------------------------
	EVENT(0, iDAMinRepClass, 201, " IdaWebInterface");
	idaTrackTrace(("Constructor WebInterface -OUT-"));

	#ifdef MONITORING
		cout << "_webInterfacePort      = " << _webInterfacePort << endl;
		cout << "build 0003" << endl;
		cout << "------------------------------------------------------" << endl;
	#endif

		
	reqCounter			= 0;
	requestDauer		= 0;
	requestDauerMin		= 1000000;
	requestDauerMax		= 0;
	responseCounter		= 0;
	responseDauer		= 0;
	responseDauerMin	= 1000000;
	responseDauerMax	= 0;
	gesamtCounter		= 0;
	gesamtDauer			= 0;
	gesamtDauerMin		= 1000000;
	gesamtDauerMax		= 0;
	retryCounter		= 0;

    return isOk;
}

Void WebInterface::addDb(RefId dbid, TdfAccess* tdfAccess)
{
    _databaseList.addDb(dbid, tdfAccess);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Wenn eine Anforderung vom WEB Server (Servlet) kommt, wird die Applikation
//	(bzw. dieser Prozeß) durch Aufruf der Methode eventBox davon benachrichtigt
//
//	Die folgende Zeile kann aktiviert werden, um für Testzwecke den Loopback
//	auf WebInterface-Ebene zu erreichen. Dabei wird der empfangene Request
//	einfach sofort zum Client zurück gesendet
//
//	Sonderfälle, die zu behandeln sind:
//
//	1. Der Client schließt vorzeitig den Socket,
//	   und öffnet ihn dann sofort wieder für einen
//	   neuen Request
//	2. Ein Zweiter Client versucht einen parallele
//	   Anfrage bei einem WebProzess, der bereits seinen
//	   StreamSocket in Betrieb (geöffnet) hat
//
ReturnStatus WebInterface::handleEvent( PcpReturnEvent & event)
{
    TRACE_FUNCTION("WebInterface::handleEvent(...)");
	idaTrackTrace(("Event received"));

	PcpHandle fileDescriptor = event.getSignaledHandle();
   Int  myError = event.getConnectError ();


	// Sollte auch nicht auftreten:
	if (myError != 0)
	{
		idaTrackExcept(("Event error"));
		#ifdef MONITORING
			cout << "\n*** EventError #" << myError << " occured" << endl;
		#endif
		return isNotOk;
	}
	idaTrackData(("***** no pollEvent error detected"));


	// -------------------------------------------------------------------------
	// Am File-Descriptor können wir erkennen, für welchen Socket
	// das Event galt ...
	if (fileDescriptor == serverSocket.getListenFd())
	{
		#ifdef MONITORING
			timeStamp1 = Time();
		#endif
		idaTrackData(("***** event for server socket"));
		handleServerSocketEvent();

		{
		  PcpTime time;
		  idaTrackData(("*** Server Socket Event at        %d:%d:%d,%d",
					   time.getHour(), time.getMinute(), time.getSec(), time.getMilliSec()));
		}
	}
	return isOk;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Wenn ein Event für den Server-Socket eintrifft, dann versucht ein Client seinerseits eine
//	Socket-Verbindung zu öffnen
//
Void WebInterface::handleServerSocketEvent()
{
    TRACE_FUNCTION("WebInterface::handleServerSocketEvent(...)");
	
	// ---------------------------------------------------------------------
	// Socketverbindung herstellen 
	if (serverSocket.connect(true) == isOk)
	{
		// es hat geklappt, wir können uns den StreamSocket holen
		#ifdef MONITORING
			cout << "[S>>|" << flush;
		#endif
		
		// Wir spalten einen StreamSocket ab ...
		WebConnection* connection = new WebConnection(*this, serverSocket);
		idaTrackTrace (("***** streamSocket created"));
		if ( isNotOk == connection->init() )
		{
			idaTrackFatal(("checkInFd(streamSocket...) failed!"));
			#ifdef MONITORING
				cout << "*** checkInFd(streamSocket...) failed" << endl;
			#endif
			delete connection;
		}
		else
		{
			// Neue Verbindung eintragen
			_connections[connection->getSockFd()] = connection;
			idaTrackTrace(("***** streamSocket checked in"));
		}

	}
	else
	{
		// Fehler:
		// Leider können wir kein Fehlerdokument zurück senden
		idaTrackExcept(("***** socket.connect() failed!"));
		#ifdef MONITORING
			cout << "*** socket.connect() failed" << endl;
		#endif
	}
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Hier wird der Request an den TdfClient weitergesandt. Die Entscheidung, welcher TdfClient die
//	Nachricht erhält, wird ebenfalls hier getroffen
//
ReturnStatus WebInterface::sendRequestToTdfClient(const String& queryStr, WebConnection& connection, ULong& timeOut)
{
    TRACE_FUNCTION("WebInterface::sendRequestToTdfClient(...)");
	
	// initiale Anfrage -> retryCounter auf 0
	retryCounter = 0;

	// -------------------------------------------------------------------------
	// Wir benötigen die ID der Datenbank, die angesprochen werden soll
	RefId dbId = -1;
	// ... holen diese aus dem Request
	getValueOfXMLElement(queryStr.cString(), "DBID", dbId);
	// Anhand der DBID können wir die zugehörige ObjectID des entsprechenden
	// TdfAccess-Objektes ermitteln
	TdfAccess* tdfAccess = _databaseList.findTdfAccessOfDB(dbId);
	// Wenn die ObjectId -1 ist, ...
	if (!tdfAccess)
	{
		// ... dann ist ein Fehler aufgetreten
		idaTrackTrace(("findTdfAccessOfDB() failed"));
		
		// Ohne DBID könne wir nicht fortfahren
		// also Fehlerdokument senden
		String xmlString;
    XmlHelper::formatXMLErrorResponse(xmlString, "webProcess", "parameterError", "Wrong or missing 'DBID' value");
		connection.sendViaSocketAndClose(xmlString);

		return isOk;
	}

	
	// -------------------------------------------------------------------------
	// Timeout Wert aus dem Request holen
	timeOut = 0;
	if (getValueOfXMLElement(queryStr.cString(), "Timeout", (long&)timeOut) == isNotOk)
	{
		idaTrackTrace(("findOIDofDB() failed --> default value"));
		
		// Wir nehemen den Default-Wert
		timeOut = searchTimeout;
	}
	if (timeOut <= 100) timeOut = 100;	// = 100 Milli-Sekunden

	const UShort requestId = getNextRequestId();
	tdfAccess->handleRequest(requestId, queryStr);
	connection.setRequestId(requestId);

	#ifdef MONITORING
		cout << "R(" << requestId << ")>>T|" << flush;
	#endif

	{
	  PcpTime time;

	  idaTrackData(("*** Request send to TdfProcess at %d:%d:%d,%d",
						 time.getHour(), time.getMinute(), time.getSec(), time.getMilliSec()));
	}
	
		
	// -------------------------------------------------------------------------
	// Timer aufziehen
	//RefId timerId;
 // startTimer(timerId, timeOut, PcpTimerInterface::once);
	//idaTrackTrace(("***** start timer !"));
 // connection.setTimerId(timerId);

	#ifdef MONITORING
		{
			++reqCounter;
			RelTime delta = Time() - timeStamp1;
			ULong dauer = delta.inMilliSeconds();
			requestDauer += dauer;
			if (dauer < requestDauerMin) requestDauerMin = dauer;
			if (dauer > requestDauerMax) requestDauerMax = dauer;
			if (reqCounter >= 100)
			{
				cout << "\tRequest  min/avg/max [ms] : " 
					 << requestDauerMin << "/"
					 << requestDauer / reqCounter << "/"
					 << requestDauerMax
					 << endl;
				requestDauerMin = 1000000;
				requestDauerMax = 0;
				requestDauer = 0;
				reqCounter = 0;
			}
		}
	#endif
	
	return isOk;
}

ReturnStatus WebInterface::sendResponse(UShort requestId, const String& msgString)
{
  TRACE_FUNCTION("WebInterface::sendReponse(...)");
  
  // Alle Messages mit falscher requestId werden verworfen
  WebConnection* connection = findConnectionByRequestId(requestId);
  if (!connection)
  {
		idaTrackFatal(("No connection for requestId %d", requestId));
    return isNotOk;
  }
  
  // senden ...
  return connection->sendViaSocketAndClose(msgString);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Die Methode holt den Wert eines XML-Elementes aus einer XML-Struktur (String). Voraussetzung
//	ist, dass der Tag-Name eindeutig ist, da hier kein richtiger Parser verwendet wird.
//
ReturnStatus WebInterface::getValueOfXMLElement(const char* queryStr,
											  const char* tagName,
											  String& value)
{
    TRACE_FUNCTION("WebInterface::getValueOfXMLElement(...)");
	
	
	// Wir suchen den Eintrag mit Form "<tagName>nnn</tagName>
	String startTag;
	startTag.cat("<");
	startTag.cat(tagName);
	startTag.cat(">");

	String endTag;
	endTag.cat("</");
	endTag.cat(tagName);
	endTag.cat(">");

	UInt startPos   = 0;
	UInt endPos     = 0;

	String tmpStr(queryStr);

	// Positionen der beiden Tags
	if (!tmpStr.isSubStrInStr(startTag, startPos))     return isNotOk;
	if (!tmpStr.isSubStrInStr(endTag, endPos))         return isNotOk;

	// Auf die Startposition addieren wir noch die Länge des StartTags
	startPos += startTag.cStringLen();

	// Ergebnisstring holen
	String valueStr;
	if (tmpStr.subString(startPos, endPos - startPos, valueStr) == isNotOk)
		return isNotOk;

	// Ergebnis zuweisen
	value = valueStr;

	return isOk;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
ReturnStatus WebInterface::getValueOfXMLElement(const char* queryStr,
											  const char* tagName,
											  long& value)
{
//	TRACE_FUNCTION("WebInterface::getValueOfXMLElement(...)");
	
	
	// Ergebnisstring holen
	String valueStr;
	if (getValueOfXMLElement(queryStr, tagName, valueStr) == isNotOk)
		return isNotOk;

	// String nach long wandeln
	DecString decString(valueStr);
	long val = 0;
	
	if (decString.getValue(val) == isNotOk)
		return isNotOk;

	// Ergebnis zuweisen
	value = val;

	return isOk;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Bei jedem Aufruf dieser Methode wird eine neue Nummer für den Request
//	ermittelt im Bereich 1 ... USHRT_MAX
//
UShort WebInterface::getNextRequestId()
{
//	TRACE_FUNCTION("WebInterface::getValueOfXMLElement(...)");
	// Id inkrementieren
	if ( ++_requestCounter == 0 )
	{
		_requestCounter = 1;
	}
	return _requestCounter;
}

ReturnStatus 
WebInterface::reportEvent ( ReportClass  reportClass,
         ULong	      reportId,
         const String & userData )
{
  return reporter()->reportEvent(0, reportClass, reportId, userData);
}

ReturnStatus 
WebInterface::reportProblem ( ReportClass  reportClass,
				  ULong        reportId,
				  const String & userData )
{
  return reporter()->reportProblem(0, reportClass, reportId, userData);
}

ReturnStatus    
WebInterface::reportAlarm ( ReportClass  reportClass,
				ULong        reportId,
				const String & userData )

{
  return reporter()->reportAlarm(0, reportClass, reportId, userData);
}

WebConnection* 
WebInterface::findConnectionByRequestId( UShort requestId ) const
{
  for (WebConnections::const_iterator it = _connections.begin(); it != _connections.end(); ++it)
  {
    WebConnection* connection = it->second;
    if (connection->getRequestId() == requestId)
    {
      return connection;
    }
  }
  return 0;
}

ReturnStatus
WebInterface::removeConnection(const WebConnection* connection)
{
  for (WebConnections::iterator it = _connections.begin(); it != _connections.end(); ++it)
  {
    if (it->second == connection)
    {
      _connections.erase(it);
      return isOk;
    }
  }
  return isNotOk;
}

ReporterClient* 
WebInterface::reporter()
{
	return Process::getToolBox()->getReporter();
}

// *** EOF ***


