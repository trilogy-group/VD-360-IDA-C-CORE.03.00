#ifndef IdaTdfAccess_h
#define IdaTdfAccess_h

//CB>-------------------------------------------------------------------
//
//   File:      IdaTdfAccess.h
//   Revision:  1.1
//   Date:      28-MAY-2010 10:53:58
//
//   DESCRIPTION:
//
//
//<CE-------------------------------------------------------------------

static const char * SCCS_Id_IdaTdfAccess_h = " (#) IdaTdfAccess.h 1.1";


#include <pcpdefs.h>
#include <pcpstring.h>
#include <pcpxml.h>
#include <osaticket.h>
#include <toolbox.h>
#include <reporterclient.h>
#include <tdfclient.h>
#include <IdaTdfChannelMgr.h>
#include <IdaRequestContainer.h>
#include <IdaRequestList.h>
#include <tdferrorarg.h>
#include <tdfaddress.h>
#include <tdforiginid.h>
#include <tdsrequest.h>
#include <pcptimerinterface.h>
#include <modifyrequest.h>
#include <modifyresponse.h>

class TdsResponse;
class WebInterface;

////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
class TdfAccess : public TdfClient, public PcpTimerInterface
{
		// -------------------------------------------------------------------------
		// types
		// -------------------------------------------------------------------------
	private:
	
		// -------------------------------------------------------------------------
		// Status
		// -------------------------------------------------------------------------
		enum TdfAccessStatus
		{
			start					= 0,	// Anfangszustand
			initializationFailed	= 1,	// Initialisierung schlug fehl
			initialized				= 2,	// Initialisierung erfolgreich
			tryingToRegister		= 3,	// war noch nie registriert aber Request abgeschickt
			unAvailable				= 4,	// nicht verfügbar
			registered				= 5		// registriert fertig Anfragen zu bearbeiten
		};
	
		Void					changeStatus(enum TdfAccessStatus newStatus);
		TdfAccessStatus			getStatus() const { return status; }

    WebInterface& _webInterface;
	
	
		// -------------------------------------------------------------------------
		// Konstruktor / Destruktor
		// -------------------------------------------------------------------------
	public:
	
		TdfAccess(WebInterface& _webInterface,
                Long              dbId,			// Database ID
                const ObjectId&   serverService,// für TdfAccess
                const String&     serviceName,	// 
                const String&     applName,		// 
                UShort            nChannels,		// Anzahl anzufordernder Channels
                const String&     osaTicket,		// 
                ULong             regTimer,		// Timeout für Registrierung
                ULong             searchTimeout,// Timeout für Suchanfragen
                Short              maxReg 		   // Max. Anzahl Registrierungsversuche
        );
	
  ~TdfAccess();
	
	
		ReturnStatus    initLocalTdfAddress();
	
		/** Resolve client "browser" IP address */
		ULong           resolveClientIp(String clientIp);
	
	
	
	private:
		TdfAccessStatus status;
	
		String                  osaTicketFileName;
		ReturnStatus            prepareOsaTicket();
	
		// -------------------------------------------------------------------------
		// Registrierung bei der Datenbank (NDIS)
		// -------------------------------------------------------------------------
	private:
		/** length of time in milli seconds after which registration with NDIS DAP is repeated */
		UShort         registerTimer;
		Bool				registerTimerFlag;
		/** max. number of registration attempts */
		Short           maxRegistrationAttempts;
		/** Timer ReferenzNummer */
		RefId           registerTimerId;
		UShort          registrationRetryCounter;
	
	
		ReturnStatus        startRegistration();
	
		ReturnStatus        sendRegisterRequest();
		ReturnStatus        sendDeRegisterRequest();
	
		/** Handle registerRes from TDF Server */
		Void                registerConfirmation(const TdfRegisterRes & arg);
	
		/** Handle deRegisterReport from TDF Server */
		Void                deRegisterReportIndication(const TdfDeRegisterReport & arg);
	
		/**	Startet einen Timer einmalig für die Registrierung */
		ReturnStatus        startRegisterTimer();
	
		/** Canceled einen, für die Registrierung laufenden, Timer */
		ReturnStatus        cancelRegisterTimer();
	
	
	
	
	
	
	public:
	
		// public methodes -----------------------------------------------------
	
	
		// Handle statusReport from TDF Server
		Void            statusReportIndication(const TdfStatusReport & arg);
	
		// Handle searchRes from TDF Server
		Void            searchConfirmation(const TdfResponse & arg);
	
		// Message handler
		Void			handleRequest(const UShort requestId, const String& xmlString);
	
		Void            timerBox(const RefId);
	
		// Receive shutdown message
		Void            shutdown(ObjectId & myId);
	
	
	private:
	
		// private methodes ----------------------------------------------------
	
		Void            assembleTdfOriginId( TdfOriginId* orId, const char* clientAddress);
	
		ReturnStatus    getParametersFromDom( ClNode * rootNode,
											String& clientIpAddressString,
											String& linkContextString);
		ReturnStatus    getTimeoutFromDom ( ClNode * rootNode, ULong& timeout);
	
	
		// Konvertierung TDS > XML direkt
		ReturnStatus    convTdsResponseToXml(const TdsResponse& tdsResponse,
											 String& xmlString,
											 String& typeString);
	
		ReturnStatus    convTdfErrorArgToXml(const TdfErrorArg& tdfErrorArg,
											 String& xmlString,
											 String& typeString);
	
		ReturnStatus    convIndentRecordToXml(
											 String& xmlString,
											 const IndentRecord& indentRecord);
	
		ReturnStatus    sendResponse(UShort requestId, const String& string);
	
		Void            dataToHexString(String byteSequence, String& result);
	
		UShort          getNextSearchId()
		{
			return ++searchId;
		}
		UShort          getActualSearchId() const
		{
			return searchId;
		}
		UShort          getNextReferenceId()
		{
			return ++referenceId;
		}
		UShort          getActualReferenceId() const
		{
			return referenceId;
		}
		ULong           getNextOpId()
		{
			return curOperationId++;
		}
	
	
	
		ReturnStatus    startSendRetryTimer();
		ReturnStatus	cancelSendRetryTimer();
	
	
	
		/** Zur Vereinfachung der Reporter-Ausgaben. Der Pointer auf den Reporter wird
			nur beim ersten Aufruf tatsächlich geholt */
		static ReporterClient* reporter();
	
	
	
		Data&           hexToData(const char * hex);
		UChar           ucharOfString(const char* str);
		ULong           ulongOfString(const char* str);
		UShort          ushortOfString(const char* str);
	
	
	
	
		// ---------------------------------------------------------------------
		// methodes to create a TdsRequest-object from a DOM-document
		ReturnStatus    createTdsRequestFromDom(TdsRequest& tdsRequest, ClNode * rootNode, String& errorText);
		ReturnStatus    createSearchSpecification(TdsRequest& tdsRequest, ClNode* node, String& errorText);
		ReturnStatus    createSearchVariation(TdsRequest& tdsRequest, ClNode* node, String& errorText);
		ReturnStatus    createRequestedResponse(TdsRequest& tdsRequest, ClNode* node, String& errorText);
		ReturnStatus    createSearchAttributes(TdsRequest& tdsRequest, ClNode* node, String& errorText);
		ReturnStatus	getValueOfDomNode(String& textValue, ClNode* node);
	
		
		Void			processSendQueue();
	
		ReturnStatus	createTdfArgument(TdfArgument*	tdfArgument,
										  TdsRequest&	tdsRequest,
										  ClNode * rootNode,
										  UShort        requestId,
										  String&       errorText);
	
		ReturnStatus	createTdfArgument(TdfArgument*	tdfArgument,
										  ModifyRequest&	modifyRequest,
										  ClNode * rootNode,
										  UShort        requestId,
										  String&       errorText);
	
	
		Void            handleErrorArg(TdfErrorArg tdfErrorArg);
	
		Void			refuseRequest(RequestContainer& requestContainer,
									  String errorCode,
									  String errorText);
		Void			sendAuthenticationAcknowledge(RequestContainer& requestContainer);

		virtual Void modifyConfirmation( const TdfResponse & arg );
		
		ReturnStatus createModifyRequestFromDom(ModifyRequest& modifyRequest,
                                                 ClNode * rootNode,
                                                 String& errorText);	

	 	ReturnStatus createModifySpecification(ModifyRequest& modifyRequest, ClNode* node, String& errorText);

		ReturnStatus createModifyAttributes(ModifyRequest& modifyRequest, ClNode* node, String& errorText);

		ReturnStatus createModifyFlags(ModifyRequest& modifyRequest, ClNode* node, String& errorText);

		Void convModifyResponseToXml(ModifyResponse & modifyResponse, String & xml, String & typeString);
	
	private:
	
		// private member ------------------------------------------------------
	
	
		String                  countryCode;		// country oder database identifier für Reporter
		String                  applicationName;	// Name für die Anmeldung bei NDIS
		UShort                  numberOfChannels;	// Anzahl Channels, die von NDIS angefordert werden
		OsaTicket               osaTicket;			// OSA Ticket für den DB Zugriff
		TdfAddress              localTdfAddress;	// 
		String                  serviceName;		// Parameter aus "ida.par"
		ULong                   searchTimeOut;		// timeout in Millisecunden für DB-Anfragen
		Long                    databaseId;			// 
		UShort                  applicationId;
		UShort                  referenceId;
		UShort                  searchId;
		ULong                   curOperationId;
		Short                   searchTimerMsgNr;
		TdfChannelMgr           channelMgr;
		RequestList             workList;
		RequestList             waitingList;
		RequestList				requestPool;		// Liste mit den 
		Data                    tempData;
		//DOMDocument            requestDOM;
	
		RefId					retryTimerId;		// TimerID des retry timers
		Bool					retryTimerFlag;		// Timer aktiv oder nicht
	

  		// ------------------------------------------------------------
		// nur für Tests:
		String                  dummyResponse;
		char                    dbIdString[20];
		PcpTime                    timeStamp1;
		PcpTime                    timeStamp2;
#ifdef MONITORING
		ULong					requestCounter;
		ULong					requestDauer;
		ULong					requestDauerMin;
		ULong					requestDauerMax;
		ULong					responseCounter;
		ULong					responseDauer;
		ULong					responseDauerMin;
		ULong					responseDauerMax;
		ULong					dbCounter;
		ULong					dbDauer;
		ULong					dbDauerMin;
		ULong					dbDauerMax;
#endif // MONITORING
	
};




#endif	// IdaTdfAccess_h


// *** EOF ***

