#ifndef IdaWebInterface_h
#define IdaWebInterface_h

//CB>-------------------------------------------------------------------
//
//   File:      IdaWebInterface.h
//   Revision:  1.3
//   Date:      28-MAY-2010 10:54:02
//
//   DESCRIPTION:
//     IDA.plus Web interface.
//     WebInterface is derived from Process.
//
//<CE-------------------------------------------------------------------

static const char * SCCS_Id_IdaWebInterface_h = "@(#) IdaWebInterface.h 1.3";

#include <pcpdefs.h>
#include <pcpstring.h>
#include <pcpprocess.h>
#include <pcptime.h>
#include <vector.h>
#include <map.h>
#include <toolbox.h>
#include <reporterclient.h>
#include <comman.h>
#include <IdaTypes.h>
  #include <sys/commannorouter.h>
  #include <pcpeventdispatcher.h>
  #include <pcpeventhandler.h>
#include <dispatchable.h>
#include <message.h>
#include <signal.h>
#include <serversockettcp.h>
#include <streamsocketdata.h>
#include "IdaDatabaseList.h"


class ComMan;
class EventDispatcher;
class WebConnection;

class WebInterface : public PcpEventHandler
{

public :
	
		/** Konstruktor */
  WebInterface(
    UShort				webInterfacePort,		// Port-Nummer des Sockets
    ULong				sTimeout		// Timeout fuer Suchanfragen
    );		
  // Shared Memory Objekt

		/** Destruktor */
  ~WebInterface();

  // init of the web process
  ReturnStatus init ();

  // add a configured DB to OID mapping entry
  Void addDb(RefId dbid, TdfAccess* tdfAccess);

  ReturnStatus removeConnection(const WebConnection* webConnection);
	
  ReturnStatus sendRequestToTdfClient(const String& queryStr, WebConnection& connection, ULong& timeOut);

  ReturnStatus reportEvent ( ReportClass  reportClass,
         ULong	      reportId,
         const String & userData = "" );

  ReturnStatus reportProblem ( ReportClass  reportClass,
				  ULong        reportId,
				  const String & userData = "" );

  ReturnStatus    reportAlarm ( ReportClass  reportClass,
				ULong        reportId,
				const String & userData = "" );

  ReturnStatus    sendResponse(UShort requestId, const String& string);
		
private :
  /** Behandelt die Filedescriptor-Events wenn ein Client versucht via
      Socket zu kommunizieren */
  ReturnStatus		handleEvent(PcpReturnEvent & event);
  Void				handleServerSocketEvent();

    /** Extrahiert aus einem XML-Dokument (queryStr) den Wert eines Elementes. Der zugehörende
			Tag-Name wird in tagName übergeben. Der Tagname darf nur einmal im Dokument auftauchen,
			sonst wird das erste Vorkommen ausgewertet. Der Wert wird als long zurück gegeben */
		ReturnStatus		getValueOfXMLElement(const char* queryStr, const char* tagName, long& value);

		/** Extrahiert aus einem XML-Dokument (queryStr) den Wert eines Elementes. Der zugehörende
			Tag-Name wird in tagName übergeben. Der Tagname darf nur einmal im Dokument auftauchen,
			sonst wird das erste Vorkommen ausgewertet. Der Wert wird als String zurück gegeben */
		ReturnStatus		getValueOfXMLElement(const char* queryStr, const char* tagName, String& value);

		/** Die Methode ermittelt die nächste Zahl, die als RequestID für den nächsten Request
			benutzt werden kann. Sie berücksichtigt dabei die unterschiedlichen ID Bereiche der
			verschiedenen WebProzesse */
		UShort				getNextRequestId();
	

		/** Zur Vereinfachung der Reporter-Ausgaben. Der Pointer auf den Reporter wird
			nur beim ersten Aufruf tatsächlich geholt */
		static ReporterClient* reporter();

    typedef s_map<PcpHandle, WebConnection*> WebConnections;
    WebConnections _connections;
    WebConnection* findConnectionByRequestId( UShort requestId ) const;

		ReporterClient*		repClient;			// Zugriffs-Objekt für den Reporter-Prozeß
		SysPoll* 		sysPoll;			// Zugriff-Objekt auf den System Poll	

# ifdef CLASSLIB_03_00
		PcpEventDispatcher*	eventDispatcher;	// dito. für den Dispatcher
		ComManNoRouter*         comManNoRouter;                 // Zugriff-Objekt auf den Kommunikations-Manager
# else
		EventDispatcher*     eventDispatcher;		// dito. für den Dispatcher
		 ComMan*                         comMan;                 // Zugriff-Objekt auf den Kommunikations-Manager
#endif
		UShort				_requestCounter;		// Hilfsvariable zur Request ID Berechnung
		DatabaseList		_databaseList;		// Liste mit allen TdfAccess-ObjectId's und 
												// zugehörenden Database ID's
		ULong				searchTimeout;		// Timeout für Suchanfragen
		ServerSocketTcp		serverSocket;		// Server Socket
		UShort				_webInterfacePort;		// Portnummer des Server Sockets
		String				msgString;			// Variable für den Request-String
		UShort				blockNumberCounter;	// Zähler für die Fehlererkennung bei Reihenfolge-Prüfung
		int				retryCounter; // Zaehler fuer die Anzahl der Versuche ueber ein TdfAccess Objekt eine DB-Verbindung zu erreichen (Backup)
		
		// Nur für Testzwecke
		PcpTime                timeStamp1;
		PcpTime                timeStamp2;

		ULong	   			reqCounter;
		ULong				requestDauer;
		ULong				requestDauerMin;
		ULong				requestDauerMax;
		ULong				responseCounter;
		ULong				responseDauer;
		ULong				responseDauerMin;
		ULong				responseDauerMax;
		ULong				gesamtCounter;
		Ulong				gesamtDauer;
		Ulong				gesamtDauerMin;
		Ulong				gesamtDauerMax;
};

#endif	// IdaWebInterface_h


// *** EOF ***


