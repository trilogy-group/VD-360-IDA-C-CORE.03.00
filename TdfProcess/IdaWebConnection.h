#ifndef IdaWebConnection_h
#define IdaWebConnection_h

#include <pcpdefs.h>
#include <pcpeventhandler.h>
#include <pcptimerinterface.h>
#include <streamsocketdata.h>

class ReporterClient;
class WebInterface;

class WebConnection : public PcpEventHandler, public PcpTimerInterface
{
private:
  WebInterface& _webInterface;        // The owning web interface
  RefId _timerId;                     // Id of search timer
  UShort _requestId;                  // enth‰lt die aktuelle Request ID
  StreamSocketData _streamSocket;     // Kommunikations Socket

  Bool _requestCompleted;             // true when a complete request XML document was received
  String _requestString;              // the request (in XML format)

public:
  WebConnection(WebInterface& webInterface, StreamSocketBase& streamSocket);
  ~WebConnection();

  ReturnStatus init();
  Void dispose();

  PcpHandle getSockFd() const;

  Void setRequestId(UShort requestId);
  UShort getRequestId() const;

  Void handleStreamSocketEvent();

  /** Sendet einen String zum Client und schlieﬂt 
      anschlieﬂend den Socket */
  ReturnStatus sendViaSocketAndClose(const String& sendString);

private:
  ReturnStatus handleEvent(PcpReturnEvent & event);

  Void timerBox(const RefId id);
};

#endif // IdaWebConnection_h
