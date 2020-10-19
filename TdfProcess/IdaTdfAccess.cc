//CB>-------------------------------------------------------------------
//
//   File:      IdaTdfAccess.cc
//   Revision:  1.8
//   Date:      11-AUG-2010 15:47:51
//
//<CE-------------------------------------------------------------------

static const char * SCCS_Id_TdfAccess_cc = "@(#) IdaTdfAccess.cc 1.10";

#include <stdafx.h>
#include <IdaDecls.h>
#include <idatraceman.h>
#include <pcpdispatcher.h>
#include <pcperrorarg.h>
#include <pcpprocess.h>
#include <messagetypes.h>
#include <message.h>
#include <stdlib.h>
#include <syspar.h>
#include <syspargrp.h>
#include <toolbox.h>
#include <pcpstring.h>
#include <tdfderegisterarg.h>
#include <tdfregisterarg.h>
#include <tdfregisterres.h>
#include <tdfresponse.h>
#include <tdfargument.h>
#include <tdfcancelsearcharg.h>
#include <tdftypes.h>
#include <tdfstatusreport.h>
#include <tdferrorarg.h>
#include <tdsrequest.h>
#include <tdsresponse.h>
#include <modifyrequest.h>
#include <modifyresponse.h>
#include <osaapidefs.h>
#include <decstring.h>
#include <pcpostrstream.h>

#include <sys/types.h>

#ifndef _WINDOWS
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fstream.h>
#include <iostream.h>
#else
#include <pcpcominitializer.h>
#include <fstream>
#include <iostream>
#endif



#include <IdaTypes.h>
#include <IdaTdfChannel.h>
#include <IdaStringToEnum.h>
#include <IdaCpd.h>
#include <IdaRequestContainer.h>
#include "IdaWebInterface.h"
#include "IdaXmlHelper.h"

// Das folgende Makro ist notwendig um unter AIX die XERCES header mit einbinden zu können.
// Diese benutzen leider auch einen boolean Typ der mit dem der STL kollidiert
// In allen Dateien des OSA-Treibers darf nur der Typ "Bool" verwendet werden, damit es
// kein Durcheinander gibt !

#include <IdaTdfAccess.h>

////////////////////////////////////////////////////////////////////////////////////////////////////
//

#ifdef ALLOW_STDOUT
#define MONITORING
#endif

#include <IdaXmlTags.h>


////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Konstruktor
//
TdfAccess::TdfAccess(WebInterface& webInterface,
                     Long				 dbId,
                     const ObjectId& serverService,
                     const String&	 service,
                     const String&	 applName,
                     UShort			 nChannels,
                     const String&	 osaTick,
                     ULong				 regTimer,
                     ULong				 searchTimeout,
                     Short				 maxReg )
                     :
_webInterface				(webInterface),
status						(start),	// Startzustand
databaseId					(dbId),
serviceName					(service),
applicationName				(applName),
numberOfChannels			(nChannels),
osaTicketFileName			(osaTick),
registerTimer				(regTimer > 0 ? regTimer : 180000),
searchTimeOut				(searchTimeout > 0 ? searchTimeout : 10000),
maxRegistrationAttempts		(maxReg > -2 ? maxReg : 10),
applicationId				(0),
referenceId					(0),
searchId					(0),
curOperationId				(1),
registrationRetryCounter	(0),
registerTimerId				(0),
retryTimerFlag				(false),
registerTimerFlag			(false)

{
  TdfClient::checkInUniqueDynamic(serverService, worldScope);

  countryCode = DecString(databaseId).getString();

  TRACE_FUNCTION("TdfAccess::TdfAccess(...)");
  EVENT(0, iDAMinRepClass, 209, countryCode);


  sprintf(dbIdString, "[%d]: ", databaseId);


  idaTrackTrace (( "applicationName = %s", applicationName.cString () ));
  idaTrackTrace (( "databaseId      = %d", databaseId ));
  idaTrackTrace (( "serverService   = %d", serverService.getIdentifier() ));


  ReturnStatus returnStatus = isOk;

  // Initialize OSA ticket
  // Hier wird ein Ticket aus einer Datei geladen, das Default-Ticket
  // Verwendung findet, wenn SES deaktiviert ist
  if (prepareOsaTicket() == isNotOk)		returnStatus = isNotOk;

  // Lokale Tdf-Adresse ermitteln
  if (initLocalTdfAddress() == isNotOk)	returnStatus = isNotOk;

  // Status der Initialisierungen anzeigen
  if (returnStatus == isNotOk)
  {
    idaTrackExcept(("TdfAccess::TdfAccess: Initialization NOT successful"));
    changeStatus(initializationFailed);
  }
  else
  {
    idaTrackData(("TdfAccess::TdfAccess: Initialization successful"));
    changeStatus(initialized);
  }

  // Hier wird der Registrierungs-Mechanismus (inkl. Retry) angestoßen
  startRegistration();

    // initialize COM 
#ifdef _WINDOWS
    m_hr = ::CoInitialize(NULL);

    // event. noch das folgende:
    if ( ! ( SUCCEEDED(m_hr) || (m_hr == RPC_E_CHANGED_MODE ) ) )
    { 
	; // do nothing, COM is already initialized with different Parameters
    }
  idaTrackTrace(("TdfAccess::handleRequest after com init"));
#endif



#ifdef MONITORING
  // Initialisierung einiger Variablen für die Performance-Messung
  requestCounter = 0;
  requestDauer = 0;
  requestDauerMin = 1000000;
  requestDauerMax = 0;
  responseCounter = 0;
  responseDauer = 0;
  responseDauerMin = 1000000;
  responseDauerMax = 0;
  dbCounter = 0;
  dbDauer = 0;
  dbDauerMin = 1000000;
  dbDauerMax = 0;
#endif // MONITORING

}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Destruktor
//
TdfAccess::~TdfAccess()
{
  TRACE_FUNCTION("Destructor TdfAccess(...)");

#ifdef _WINDOWS
      if (SUCCEEDED(m_hr))
    {
      ::CoUninitialize();
    }
#endif
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Shutdown handler
//
Void TdfAccess::shutdown(ObjectId& myId)
{
  TRACE_FUNCTION("TdfAccess::shutdown(...)");
  EVENT(0, iDAMinRepClass, 210, countryCode);

  idaTrackExcept(("Shutdown request received"));

  //	ComMan::getDispatcher()->stopDispatchForever();
  //	ComMan* comMan = Process::getComMan();          
  //	comMan->checkOut(myId);
  //	comMan->dispatcherCheckOut(myId);

  cancelRegisterTimer();
  sendDeRegisterRequest();
  cancelSendRetryTimer();

  checkOut();

  ReturnStatus rs = Process::shutdownComplete(myId);

}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Diese Methode kontrolliert zentral alle Änderungen des internen Zustandes.
//	Der interne Zustand zeigt an, ob das System ordnungsgemäß initialisiert
//	und registriert wurde.
//	Die Zustands-Variable "status" sollte also in keinem Fall direkt gesetzt
//  werden. Illegale Zustandsübergänge werden hier erkannt und angezeigt.
//
Void TdfAccess::changeStatus(TdfAccessStatus newStatus)
{
  TRACE_FUNCTION("TdfAccess::changeStatus(...)");

  // Nur echte Änderungen behandeln
  if (status == newStatus) return;

  // Alten Zustand sichern
  TdfAccessStatus oldStatus = status;

  // Flag, das anzeigt, ob eine Zustandsänderung stattgefunden hat
  int changed = 0;

  switch (status)
  {
    // -----------------------------------------------------------
  case start :
    if (newStatus == initializationFailed || newStatus == initialized)
    { status = newStatus; changed = 1; }
    break;
    // -----------------------------------------------------------
  case initializationFailed :
    // Nichts geht mehr !
    break;
    // -----------------------------------------------------------
  case initialized :
    if (newStatus == tryingToRegister)
    { status = newStatus; changed = 1; }
    break;
    // -----------------------------------------------------------
  case tryingToRegister :
    if (newStatus == registered || newStatus == unAvailable)
    { status = newStatus; changed = 1; }
    break;
    // -----------------------------------------------------------
  case unAvailable :
    if (newStatus == initialized)
    { status = newStatus; changed = 1; }
    break;
    // -----------------------------------------------------------
  case registered :
    if (newStatus == initialized || newStatus == tryingToRegister)
    { status = newStatus; changed = 1; }
    break;
  }

  if (changed == 1)
  {
    idaTrackData(("Internal status changed : %d --> %d", oldStatus, newStatus));
  }
  else
  {
    idaTrackExcept(("Illegal status changed was attempted : %d --> %d", oldStatus, newStatus));
  }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
ReturnStatus TdfAccess::initLocalTdfAddress()
{
  TRACE_FUNCTION("TdfAccess::initLocalTdfAddress(...)");

  Char tmpHostName[256] = {0};
  gethostname(tmpHostName, 256);

  idaTrackData(("initLocalTdfAddress(): gethostname = %s", tmpHostName));

  ULong localIpAddress = resolveClientIp(tmpHostName);
  if (localIpAddress == 0)
  {
    //localTdfAddress.setObjectId(myObjectId.getIdentifier());
    idaTrackFatal(("resolveClientIp(%s) failed !", tmpHostName));
    return isNotOk;
  }

  localTdfAddress.setIpAddress(localIpAddress);
  return isOk;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
ULong TdfAccess::resolveClientIp(String clientIp)
{
  TRACE_FUNCTION("TdfAccess::resolveClientIp(...)");

  idaTrackData(("resolveClientIp for IpAddress : %s", clientIp.cString()));

  struct hostent*  host;
  static struct in_addr saddr;

  // First try it as "aaa.bbb.ccc.ddd"
  saddr.s_addr = inet_addr(clientIp.cString());
  if (saddr.s_addr != -1)
  {
    // 2008-01-17, cp
    return ntohl(saddr.s_addr);
  }

  // ... or try it as name
  host = gethostbyname(clientIp.cString());
  if (host != NULL)
  {
    struct in_addr *inAddr;
    inAddr =  (struct in_addr *) *host->h_addr_list;
    // 2008-01-17, cp
    return ntohl(inAddr->s_addr);
  }

  idaTrackExcept(("resolveClientIp() failed !"));

  return 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Hier wird ein universelles OsaTicket aus einer Datei eingelesen
// 
ReturnStatus TdfAccess::prepareOsaTicket()
{
  TRACE_FUNCTION("TdfAccess::prepareOsaTicket(...)");

  s_ifstream osaTicketFile(osaTicketFileName.cString());
  if (osaTicketFile)
  {
    idaTrackData(("prepareOsaTicket(): Reading osaTicket file."));
    osaTicketFile >> osaTicket;
    osaTicketFile.close();
    idaTrackData(("Ticket:\n%s", osaTicket.dumpString()));
    return isOk;
  }
  else
  {
    idaTrackFatal(("prepareOsaTicket(): Cannot open osaTicket file \"%s\" !",
      osaTicketFileName.cString()));
    return isNotOk;
  }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Diese Methode bereitet den egistrierungs-Mechanismus vor und startet ihn
//
ReturnStatus TdfAccess::startRegistration()
{
  TRACE_FUNCTION("TdfAccess::startRegistration(...)");

  // Wenn die Registrierung bereits läuft, wird nicht gemacht
  if (getStatus() == tryingToRegister) return isOk;

  // Status ändern
  changeStatus(initialized);
  changeStatus(tryingToRegister);


  // Wir setzen den Registrierungs-Zähler zurück
  registrationRetryCounter = 0;

  EVENT(0, iDAMinRepClass, 211, "");

  // Nachricht senden
  return sendRegisterRequest();
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Hier wird ein Request-Objekt für die Registrierung aufgebaut und gesendet. Anschießend wird
//	der Registrierungs-Timer gestartet. Die Überprüfung der maximalen Registrier-Wiederholungen
//	wird ebenfalls an dieser Stelle überwacht und behandelt.
//
ReturnStatus TdfAccess::sendRegisterRequest()
{
  TRACE_FUNCTION("TdfAccess::sendRegisterRequest(...)");

  idaTrackTrace (( "sendRegisterRequest() called" ));

  // -------------------------------------------------------------------------
  // Der richtige Status muß vorliegen
  if (getStatus() != tryingToRegister) return isNotOk;


  if ((registrationRetryCounter >= maxRegistrationAttempts) && (maxRegistrationAttempts !=-1) )
  {
    idaTrackFatal(("Max registration (#%d) count exceeded", maxRegistrationAttempts));
    ALARM(0, iDAMinRepClass, 4, countryCode);

    idaTrackTrace (( "registrationRetryCounter (%d) >= maxRegistrationAttempts (%d)",registrationRetryCounter, maxRegistrationAttempts ));
    idaTrackTrace (( "registration attempt stopped ! *** SERVICE NOT AVALABLE ***" ));

    changeStatus(unAvailable);

    return isNotOk;
  }
  else
  {
    // RegisterCounter inkrementieren
    ++registrationRetryCounter;
    if (maxRegistrationAttempts == -1 )
    {
      idaTrackData(("max registration #%d, registration attempt #%d", maxRegistrationAttempts, registrationRetryCounter));
    }
    else
    {
      idaTrackData(("registration attempt #%d", registrationRetryCounter));
    }
  }

  // ---------------------------------------------------------------------
  // OsaLimits::getInstance()->setOsaComErrorCode(0);
  TdfRegisterArg tdfRegisterArg;
  tdfRegisterArg.setSourceApplication(applicationName);
  tdfRegisterArg.setRequestedChannels(numberOfChannels);
  tdfRegisterArg.setOsaTicket(osaTicket);
  tdfRegisterArg.setDataFormat(tdsDataFormat);
  tdfRegisterArg.setReference(getNextReferenceId());
  // Validate data
  if (tdfRegisterArg.areMandatoryItemsSet() != true)
  {
    idaTrackFatal(("Error: TdfRegisterArg not all mandatory Items are set!"));
    return isNotOk;
  }

  // ---------------------------------------------------------------------
  // Send register request to TDS server
  idaTrackData(("We are before registerRequest!!"));
  if (registerRequest(tdfRegisterArg) == isOk)
  {
    idaTrackData(("registerRequest() OK!"));
  }
  else
  {
    OsaComError error = getLastError();

    idaTrackExcept(("registerRequest() failed. %s", error.getErrorText()));
    PROBLEM(0, iDAMinRepClass, 120, "");
  }

  // Start registration timer
  return startRegisterTimer();
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Vorbereitung und Durchführung der De-Registrierung von der Datenbank
//
ReturnStatus TdfAccess::sendDeRegisterRequest()
{
  TRACE_FUNCTION("TdfAccess::sendDeRegisterRequest(...)");

  TdfDeRegisterArg tdfDeRegisterArg;
  tdfDeRegisterArg.setApplicationId(applicationId);
  if (deRegisterRequest(tdfDeRegisterArg) == isNotOk)
  {
    idaTrackData(("Error sending deRegisterRequest!"));
    return isNotOk;
  }

  EVENT(0, iDAMinRepClass, 213, "");
  idaTrackData(("deRegisterRequest sent!"));

  return isOk;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Implementierung einer Methode der abstrakten Superklasse "TdfClient".
//
//	Diese Methode wird vom Framework der ODAAPI aufgerufen, als Antwort auf eine Registrierung.
//
Void TdfAccess::registerConfirmation(const TdfRegisterRes& tdfRegisterRes)
{
  TRACE_FUNCTION("TdfAccess::registerConfirmation(...)");
  EVENT(0, iDAMinRepClass, 212, "");


  // -------------------------------------------------------------------------
  // Check register reference ID
  UShort refId = 0;
  if (tdfRegisterRes.getRegisterRef(refId) == isNotOk)
  {
    idaTrackExcept(("Error: tdfRegisterRes.getRegisterRef(refId) failed !"));
    return;
  }
  if (refId != getActualReferenceId())
  {
    idaTrackExcept(("Error: refId != getActualReferenceId()"));
    return;
  }

  // -------------------------------------------------------------------------
  // Cancel registration timer
  cancelRegisterTimer();


  // -------------------------------------------------------------------------
  // Check TDS response error code
  if (tdfRegisterRes.isErrorArgSet())
  {
    // Der TdfServer hat zwar geantwortet, aber irgendetwas ist schief gegangen
    TdfErrorArg errorArg;
    errorArg = tdfRegisterRes.getErrorArg();
    idaTrackExcept(("registerConfirmation(): ErrorText = %s", errorArg.getErrorText()));

    // Wir versuchen eine erneute Registrierung
    // (aber nicht sofort, sonst würde der Server mit Nachrichten überflutet werden)
    startRegisterTimer();
    return;
  }

  // Extract relevant data
  tdfRegisterRes.getApplicationId(applicationId);
  UShort confirmedChannels = 0;
  tdfRegisterRes.getConfirmedChannels(confirmedChannels);
  if (confirmedChannels != numberOfChannels)
  {
    idaTrackExcept(("Requested %d channels, but %d channels have been confirmed", 
      numberOfChannels, confirmedChannels));
  }


  idaTrackData(("TDS applId:%d, #channels:%d", applicationId, confirmedChannels));

  // Initialize all channels
  channelMgr.setNoOfChannels(confirmedChannels);


  // Setup status
  changeStatus(registered);


  idaTrackData(("Registration of TdfAccess successful"));

}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Implementierung einer Methode der abstrakten Superklasse "TdfClient".
//
//  Diese Methode wird vom TDF-Server aufgerufen wenn er den Client (also uns)
//	deregistriert
//
Void TdfAccess::deRegisterReportIndication(const TdfDeRegisterReport & arg)
{
  TRACE_FUNCTION("TdfAccess::deRegisterReportIndication(...)");
  PROBLEM(0, iDAMinRepClass, 215, "");
  idaTrackExcept(("deRegisterReportIndication() received for %s", dbIdString ));

  // Wir stoppen einen evtl. laufenden Registrierungs-Timer ...
  cancelRegisterTimer();

  // ... und weisen alle neuen Anfragen zurück mit:
  changeStatus(initialized);

  // -------------------------------------------------------------------------
  // We will try to register again, assuming the deregistration is temporary
  idaTrackData(("TdfAccess::deRegisterReportIndication starts registration() "));	
  startRegistration();
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Starts the Registertimer. Die Methode ist Flag-gesteuert und kann somit auch mehrfach 
//	hintereinander aufgerufen werden, ohne das es zu Störungen kommt
//
ReturnStatus TdfAccess::startRegisterTimer()
{
  TRACE_FUNCTION("TdfAccess::startRegisterTimer(...)");

  if (!registerTimerFlag)
  {
    // Starten eines "Einmal"-Timers
    if (startTimer(registerTimerId, registerTimer, PcpTimerInterface::once) == isNotOk)
    {
      idaTrackFatal(("Could not start register timer"));
      return isNotOk;
    }
    idaTrackData(("register timer started"));
    registerTimerFlag = true;
  }
  return isOk;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Löscht den evtl. laufenden Register-Timer (auch mehrfach aufrufbar)
//	
ReturnStatus TdfAccess::cancelRegisterTimer()
{
  TRACE_FUNCTION("TdfAccess::cancelRegisterTimer(...)");
  if (registerTimerFlag)
  {
    if (stopTimer(registerTimerId) == isNotOk)
    {
      idaTrackExcept(("Register timer could not be canceled"));
      return isNotOk;
    }
    idaTrackData(("register timer canceled"));
    registerTimerFlag = false;
  }
  return isOk;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Startet einen Timer für die zeitgesteuerte Reaktivierung der Methode "processSendQueue", 
//	welche die Liste der Requests (requestPool) abarbeitet.
//	(mehrfach aufrufbar, nicht nachtriggernd,
//	d.h. durch Mehrfachaufruf verlängert sich die Wartezeit nicht)
//
ReturnStatus TdfAccess::startSendRetryTimer()
{
  TRACE_FUNCTION("TdfAccess::startSendRetryTimer(...)");

  if (!retryTimerFlag)
  {
    if (startTimer(retryTimerId, 500, PcpTimerInterface::once) == isNotOk)
    {
      idaTrackFatal(("Could not start send retry timer for %s", dbIdString ));
      return isNotOk;
    }
    idaTrackData(("send retry timer started"));
    retryTimerFlag = true;
  }
  return isOk;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Inverse Methode zu "startSendRetryTimer()", mit gleichen Eigenschaften
//
ReturnStatus TdfAccess::cancelSendRetryTimer()
{
  if (retryTimerFlag)
  {
    if (stopTimer(retryTimerId) == isNotOk)
    {
      idaTrackExcept(("Send retry timer could not be canceled"));
      return isNotOk;
    }
    idaTrackData(("send retry timer canceled"));
    retryTimerFlag = false;
  }
  return isOk;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Implementierung einer Methode der abstrakten Superklasse "TdfClient".
//
//
Void TdfAccess::statusReportIndication(const TdfStatusReport& arg)
{
  idaTrackTrace (("TdfAccess::statusReportIndication(...)"));

  TdfStatusReport::Status dbStatus = arg.getStatus();
  switch (dbStatus)
  {
  case TdfStatusReport::available :
    idaTrackData(("DB Server available"));
    EVENT(0,iDAMinRepClass,215,countryCode);
    break;
  case TdfStatusReport::notAvailable :
    idaTrackExcept(("DB Server *NOT* available"));
    ALARM(0, iDAMinRepClass, 6, countryCode);
    // Wie im Handbuch beschrieben deregistrieren wir uns
    sendDeRegisterRequest();
    changeStatus(initialized);
    startRegistration();
    break;
  }
}


Void TdfAccess::modifyConfirmation( const TdfResponse & arg )
{
  searchConfirmation( arg );
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Implementierung einer Methode der abstrakten Superklasse "TdfClient".
//
//	Für jeden Request (Suchanfrage) an einen Datenbank-Server (NDIS) wird diese Methode vom
//	OSAAPI-Framework aufgerufen, um die Antwort zurück zu liefern
//
Void TdfAccess::searchConfirmation(const TdfResponse& tdfResponse)
{
  TRACE_FUNCTION("TdfAccess::searchConfirmation(...)");

  PcpTime time;
  idaTrackData(("*** Response received from Gateway   at %d:%d:%d,%d",
    time.getHour(), time.getMinute(), time.getSec(), time.getMilliSec()));	
  idaTrackData(("TdfResponse:\n%s", tdfResponse.dumpString()));

  EVENT(0, iDAMinRepClass, 218, "");



  // -------------------------------------------------------------------------
  // Zuerst muß die Empfänger-OID aus dem zugehörigen Request-Objekt
  // ermittelt werden:
  RequestContainer	requestContainer;
  UShort				requestId	= tdfResponse.getReference();
  // wir holen die Request-Daten aus der Request-Liste
  if (!requestPool.getRequest(requestId, requestContainer))
  {
    // Wenn der Request nicht gefunden wurde ...

    idaTrackExcept(("TdfAccess::searchConfirmation: requestPool.getRequest failed"));

    // Eigentlich müßten wir ein Fehler-Dokument zurück senden.
    // Da wir den Adressaten aber nicht ermitteln konnten,
    // können wir hier nicht tun !

    return;
  }

  TdfArgument*	tmpTdfArg	= requestContainer.getTdfArgument();
  UShort			channel		= requestContainer.getChannel();
  RefId			timerId		= requestContainer.getTimerId();
  PcpTime			birth		= requestContainer.getBirth();



#ifdef MONITORING
  {
    ++dbCounter;
    RelTime delta = PcpTime() - birth;
    ULong dauer = delta.inMilliSeconds();
    cout << dbIdString << "(" << requestId << ")DB searchtime : " << dauer << " [ms]" << endl;
    dbDauer += dauer;
    if (dauer < dbDauerMin) dbDauerMin = dauer;
    if (dauer > dbDauerMax) dbDauerMax = dauer;
    if (dbCounter >= 100)
    {
      cout << dbIdString << "\t\t\t\t\t\tDatabase min/avg/max [ms] : " 
        << dbDauerMin << "/"
        << dbDauer / dbCounter << "/"
        << dbDauerMax
        << endl;
      dbDauerMin = 1000000;
      dbDauerMax = 0;
      dbDauer = 0;
      dbCounter = 0;
    }
  }
#endif

  // --------------------------------------------------------------------------
  // Resourcen freigeben:
  // Timer löschen
  stopTimer(timerId);
  // Channel freigeben
  channelMgr.releaseChannel(channel);
  // TdfArgument-Objekt löschen !
  tmpTdfArg->reset();
  if ( tmpTdfArg )
  {
    delete tmpTdfArg;
    tmpTdfArg = 0;
  }

  // Request aus der Liste löschen
  requestPool.removeRequest(requestId);


  String xmlDocType;							// für den Dokument-Typ Bezeichner
  String responseString;						// für die XML-gewandelten Antwortdaten
  responseString.setReallocBlockSize(0x8000); // verhindert zu haefiges Reallozieren

  // -------------------------------------------------------------------------
  // abhaengig vom DataFormat entscheiden wir, was wir der Payload des
  // TdfResponse-Objektes entnehmen koennen

  TdsResponse	tdsResponse;
  tdsResponse.reset();
  ModifyResponse	modifyResponse;
  modifyResponse.reset();
  char modifyRespOpen [] = "<OsaModifyResponse ";  // an attribute will be added
  char modifyRespClose [] = "</OsaModifyResponse>";
  const char * rootTagOpen = tagoOsaResponse.cString();
  const char * rootTagClose = tagcOsaResponse.cString();

  TdfErrorArg	tdfErrorArg;

  switch (tdfResponse.getDataFormat())
  {
    // ---------------------------------------------------------------------
    // Alles ist gut gegangen, und ein TdsResponse Objekt wurde geliefert
  case tdsDataFormat:
    // 1.: TdsResponse-Objekt holen
    if (tdfResponse.isDataSet())
    {
      if (tdfResponse.getData(tdsResponse) == isNotOk)
      {
        idaTrackFatal(("tdfResponse.getData(tdsResponse) failed"));
        // Die Daten des TdfResponse-Objektes konnten nicht geholt werden
      }
      idaTrackData(("TdsResponse received:\n%s", tdsResponse.dumpString()));
    }
    else
    {
      idaTrackExcept(("tdfResponse.isDataSet() was false"));
      // Das TdfResponse-Objekt enthaelt keine Daten
      return;
    }

    // TdsResponse-Objekt umwandeln
    convTdsResponseToXml(tdsResponse, responseString, xmlDocType);


    break;

    // ---------------------------------------------------------------------
    // Alles ist gut gegangen, und ein ModifyResponse Objekt wurde geliefert
  case tdsModifyDataFormat:
    rootTagOpen = modifyRespOpen;
    rootTagClose = modifyRespClose;

    if (tdfResponse.isDataSet())
    {
      if (tdfResponse.getData(modifyResponse) == isNotOk)
      {
        idaTrackFatal(("tdfResponse.getData(modifyResponse) failed"));
        // Die Daten des TdfResponse-Objektes konnten nicht geholt werden
      }
      idaTrackData(("ModifyResponse received:\n%s", modifyResponse.dumpString()));
    }
    else
    {
      idaTrackExcept(("modifyResponse.isDataSet() was false"));
      // Das ModifyResponse-Objekt enthaelt keine Daten
      return;
    }

    // ModifyResponse-Objekt umwandeln
    convModifyResponseToXml(modifyResponse, responseString, xmlDocType);

    break;

    // ---------------------------------------------------------------------
    // Bei der Abfrage ist ein TDF-Fehler aufgetreten
  case errorArgDataFormat:
    tdfResponse.getData(tdfErrorArg);
    idaTrackData(("TdfErrorArg received:\n%s", tdfErrorArg.dumpString()));
    handleErrorArg(tdfErrorArg);

    // kein break !

    // ---------------------------------------------------------------------
  default:
    // Protokoll-Ausgabe

    // Fehlerbeschreibung generieren (in XML)
    convTdfErrorArgToXml(tdfErrorArg, responseString, xmlDocType);

    break;

  }

  // -------------------------------------------------------------------------
  // Das Ergebnis der NDIS-Abfrage wird in einen XML-String gewandelt
  // Der String wird in folgender Variablen aufgebaut:
  String xmlString;
  xmlString.setReallocBlockSize(0x8000); // verhindert zu haefiges Reallozieren

  // XML-Dokument beginnen
  // Zeichensatz Encoding / Version usw. angeben

  xmlString.cat(tagHEADER);

  xmlString.cat(rootTagOpen);		//   '<OsaResponse'
  xmlString.cat(" type=\"");			// + ' type="'
  xmlString.cat(xmlDocType);			// + 'residential'		(z.B.)
  xmlString.cat("\">\n");				// + '">' + Zeilenumbruch


  // TdfDaten einbauen

  if (tdfResponse.isApplicationIdSet())
  {
    XmlHelper::createXmlNode(xmlString, tagApplicationId, (Int)tdfResponse.getApplicationId());
  }

  if (tdfResponse.isSearchRefSet())
  {
    XmlHelper::createXmlNode(xmlString, tagSearchRef, (ULong)tdfResponse.getSearchRef());
  }

  if (tdfResponse.isSourceIdSet())
  {
    XmlHelper::createXmlNode(xmlString, tagSourceId, tdfResponse.getSourceId());
  }

  if (tdfResponse.isLinkContextSet())
  {
    String result;
    String byteString;
    tdfResponse.getLinkContext(byteString);
    dataToHexString(byteString, result);
    XmlHelper::createXmlNode(xmlString, tagTdfLinkContext, result.cString());
  }

  // TdsDaten anhängen
  xmlString.cat(responseString);

  // XML-Dokument abschließen
  xmlString.cat(rootTagClose);

  // -> der XML-String ist fertig

  idaTrackData(("XML-Response:\n%s", xmlString.cString()));



  // -------------------------------------------------------------------------
  // XML-String über das WebInterface senden
  sendResponse(requestId,  xmlString);

#ifdef MONITORING
  {
    ++responseCounter;
    RelTime delta = Time() - timeStamp2;
    ULong dauer = delta.inMilliSeconds();
    cout << dbIdString << "(" << requestId << ")Response      : " << dauer << " [ms]" << endl;
    responseDauer += dauer;
    if (dauer < responseDauerMin) responseDauerMin = dauer;
    if (dauer > responseDauerMax) responseDauerMax = dauer;
    if (responseCounter >= 100)
    {
      cout << dbIdString << "\t\t\t\t\t\tResponse min/avg/max [ms] : " 
        << responseDauerMin << "/"
        << responseDauer / responseCounter << "/"
        << responseDauerMax
        << endl;
      responseDauerMin = 1000000;
      responseDauerMax = 0;
      responseDauer = 0;
      responseCounter = 0;
    }
  }
#endif



  // -------------------------------------------------------------------------
  // Der Response ist bearbeitet und ein ein Channel ist frei geworden
  // Wir können nun versuchen eine Request aus der "waitingList" abzusenden.
  //	processSendQueue();

  return;
}



////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Wenn ein TdfErrorArg vom Server geschickt wird müssen wir untersuchen, ob
//	z. B. die Registrierung wiederholt werden muß
// 
Void TdfAccess::handleErrorArg(TdfErrorArg tdfErrorArg)
{
  TRACE_FUNCTION("TdfAccess::handleErrorArg(...)");

  if (tdfErrorArg.isErrorClassSet())
  {

    VisibleString vsSource;
    VisibleString vsClass;
    enumToString(tdfErrorArg.getErrorSource(), vsSource);
    enumToString(tdfErrorArg.getErrorClass(), vsClass);

    idaTrackData(("TdfErrorArg: ErrorSource = %s, ErrorClass = %s", vsSource.dumpString(), vsClass.dumpString()));

    switch (tdfErrorArg.getErrorClass())
    {
    case TdfErrorArg::undetailedError:
      // ...
      break;
    case TdfErrorArg::protocol :
      // ...
      break;
    case TdfErrorArg::connection :
      // ...
      break;
    case TdfErrorArg::systemCallFailed :
      // ...
      break;
    case TdfErrorArg::tableOverflow :
      // ...
      break;
    case TdfErrorArg::noSuchConverterAvailable :
      // ...
      break;
    case TdfErrorArg::applicationIdUnknown :
      // ...
      changeStatus(initialized);
      startRegistration();
      break;
    case TdfErrorArg::registrationFailure :
      // ...
      changeStatus(initialized);
      startRegistration();
      break;
    case TdfErrorArg::corruptData :
      // ...
      break;
    case TdfErrorArg::shutdownRequested :
      // ...
      break;
    case TdfErrorArg::timeout :
      // ...
      break;
    case TdfErrorArg::serverInDryMode :
      // ...
      break;
    case TdfErrorArg::deRegisterForcedByAdministration :
      // ...
      break;
    case TdfErrorArg::permissionDenied :
      // ...
      break;
    case TdfErrorArg::congestion :
      // ...
      break;
    case TdfErrorArg::permanentOutOfChannels :
      // ...
      break;
    case TdfErrorArg::functionNotSupported :
      // ...
      break;
    case TdfErrorArg::contextNotAvailable :
      // ...
      break;
    case TdfErrorArg::compatibilityError :
      // ...
      break;
    case TdfErrorArg::ticketValidationFailed :
      // ...
      break;
    }
  }
}




////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Diese Methode behandelt einen Request gleich welcher Art (Login, Logout, Such-Request, ...) der
//	von einer WebConnection kommt. Ihre Aufgabe ist, ein RequestContainer-Objekt anzulegen und
//	mit allen benötigten Informationen zu füllen um den den Request "sende-fertig" in die Sende-Liste
//	(= requestPool) einzutragen. Dazu gehört auch die Wandlung vom XML- in das proprietäre TDS-Format.
//	Das eigentliche Senden (bzw. zuvor Authentifizieren) übernimmt dann die Methode processSendQueue.
//
Void TdfAccess::handleRequest(const UShort requestId, const String& xmlString)
{
  TRACE_FUNCTION("TdfAccess::handleRequest(...)");

  {
    PcpTime time;
    idaTrackData(("*** Request from WebConnection received at %d:%d:%d,%d",
      time.getHour(), time.getMinute(), time.getSec(), time.getMilliSec()));
  }

  EVENT(0, iDAMinRepClass, 205, "");

#ifdef MONITORING
  timeStamp1 = PcpTime();
#endif

  // ---------------------------------------------------------------------
  // Zum Speichern des gesamten Request-Kontextes legen wir ein
  // RequestContainer-Objekt an :
  RequestContainer requestContainer(requestId);


  idaTrackData(("XML-Request:\n%s", xmlString.cString()));

  ClXMLParser * parser = NULL;
  ClDocument * doc = NULL;
  ClXMLSource * xmlSource = NULL;
  ClNode * rootNode = NULL;

  idaTrackTrace(("TdfAccess::handleRequest after declarations"));



  // ------------------------------------------------------------------------
  // Aus der payload versuchen wir ein DOM-Dokument aufzubauen
  PcpErrorArg err;
  if ( PCPXML::createDOMFromString ( xmlString, xmlSource, parser, doc, rootNode, err, "default" ) == isNotOk )
  {
    idaTrackExcept (( "cannot parse xml document to DOM" ));
    String errorText = err.getInfo ( NULL, NULL );
    idaTrackExcept(( errorText.cString () ));

    // The request cannot be processed and is rejected
    refuseRequest(requestContainer, "parserError", errorText);
    return;
  }

  idaTrackTrace(("TdfAccess::handleRequest after document creation"));

  // -------------------------------------------------------------------------
  // Wir holen den Wert für den timeout aus dem DOM und tragen diesen
  // in den RequestContainer ein.
  ULong timeout = searchTimeOut;			// default aus der Konfigurationsdatei
  getTimeoutFromDom(rootNode, timeout);		// Wenn dem Request ein eigener Timeout-Wert mit gegeben
  // wurde, wird der Default-Wert in "timeout" hier
  // überschrieben
  requestContainer.setTimeout(timeout);	// ... und den Wert im Container speichern
  idaTrackData(("timeout = %d [ms]", timeout));

  // -------------------------------------------------------------------------
  // Wir starten den Timer. Dieser bestimmt den Zeitpunkt, wann der Request
  // unabhängig davon, wie weit er bearbeitet wurde, zurückgewiesen wird
  RefId timerId = 0;
  if (startTimer(timerId, timeout, PcpTimerInterface::once) == isNotOk)
  {
    idaTrackFatal(("startTimer failed"));
    refuseRequest(requestContainer, "internalError", "startTimer failed");
    return;
  }
  requestContainer.setTimerId(timerId);

  // -------------------------------------------------------------------------
  requestContainer.setStatus(RequestContainer::readyToSend);

  // -------------------------------------------------------------------------
  // Als nächstes muß festgestellt werden um welche Art von Request es sich handelt.
  // Möglichkeiten sind: Login, Logout, ChangPwd und Request
  // Der Typ wird über das Wurzel-Element bestimmt
  bool isModifyRequest = false;
  String requestType;

  PCPXML:: getNodeName(rootNode, requestType);
  // Je nach Type muß anders vorgegangen werden
  if (requestType == String("Login"))
  {
    idaTrackData(("request type: Login"));
    requestContainer.setMode(RequestContainer::login);
    requestPool.addRequest(requestContainer);
    processSendQueue();
    return;
  }
  else if (requestType == String("Logout"))
  {
    idaTrackData(("request type: Logout"));
    requestContainer.setMode(RequestContainer::logout);
    requestPool.addRequest(requestContainer);
    processSendQueue();
    return;
  }
  else if (requestType == String("ChangePwd"))
  {
    idaTrackData(("request type: ChangePwd"));
    requestContainer.setMode(RequestContainer::changePwd);
    requestPool.addRequest(requestContainer);
    processSendQueue();
    return;
  }
  else if (requestType == String("Request"))
  {
    idaTrackData(("request type: Request"));
    requestContainer.setMode(RequestContainer::request);
  }
  else if (requestType == String("ModifyRequest"))
  {
    idaTrackData(("request type: ModifyRequest"));
    requestContainer.setMode(RequestContainer::request);
    isModifyRequest = true;
  }
  else
  {
    // Wenn dieser Fall ein tritt, liegt ein Fehler vor
    idaTrackExcept(("Unknown request type: %s",&requestType));
    refuseRequest(requestContainer, "formatError", "unknown request type (see XML root element)");
    return;
  }



  // -------------------------------------------------------------------------
  // Wir prüfen die Bereitschaft.
  // Wenn wir nicht registriert sind, nehmen wir keine Requests an
  if (getStatus() != registered)
  {
    if (getStatus() != tryingToRegister || registerTimerFlag == false)
    {
      startRegistration();
    }
    switch (getStatus())
    {
    case initializationFailed :
      idaTrackExcept(("Initialization error --> request refused"));
      refuseRequest(requestContainer, "notReady", "Initialization error");
      if ( PCPXML::deleteDOM (  xmlSource, parser, doc, rootNode, err ) == isNotOk )
      {
        // Ein Fehler ist beim Aufräumen der XML Platform aufgetreten
        String errorText("\nError during XML Parser cleanup:\n");
        errorText.cat ( err. getInfo ( NULL, NULL ) );
        idaTrackExcept(( errorText.cString () ));
      }
      return;
    case unAvailable :
      idaTrackExcept(("System is not available --> request refused"));
      refuseRequest(requestContainer, "unAvailable", "System is not available");
      if ( PCPXML::deleteDOM (  xmlSource, parser, doc, rootNode, err ) == isNotOk )
      {
        // Ein Fehler ist beim Aufräumen der XML Platform aufgetreten
        String errorText("\nError during XML Parser cleanup:\n");
        errorText.cat ( err. getInfo ( NULL, NULL ) );
        idaTrackExcept(( errorText.cString () ));
      }
      return;
    case tryingToRegister :
      idaTrackExcept(("System is trying to register at DB --> request refused"));
      refuseRequest(requestContainer, "tryingToRegister", "System is trying to register at DB");
      if ( PCPXML::deleteDOM (  xmlSource, parser, doc, rootNode, err ) == isNotOk )
      {
        // Ein Fehler ist beim Aufräumen der XML Platform aufgetreten
        String errorText("\nError during XML Parser cleanup:\n");
        errorText.cat ( err. getInfo ( NULL, NULL ) );
        idaTrackExcept(( errorText.cString () ));
      }
      return;
    }
  }


  // -------------------------------------------------------------------------
  // DOM traversieren und dabei ein TdsRequest-Objekt aufbauen
  TdsRequest tdsRequest;
  ModifyRequest modifyRequest;
  ReturnStatus result;

  String errorText;
  if ( isModifyRequest )
  {
    result = createModifyRequestFromDom(modifyRequest, rootNode, errorText);
  }
  else
  {
    result = createTdsRequestFromDom(tdsRequest, rootNode, errorText);
  }

  if (result != isOk)
  {
    idaTrackExcept(("Could not create TdsRequest/ModifyRequest from DOM: %s", errorText.cString() ));
    // Fehler beim Aufbau des TdsRequest-Objektes
    // Also wieder ein Fehler-Dokument senden
    refuseRequest(requestContainer, "requestCreationError", errorText);

    if ( PCPXML::deleteDOM (  xmlSource, parser, doc, rootNode, err ) == isNotOk )
    {
      // Ein Fehler ist beim Aufräumen der XML Platform aufgetreten
      errorText.cat("\nError during XML Parser cleanup:\n");
      errorText.cat ( err. getInfo ( NULL, NULL ) );
      idaTrackExcept(( errorText.cString () ));
    }

    return;
  }

  if ( isModifyRequest )
  {
    idaTrackData(("ModifyRequest:\n%s", modifyRequest.dumpString()));
  }
  else
  {
    idaTrackData(("TdsRequest:\n%s", tdsRequest.dumpString()));
  }


  // -------------------------------------------------------------------------
  // TdfArgument anlegen und im RequestContainer ablegen
  TdfArgument* tdfArgument = new TdfArgument;

  if ( isModifyRequest )
  {
    result = createTdfArgument(tdfArgument, modifyRequest, rootNode, requestId, errorText);
  }
  else
  {
    result = createTdfArgument(tdfArgument, tdsRequest, rootNode, requestId, errorText);
  }

  if (result == isNotOk)
  {
    idaTrackExcept(("Could not create TdfArgument-Object"));
    refuseRequest(requestContainer, "requestCreationError", errorText);
    if ( PCPXML::deleteDOM (  xmlSource, parser, doc, rootNode, err ) == isNotOk )
    {
      // Ein Fehler ist beim Aufräumen der XML Platform aufgetreten
      errorText.cat("\nError during XML Parser cleanup:\n");
      errorText.cat ( err. getInfo ( NULL, NULL ) );
      idaTrackExcept(( errorText.cString () ));
    }

    if ( tdfArgument )
    {
      delete tdfArgument;
      tdfArgument = 0;
    }

    return;
  }
  requestContainer.setTdfArgument(tdfArgument);

  idaTrackData(("TdfArgument:\n%s", tdfArgument->dumpString()));

  //Gibt das Dokument und alle damit referenzierten DOM-Objekte frei

  if ( PCPXML::deleteDOM (  xmlSource, parser, doc, rootNode, err ) == isNotOk )
  {
    // Ein Fehler ist beim Aufräumen der XML Platform aufgetreten
    errorText.cat("\nError during XML Parser cleanup:\n");
    errorText.cat ( err. getInfo ( NULL, NULL ) );
    idaTrackExcept(( errorText.cString () ));
  }

  // -------------------------------------------------------------------------
  // The request now should be ready to send 
  // add the RequestContainer to the sendqueue
  requestPool.addRequest(requestContainer);

  // -------------------------------------------------------------------------
  // initiate send
  processSendQueue();

#ifdef MONITORING
  {
    ++requestCounter;
    RelTime delta = PcpTime() - timeStamp1;
    ULong dauer = delta.inMilliSeconds();
    cout << dbIdString << "(" << requestId << ")Request       : " << dauer << " [ms]" << endl;
    requestDauer += dauer;
    if (dauer < requestDauerMin) requestDauerMin = dauer;
    if (dauer > requestDauerMax) requestDauerMax = dauer;
    if (requestCounter >= 100)
    {
      cout << dbIdString << "\t\t\t\t\t\tRequest  min/avg/max [ms] : " 
        << requestDauerMin << "/"
        << requestDauer / requestCounter << "/"
        << requestDauerMax
        << endl;
      requestDauerMin = 1000000;
      requestDauerMax = 0;
      requestDauer = 0;
      requestCounter = 0;
    }
  }
#endif

  return;
}  // end handle request


////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
Void TdfAccess::refuseRequest(RequestContainer& requestContainer,
                              String errorCode,
                              String errorText)
{
  TRACE_FUNCTION("TdfAccess::refuseRequest(...)");

  idaTrackExcept(("request refused: %s", errorCode.cString()));
  PROBLEM(0, iDAMinRepClass, 121, errorCode.cString());

  String xmlString;

  // XML Response-Dokument erzeugen ...
  XmlHelper::formatXMLErrorResponse(xmlString, "tdfAccess", errorCode.cString(), errorText.cString());

  // ... und senden
  sendResponse(requestContainer.getRequestId(), xmlString);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
Void TdfAccess::sendAuthenticationAcknowledge(RequestContainer& requestContainer)
{
  TRACE_FUNCTION("TdfAccess::sendAuthenticationAcknowledge(...)");

  idaTrackData((""));
  EVENT(0, iDAMinRepClass, 236, "");


  String xmlString;

  // XML Response-Dokument erzeugen ...
  xmlString.cat(tagHEADER);
  xmlString.cat(tagoOsaResponse);

  switch (requestContainer.getMode())
  {
  case RequestContainer::login :		xmlString.cat(" type=\"loginAcknowledge\">\n");     break;
  case RequestContainer::logout :		xmlString.cat(" type=\"logoutAcknowledge\">\n");	break;
  case RequestContainer::changePwd :	xmlString.cat(" type=\"changePwdAcknowledge\">\n");	break;
  }

  xmlString.cat(tagcOsaResponse);

  idaTrackData(("sending SES autentication aknowledge:\n%s", xmlString.cString()));

  // ... und senden
  sendResponse(requestContainer.getRequestId(), xmlString);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Implementierung einer Methode der abstrakten Superklasse "PcpTimerInterface".
//
//	Alle Timer-Events lösen einen Aufruf dieser Methode aus.
//	
Void TdfAccess::timerBox(const RefId id)
{
  TRACE_FUNCTION("TdfAccess::timerBox(...)");

  UShort requestId;

  // -------------------------------------------------------------------------
  // Wenn der RegistrierungsTimer zugeschlagen hat ...
  if (id == registerTimerId)
  {
    // Da registerTimerFlag immer anzeigen soll, ob der Timer aktiv ist
    // oder nicht, muß das Flag hier gelöscht werden
    registerTimerFlag = false;

    idaTrackExcept(("Handling register timeout"));
    PROBLEM(0, iDAMinRepClass, 109, countryCode);

    // Wiederholung der Registrierung
    sendRegisterRequest();

    return;
  }
  // -------------------------------------------------------------------------
  // Wenn der SendRetry Timer zugeschlagen hat ...
  if (id == retryTimerId)
  {
    retryTimerFlag = false;

    idaTrackExcept(("send retry timer timeout"));

    // Es wird Zeit zu versuchen evtl. Request erneut zu senden, die zuvor nicht
    // gesendet werden konnten
    processSendQueue();	

    return;
  }
  // -------------------------------------------------------------------------
  // Timeout eines Requestes
  if (requestPool.findTimerId(id, requestId))
  {
    idaTrackExcept(("Request timeout, item is removed from sendqueue"));
    PROBLEM(0, iDAMinRepClass, 110, "");

    // wir holen die Request-Daten aus der Request-Liste
    RequestContainer requestContainer;
    requestPool.getRequest(requestId, requestContainer);

    // Die DB bekommt eine cancleRequest Nachricht
    if (requestContainer.getStatus() == RequestContainer::sendAndWaiting)
    {
      idaTrackTrace (( "sending cancelSearchRequest()" ));
      TdfCancelSearchArg tdfCancelSearchArg;
      tdfCancelSearchArg.setApplicationId(requestContainer.getTdfArgument()->getApplicationId());
      tdfCancelSearchArg.setSearchRef(requestContainer.getTdfArgument()->getSearchRef());
      tdfCancelSearchArg.setSourceId(requestContainer.getTdfArgument()->getSourceId());
      ReturnStatus res = cancelSearchRequest(tdfCancelSearchArg);

      if (res == isOk)
      {
        idaTrackTrace (( "cancelSearchRequest() == isOk" ));
      }
      else
      {
        OsaComError error = getLastError();
        idaTrackTrace (( "cancelSearchRequest() failed: %s", error.getErrorText() ));
      }

    }

    // Wir schicken ein Fehler-Dokument
    refuseRequest(requestContainer, "timeout", "request timeout, item is removed from sendqueue");

    // TdfArgument-Objekt löschen !
    TdfArgument* tdfArgument = requestContainer.getTdfArgument();
    if ( tdfArgument )
    {
      delete tdfArgument;
      tdfArgument = 0;
    }

    channelMgr.releaseChannel(requestContainer.getChannel());

    // Request aus der Liste löschen
    requestPool.removeRequest(requestContainer.getRequestId());

    return;
  }

  idaTrackTrace(("Ignoring unrelated timer message"));
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
Void TdfAccess::assembleTdfOriginId(TdfOriginId* orId, const char* clientAddress)
{
  TRACE_FUNCTION("TdfAccess::assembleTdfOriginId(...)");

  orId->setProduct(OsaApiConstants::ida);
  orId->setLocalAddress(localTdfAddress);
  orId->setProtocol(OsaApiConstants::httpProt);

  TdfAddress clientTdfAddress;
  ULong clientIpAddress = resolveClientIp(clientAddress);
  idaTrackData(("clientIpAddress: %d", clientIpAddress));

  if (clientIpAddress)
  {
    clientTdfAddress.setIpAddress(clientIpAddress);
  }
  orId->setRemoteAddress(clientTdfAddress);
  orId->setServiceName(serviceName);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Direkte Wandlung des TdsResponse-Objektes in einen XML-String
//
ReturnStatus TdfAccess::convTdsResponseToXml(const TdsResponse& tdsResponse,
                                             String& xmlString,
                                             String& typeString)
{
  TRACE_FUNCTION("TdfAccess::convTdsResponseToXml(...)");

  VisibleString visibleString;

  typeString = "error";

  Bool errorFlag	= false;		// Zeigt an, ob Fehler gemeldet wurden
  Int  records	= 0;			// Anzahl Records, wir später gebraucht um den typeString richtig zu setzen

  // -----------------------------------------------------------------------------
  // SearchResult
  xmlString.cat(tagoSearchResult);

  const SearchResult& searchResult = tdsResponse.getSearchResult();

  if (searchResult.isResultSpecifierSet())
  {
    enumToString(searchResult.getResultSpecifier(), visibleString);
    XmlHelper::createXmlNode(xmlString, tagResultSpecifier, visibleString.getDataString());	
  }

  if (searchResult.isErrorSourceSet())
  {
    enumToString(searchResult.getErrorSource(), visibleString);
    XmlHelper::createXmlNode(xmlString, tagErrorSource, visibleString.getDataString());
  }

  if (searchResult.isErrorCodeSet())
  {
    errorFlag = true;
    enumToString(searchResult.getErrorCode(), visibleString);
    XmlHelper::createXmlNode(xmlString, tagErrorCode, visibleString.getDataString());
  }


  if (searchResult.isErrorAttributeSet())
  {
    enumToString(searchResult.getErrorAttribute(), visibleString);
    XmlHelper::createXmlNode(xmlString, tagErrorAttribute, visibleString.getDataString());
  }


  if (searchResult.isErrorWordSet())
  {
    XmlHelper::createXmlNode(xmlString, tagErrorWord, (const char*)searchResult.getErrorWord());
  }

  if (searchResult.isErrorTextSet())
  {
    XmlHelper::createXmlNode(xmlString, tagErrorText, (const char*)searchResult.getErrorText());
  }

  if (searchResult.isResultSourceSet())
  {
    visibleString = searchResult.getResultSource();
    XmlHelper::createXmlNode(xmlString, tagResultSource, visibleString.getDataString());
  }


  xmlString.cat(tagcSearchResult);


  // ----------------------------------------------------------------------------
  // SearchSpec(ification)
  if (tdsResponse.isSearchSpecificationSet())	// Das Attribut ist optional
  {
    String result;
    String byteString;


    xmlString.cat(tagoSearchSpec);

    const SearchSpec& searchSpec = tdsResponse.getSearchSpecification();

    if (searchSpec.isOperationSet())
    {
      enumToString(searchSpec.getOperation(), visibleString);
      XmlHelper::createXmlNode(xmlString, tagOperation, visibleString.getDataString());
    }

    if (searchSpec.isLinkContextSet())
    {
      searchSpec.getLinkContext(byteString);
      dataToHexString(byteString, result);
      XmlHelper::createXmlNode(xmlString, tagLinkContext, result.cString());
    }

    if (searchSpec.isUsedCharacterSetSet())
      XmlHelper::createXmlNode(xmlString, tagUsedCharacterSet, (Int)searchSpec.getUsedCharacterSet());

    // VectorOfLinkContext
    if (searchSpec.isVectorOfLinkContextSet())
    {
      xmlString.cat(tagoVectorOfLinkContext);

      const VectorOfData & vectorOfLinkContext = searchSpec.getVectorOfLinkContext();

      for (size_t index = 0; index < vectorOfLinkContext.size(); index++)
      {
        byteString = vectorOfLinkContext[index].getDataString();
        dataToHexString(byteString, result);
        XmlHelper::createXmlNode(xmlString, tagLinkContext, result.cString());
      }

      xmlString.cat(tagcVectorOfLinkContext);
    }

    xmlString.cat(tagcSearchSpec);

  }	// if (tdsResponse.isSearchSpecificationSet())


  // ----------------------------------------------------------------------------
  // RequestedRes(ponse)
  if (tdsResponse.isRequestedResponseSet())	// Das Attribut ist optional
  {
    xmlString.cat(tagoRequestedRes);

    const RequestedRes& requestedRes = tdsResponse.getRequestedResponse();

    if (requestedRes.isRequestedFormatSet())
    {
      enumToString(requestedRes.getRequestedFormat(), visibleString);
      XmlHelper::createXmlNode(xmlString, tagRequestedFormat, visibleString.getDataString());
    }

    if (requestedRes.isLinesSet())
      XmlHelper::createXmlNode(xmlString, tagLines, (Int)requestedRes.getLines());

    if (requestedRes.isColumnsSet())
      XmlHelper::createXmlNode(xmlString, tagColumns, (Int)requestedRes.getColumns());

    if (requestedRes.isMaxRecordsSet())
      XmlHelper::createXmlNode(xmlString, tagMaxRecords, (Int)requestedRes.getMaxRecords());

    if (requestedRes.isOrderingSet())
      XmlHelper::createXmlNode(xmlString, tagOrdering, requestedRes.getOrdering());

    // SearchFilter
    if (requestedRes.isSearchFilterSet())
    {
      xmlString.cat(tagoSetOfAttributeId);

      // leider muessen wir hier das "const" weg-casten, sonst geht's nicht !
      const SetOfAttributeId& setOfAttributeId = requestedRes.getSearchFilter();

      // Und schon wieder muessen wir ueber eine Liste iterieren:
      SetOfAttributeId::const_iterator sIterator = setOfAttributeId.begin();
      while (sIterator != setOfAttributeId.end())
      {
        // hier gehts um Objekte vom Typ "DbAttribute::AttributedId"
        //  - das ist ein enum
        // Die Bezeichnungen entsprechen Feldern der Suchmaske
        // Den Bezeichner in String-Form bekommen wir wie folgt:
        VisibleString visibleString;
        enumToString(*sIterator, visibleString);
        // Und jetzt noch unter dem Tag "AttributeId" eintragen
        XmlHelper::createXmlNode(xmlString, tagAttributeId, visibleString.getDataString());
        sIterator++;
      }

      xmlString.cat(tagcSetOfAttributeId);
    }

    // ExpRecordLines
    if (requestedRes.isExpRecordLinesSet())
      XmlHelper::createXmlNode(xmlString, tagExpRecordLines, (Int)requestedRes.getExpRecordLines());

    // SourceLanguage
    if (requestedRes.isSourceLanguageSet())
      XmlHelper::createXmlNode(xmlString, tagSourceLanguage, (Int)requestedRes.getSourceLanguage());

    xmlString.cat(tagcRequestedRes);

  } // if (tdsResponse.isRequestedResponseSet())


  // ----------------------------------------------------------------------------
  // ResultData
  if (tdsResponse.isResultDataSet())	// Das Attribut ist optional
  {
    xmlString.cat(tagoResultData);

    const ResultData& resultData = tdsResponse.getResultData();

    if (resultData.isFormatIdSet())
    {
      // ***********************************************************
      // ACHTUNG: Hier wird nur "indentedRecordFormat" unterstützt !
      // ***********************************************************

      if (resultData.getFormatId() == ResultData::indentedRecordFormat)
      {
        XmlHelper::createXmlNode(xmlString, tagFormatId, "indentedRecordFormat");
      }
      else
      {
        // Fehlerfall
        idaTrackFatal(("\t*** only indentedRecordFormat is supported ***"));
        return isNotOk;
      }
    }

    if (resultData.isTotalNumberOfRecordsSet())
      XmlHelper::createXmlNode(xmlString, tagTotalNumberOfRecords,
      (Int)resultData.getTotalNumberOfRecords());

    if (resultData.isNumberOfRecordsReturnedSet())
    {
      records = (Int)resultData.getNumberOfRecordsReturned();
      XmlHelper::createXmlNode(xmlString, tagNumberOfRecordsReturned,
        (Int)resultData.getNumberOfRecordsReturned());
    }

    if (resultData.isTotalNumberOfMarkedRecordsSet())
      XmlHelper::createXmlNode(xmlString, tagTotalNumberOfMarkedRecords,
      (Int)resultData.getTotalNumberOfMarkedRecords());

    if (resultData.isNumberOfMarkedRecordsReturnedSet())
      XmlHelper::createXmlNode(xmlString, tagNumberOfMarkedRecordsReturned,
      (Int)resultData.getNumberOfMarkedRecordsReturned());

    if (resultData.isDataTypeSet())
    {
      VisibleString visibleString;
      enumToString(resultData.getDataType(), visibleString);
      XmlHelper::createXmlNode(xmlString, tagDataType, visibleString.getDataString());

      // Wir entnehmen hier den Typ:
      typeString = visibleString.getDataString();
    }

    if (resultData.isIndentedRecordsSet())
    {
      xmlString.cat(tagoListOfRecords);

      // und wieder mal casten wir das "const" weg !
      const VectorOfIndentRecord & vectorOfIndentRecord = resultData.getIndentedRecords();

      // -----------------------------------------------------------------
      // An dieser Stelle wird die implizit per IndentLevel gegebene
      // Hierarchie der Records in eine explizite XML-Struktur gewandelt
      // D.h. die Records kommen nicht mehr einfach hintereinander, sondern
      // sind ihrem IndentLevel entsprechend baumartig verknüpft
      //
      // Bei der Iteration über die Records wird dazu immer der Pegel
      // an ausstehenden "schließenden XML-Tags" verwaltet
      // Kommt 
      int actLevel = 0;
      int pendingTags = 0;
      for (size_t index = 0; index < vectorOfIndentRecord.size(); index++)
      {
        const IndentRecord& indentRecord = vectorOfIndentRecord[index];
        // IndentLevel ermitteln
        UChar indentLevel = 0;
        if (indentRecord.isIndentLevelSet())
        {
          indentLevel = indentRecord.getIndentLevel();
        }

        // Bei Folgeanfragen (Blättern) kann es vorkommen, dass der Record
        // nicht den Level 0 hat. In diesem Fall müssen zusätzliche 
        // öffnende Tags eingebaut werden.
        if (index == 0)
        {
          for (int i = 0; i < indentLevel; i++)
          {
            xmlString.cat(tagoRecord);
            ++pendingTags;
          }
        }

        // Wenn noch ausstehende schliessende Tags fehlen ...
        if (pendingTags)
        {
          if (indentLevel <= actLevel)
          {
            for (int x = 0; x < actLevel-indentLevel+1; x++)
            {
              xmlString.cat(tagcRecord);
              --pendingTags;
            }
          }
        }

        // Öffnendes Tag
        xmlString.cat(tagoRecord);
        ++pendingTags;

        convIndentRecordToXml(xmlString,
          indentRecord);

        actLevel = indentLevel;
      }

      // Wenn jetzt noch schiessende Tags offen sind ...
      while (pendingTags > 0)
      {
        xmlString.cat(tagcRecord);
        --pendingTags;
      }

      // -----------------------------------------------------------------

      xmlString.cat(tagcListOfRecords);
    }


    // QstScreenData        NOT SUPPORTED !!!


    // UsedCharacterSet
    if (resultData.isUsedCharacterSetSet())
    {
      VisibleString visibleString;
      enumToString((OsaApiConstants::CharacterSet)resultData.getUsedCharacterSet(),
        visibleString);
      XmlHelper::createXmlNode(xmlString, tagUsedCharacterSet, visibleString.getDataString());
    }

    xmlString.cat(tagcResultData);
  }

  // ---------------------------------------------------------------------------------------------
  // Evtl. muß der typeString noch korrigiert werden:
  // Wenn keine Records geliefert wurden, eine Fehler sowie der RekordType gesetzt wurden,
  // dann soll "error" zurück gegeben werden !
  if ((records == 0) && errorFlag)
  {
    typeString = "error";
  }

  return isOk;
}



////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Die "indented" Records werden in dieser Methode in das XML-Format umgewandelt und an den
//	übergebenen "xmlString" angehängt
//
ReturnStatus TdfAccess::convIndentRecordToXml(String& xmlString,
                                              const IndentRecord& indentRecord)
{
  TRACE_FUNCTION("TdfAccess::convIndentRecordToXml(...)");

  VisibleString visibleString;

  // IndenLevel Tag
  if (indentRecord.isIndentLevelSet())
    XmlHelper::createXmlNode(xmlString, tagIndentLevel, (const Int)indentRecord.getIndentLevel());

  // Flags
  if (indentRecord.isFlagsSet())
  {
    ULong flags = indentRecord.getFlags();
    for (int i = 0; i < 32; i++)
    {  // added first bit! NewRecordFlag was missing!
      ULong bit = (ULong(1) << i) & flags;
      if (bit)
      {
        enumToString((IndentRecord::Flags)bit, visibleString);
        XmlHelper::createXmlNode(xmlString, tagFlag, visibleString.getDataString());
      }
    }
  }

  // OldRecordType
  if (indentRecord.isOldRecordTypeSet())
  {
    enumToString(indentRecord.getOldRecordType(), visibleString);
    XmlHelper::createXmlNode(xmlString, tagFlag, visibleString.getDataString());
  }

  // LinkContext
  if (indentRecord.isLinkContextSet())
  {
    String byteString;
    String result;
    indentRecord.getLinkContext(byteString);
    dataToHexString(byteString, result);
    XmlHelper::createXmlNode(xmlString, tagLinkContext, result.cString());
  }

  // isRecordTypeSet
  if (indentRecord.isRecordTypeSet())
  {
    XmlHelper::createXmlNode(xmlString, tagRecordType, (Int)indentRecord.getRecordType());
  }

  // RecordSource
  if (indentRecord.isRecordSourceSet())
  {
    XmlHelper::createXmlNode(xmlString, tagRecordSource, indentRecord.getRecordSource().getDataString());
  }

  // RecordId
  if (indentRecord.isRecordIdSet())
  {
    visibleString = indentRecord.getRecordId();
    XmlHelper::createXmlNode(xmlString, tagRecordId, visibleString.getDataString());
  }

  // RecordVersion
  if (indentRecord.isRecordVersionSet())
  {
    visibleString = indentRecord.getRecordVersion();
    XmlHelper::createXmlNode(xmlString, tagRecordVersion, visibleString.getDataString());
  }

  // VectorOfFormatInfo
  if (indentRecord.isVectorOfFormatInfoSet())
  {
    const VectorOfFormatInfo& vectorOfFormatInfo = indentRecord.getVectorOfFormatInfo();
    FormatInfo formatInfo;
    for (UInt i = 0; i < vectorOfFormatInfo.size(); i++)
    {
      formatInfo = vectorOfFormatInfo[i];

      // Flags
      if (formatInfo.isFlagsSet())
      {
        // hier haben wir den Aufwand etwas reduziert !!!
        if (formatInfo.getFlags() & FormatInfo::column)
        {
          XmlHelper::createXmlNode(xmlString, tagFlag, "column");
        }
        if (formatInfo.getFlags() & FormatInfo::fixed)
        {
          XmlHelper::createXmlNode(xmlString, tagFlag, "fixed");
        }
      }

      // Width
      if (formatInfo.isWidthSet())
      {
        XmlHelper::createXmlNode(xmlString, tagWidth, formatInfo.getWidth());
      }
    }
  }


  // Datenbankfelder
  if (indentRecord.isSetOfDbAttributeSet())
  {
    const SetOfDbAttribute& setOfDbAttribute = indentRecord.getSetOfDbAttribute();

    // Iteration über die DbAttribute Objekte
    SetOfDbAttribute::const_iterator sIterator = setOfDbAttribute.begin();
    while (sIterator != setOfDbAttribute.end())
    {
      const DbAttribute& dbAttribute = *sIterator;

      // AttributeId
      if (dbAttribute.isAttributeIdSet() && dbAttribute.isDataSet())
      {
        enumToString(dbAttribute.getAttributeId(), visibleString);
        XmlHelper::createXmlNode(
          xmlString,
          visibleString.getDataString(),
          dbAttribute.getData().getDataString());
      }

      sIterator++;
    }

  }

  return isOk;
}



////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Im Falle eines Tdf-Fehlers wird von der OSA-API ein TdfErrorArg Objekt geliefert. Dieses wird
//	hier in ein XML-Dokument umgewandelt, das einer Teilstruktur entspricht, die aus dem normalen
//	Suchergebnis erzeugt wird.
//
ReturnStatus TdfAccess::convTdfErrorArgToXml(const TdfErrorArg& tdfErrorArg,
                                             String& xmlString,
                                             String& typeString)
{
  TRACE_FUNCTION("TdfAccess::convTdfErrorArgToXml(...)");

  typeString = "error";

  xmlString.cat(tagoSearchResult);	// öffnender Tag

  // ---------------------------------------------------------------

  VisibleString vString;

  if (tdfErrorArg.isErrorSourceSet())
  {
    enumToString(tdfErrorArg.getErrorSource(), vString);
    XmlHelper::createXmlNode(xmlString, tagErrorSource, vString.getDataString());
  }

  if (tdfErrorArg.isErrorClassSet())
  {
    enumToString(tdfErrorArg.getErrorClass(), vString);
    XmlHelper::createXmlNode(xmlString, tagErrorCode, vString.getDataString());
    idaTrackTrace (( "Error Class: %s", vString.getDataString() ));
  }

  if (tdfErrorArg.isErrorTextSet())
  {
    XmlHelper::createXmlNode(xmlString, tagErrorText, tdfErrorArg.getErrorText());
  }

  if (tdfErrorArg.isErrorDetailSet())
  {
    XmlHelper::createXmlNode(xmlString, tagDetail, (Int)tdfErrorArg.getErrorDetail());
  }

  // ---------------------------------------------------------------

  xmlString.cat(tagcSearchResult);	// schließender Tag

  return isOk;
}



////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Die vorliegende Methode zerlegt einen grossen String in kleine
//  Teile und sendet diese ueber die Objekt-Kommunikation der CLASSLIB
//
ReturnStatus TdfAccess::sendResponse(UShort requestId,	// Message Referenznummer
                                     const String& string)	 	// zu sendende Daten
{
  TRACE_FUNCTION("TdfAccess::sendResponse(...)");
  return _webInterface.sendResponse(requestId, string);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Umwandlung einer Binär-Zeichnkette in einen Hex-String damit dieser problemloser in ein
//	XML-Dokument integriert werden kann
//
Void TdfAccess::dataToHexString(String byteSequence, String& result)
{
  TRACE_FUNCTION("TdfAccess::dataToHexString(...)");
  //idaTrackTrace(("TdfAccess::dataToHexString(...)"));

// DE_MR_5606, cp, 2010-08-04
// increase buffer length
  const Uint buflen = 4096;
  char buf[buflen];

  idaTrackTrace(("TdfAccess: byteSequence.len() = %d", byteSequence.len()));

  Uint pos=0;
  for (UInt h = 0; h < byteSequence.len(); h++)
  {
    sprintf(buf+pos, "%02X", (unsigned int) (unsigned char) byteSequence[h]);
    pos = pos + 2;

    if( pos == buflen )
    {
      idaTrackTrace(("TdfAccess: pos=buflen= %d", pos));
      buf[pos] = '\0';
      result += String(buf);
      pos = 0;
    }
  }
 
  idaTrackTrace(("TdfAccess: pos=%d", pos));
  buf[pos] = '\0';
  result += String(buf);

  idaTrackTrace(("TdfAccess: buf = %s", buf));
  idaTrackTrace(("TdfAccess: result = %s", result.cString()));
}



ReturnStatus TdfAccess::createModifyRequestFromDom(ModifyRequest& modifyRequest,
                                                   ClNode * rootNode,
                                                   String& errorText)
{
  TRACE_FUNCTION("TdfAccess::createModifyRequestFromDom(...)");
  idaTrackTrace (( "TdsRequest for %s", dbIdString ));

  errorText = "error during creation of TdsRequest";

  if( rootNode ==null )
  {
    errorText = " no document root element found";
    return isNotOk;
  }

  ClNode * childNode = PCPXML:: getFirstChild ( rootNode );
  ClNode * nextSiblingNode = NULL;

  ReturnStatus result = isOk;

  // -------------------------------------------------------------------------
  // Iteration ueber alle Kinder des Root-Knoten
  // der TdfLinkContext steht im TdfArgument
  while ( childNode )
  {
    if ( PCPXML:: getNodeType ( childNode ) == ClElementNode )
    {
      String childNodeName;
      PCPXML::getNodeName ( childNode, childNodeName );
      if ( childNodeName == String ( "ModifySpecification" ) )
      {
        // ... entsprechende Unterfunktion aufrufen
        result = createModifySpecification(modifyRequest, childNode, errorText);
        if (result == isNotOk) break;
      }
      if ( childNodeName == String ( "ModifyAttributes" ) )
      {
        // ... entsprechende Unterfunktion aufrufen
        result = createModifyAttributes (modifyRequest, childNode, errorText);
        if (result == isNotOk) break;
      }
    }

    nextSiblingNode = PCPXML:: getNextSibling( childNode );
    PCPXML::deleteNode (childNode);
    childNode = nextSiblingNode;
  }


  if ( result == isOk )
  {  
    // ... entsprechende Unterfunktion aufrufen
    result = createModifyFlags(modifyRequest, rootNode, errorText);
  }

  if (result == isNotOk)
  {
    idaTrackExcept(("Error creating TdsRequest-Object from ModifyRequest!"));

    if ( errorText.len() <= 1 )
    {
      errorText.assign("error creating modify request");
    }
  }

  // indentLevel and recordType are ignored by NDIS for modify requests
  modifyRequest.setIndentLevel( 0 );

  return result;
}



////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Diese Methode füllt aus den Daten, übergeben als DOM-Struktur, ein TdsRequest-Objekt.
//
//
ReturnStatus TdfAccess::createTdsRequestFromDom(TdsRequest& tdsRequest,
                                                ClNode * rootNode,
                                                String& errorText)
{
  idaTrackTrace (( "Create TdsRequest from dom for %s", dbIdString ));

  errorText = "error during creation of TdsRequest";

  ClNode * childNode = PCPXML:: getFirstChild ( rootNode );
  ClNode * nextSiblingNode = NULL;
  ReturnStatus result = isOk;

  // -------------------------------------------------------------------------
  // Iteration über alle Kinder des Root-Knoten
  while ( childNode )
  {
    // Knoten-Typ prüfen
    if ( PCPXML:: getNodeType ( childNode ) == ClElementNode )
    {
      // Namen des Knoten holen
      String childNodeName;
      PCPXML:: getNodeName ( childNode, childNodeName );

      // SearchSpecification:
      if ( childNodeName == "SearchSpecification")
      {
        // ... entsprechende Unterfunktion aufrufen
        result = createSearchSpecification(tdsRequest, childNode, errorText);
        if (result == isNotOk) break;
      }

      // SearchVariation:
      if (childNodeName == "SearchVariation")
      {
        // ... entsprechende Unterfunktion aufrufen
        result = createSearchVariation(tdsRequest, childNode, errorText);
        if (result == isNotOk) break;
      }

      // RequestedResponse:
      if (childNodeName == "RequestedResponse")
      {
        // ... entsprechende Unterfunktion aufrufen
        result = createRequestedResponse(tdsRequest, childNode, errorText);
        if (result == isNotOk) break;
      }

      // SearchAttributes:
      if (childNodeName == "SearchAttributes")
      {
        // ... entsprechende Unterfunktion aufrufen
        result = createSearchAttributes(tdsRequest, childNode, errorText);
        if (result == isNotOk) break;
      }
    }
    nextSiblingNode = PCPXML:: getNextSibling( childNode );
    PCPXML::deleteNode (childNode);
    childNode = nextSiblingNode;
  }

  if (result == isNotOk)
  {
    idaTrackExcept(("Error creating TdsRequest-Object !"));
  }

  return result;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	SearchSpec erzeugen:
//
//	Ein Validierung wird dem Parser beim Einlesen des XML-Strings überlassen !
//
ReturnStatus TdfAccess::createSearchSpecification ( TdsRequest& tdsRequest, ClNode * node, String & errorText )
{
  TRACE_FUNCTION("TdfAccess::createSearchSpecification(...)");

  ReturnStatus result = isOk;
  SearchSpec searchSpec;

  ClNode * childNode;
  ClNode * nextSiblingNode = NULL;

  ULong enumVal;


  errorText = "XML:Request/SearchSpecification: missing parameter";

  childNode = PCPXML:: getFirstChild ( node );

  if ( childNode == NULL ) return result;

  while ( childNode )
  {
    String childNodeName;
    String childNodeValue;

    PCPXML:: getNodeName ( childNode, childNodeName );


    // -------------------------------------------------------------------------
    // SearchSpecification/Operation:
    if (childNodeName == "Operation")
    {
      if (getValueOfDomNode(childNodeValue, childNode ) == isOk)
      {
        if (StringToEnum::enumOfOperation(enumVal, childNodeValue.cString()) == isOk)
        {
          idaTrackTrace (( "xmlnode: %s with value: %s", childNodeName.cString (), childNodeValue.cString () ));
          searchSpec.setOperation((SearchSpec::Operation)enumVal);
          nextSiblingNode = PCPXML:: getNextSibling( childNode );
          PCPXML::deleteNode (childNode);
          childNode = nextSiblingNode;
          continue;
        }
      }
      idaTrackExcept(("XML:Request/SearchSpecification/Operation: bad or missing value"));
      errorText = "XML:Request/SearchSpecification/Operation: bad or missing value";
      return isNotOk;
    }

    // -------------------------------------------------------------------------
    // SearchSpecification/LinkContext:
    if (childNodeName == "LinkContext")
    {
      if (getValueOfDomNode(childNodeValue, childNode ) == isOk)
      {
        idaTrackTrace (( "xmlnode: %s with value: %s", childNodeName.cString (), childNodeValue.cString () ));
        searchSpec.setLinkContext(hexToData(childNodeValue.cString()));
        nextSiblingNode = PCPXML:: getNextSibling( childNode );
        PCPXML::deleteNode (childNode);
        childNode = nextSiblingNode;
        continue;
      }
    }

    // -------------------------------------------------------------------------
    // SearchSpecification/OsaCharacterSet:
    if (childNodeName == "OsaCharacterSet")
    {
      if (getValueOfDomNode(childNodeValue, childNode ) == isOk)
      {
        if (StringToEnum::enumOfCharacterSet(enumVal, childNodeValue.cString()) == isOk)
        {
          idaTrackTrace (( "xmlnode: %s with value: %s", childNodeName.cString (), childNodeValue.cString () ));
          searchSpec.setUsedCharacterSet((SearchSpec::CharacterSet)enumVal);
          nextSiblingNode = PCPXML:: getNextSibling( childNode );
          PCPXML::deleteNode (childNode);
          childNode = nextSiblingNode;
          continue;
        }
      }
      idaTrackExcept(("XML:Request/SearchSpecification/OsaCharacterSet: bad or missing value"));
      errorText = "XML:Request/SearchSpecification/OsaCharacterSet: bad or missing value";
      return isNotOk;
    }
    nextSiblingNode = PCPXML:: getNextSibling( childNode );
    PCPXML::deleteNode (childNode);
    childNode = nextSiblingNode;
  }

  // -------------------------------------------------------------------------
  // ... und jetzt noch das ganze ... in's TdsRequest-Object einhängen
  tdsRequest.setSearchSpecification(searchSpec);

  return result;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	SearchVariation erzeugen:
//
ReturnStatus TdfAccess::createSearchVariation(TdsRequest& tdsRequest, ClNode* node, String& errorText)
{
  TRACE_FUNCTION("TdfAccess::createSearchVariation(...)");

  SearchVar searchVar;

  ClNode * childNode = PCPXML:: getFirstChild ( node );
  ClNode * nextSiblingNode = NULL;
  ULong enumVal;
  String stringValue;

  if ( childNode == NULL ) return isOk;

  ULong variation = 0;

  while ( childNode )
  {
    String childNodeName;
    PCPXML:: getNodeName ( childNode, childNodeName );

    // -------------------------------------------------------------------------
    // SearchVariation/SearchType:
    if (childNodeName == "SearchType")
    {
      if (getValueOfDomNode(stringValue, childNode) == isOk)
      {
        if (StringToEnum::enumOfSearchType(enumVal, stringValue.cString()) == isOk)
        {
          idaTrackTrace (( "xmlnode: %s with value: %s", childNodeName.cString (), stringValue.cString () ));

          searchVar.setSearchType((SearchVar::SearchType)enumVal);
          nextSiblingNode = PCPXML:: getNextSibling( childNode );
          PCPXML::deleteNode (childNode);
          childNode = nextSiblingNode;
          continue;
        }
      }
      idaTrackExcept(("XML:Request/SearchVariation/SearchType: bad or missing value"));
      errorText = "XML:Request/SearchVariation/SearchType: bad or missing value";
      return isNotOk;
    }

    // -------------------------------------------------------------------------
    // SearchVariation/Expansion:
    if (childNodeName == "Expansion")
    {
      if (getValueOfDomNode(stringValue, childNode) == isOk)
      {
        if (StringToEnum::enumOfExpansion(enumVal, stringValue.cString()) == isOk)
        {
          idaTrackTrace (( "xmlnode: %s with value: %s", childNodeName.cString (), stringValue.cString () ));
          searchVar.setExpansion((SearchVar::Expansion)enumVal);
          nextSiblingNode = PCPXML:: getNextSibling( childNode );
          PCPXML::deleteNode (childNode);
          childNode = nextSiblingNode;
          continue;
        }
      }
      idaTrackExcept(("XML:Request/SearchVariation/Expansion: bad or missing value"));
      errorText = "XML:Request/SearchVariation/Expansion: bad or missing value";
      return isNotOk;
    }

    // -------------------------------------------------------------------------
    // SearchVariation/ExpansionRange:
    if (childNodeName == "ExpansionRange")
    {
      if (getValueOfDomNode(stringValue, childNode) == isOk)
      {
        idaTrackTrace (( "xmlnode: %s with value: %s", childNodeName.cString (), stringValue.cString () ));
        searchVar.setExpansionRange(VisibleString(stringValue.cString()));
        nextSiblingNode = PCPXML:: getNextSibling( childNode );
        PCPXML::deleteNode (childNode);
        childNode = nextSiblingNode;
        continue;
      }
    }

    // -------------------------------------------------------------------------
    // SearchVariation/IndentLevelFilter:
    if (childNodeName == "IndentLevelFilter")
    {
      if (getValueOfDomNode(stringValue, childNode) == isOk)
      {
        idaTrackTrace (( "xmlnode: %s with value: %s", childNodeName.cString (), stringValue.cString () ));
        searchVar.setIndentLevelFilter(stringValue.cString());
        nextSiblingNode = PCPXML:: getNextSibling( childNode );
        PCPXML::deleteNode (childNode);
        childNode = nextSiblingNode;
        continue;
      }
    }

    // -------------------------------------------------------------------------
    // SearchVariation/Variation:
    if (childNodeName == "Variation")
    {
      if (getValueOfDomNode(stringValue, childNode ) == isOk)
      {
        if (StringToEnum::enumOfSearchVar(enumVal, stringValue.cString()) == isOk)
        {
          idaTrackTrace (( "xmlnode: %s with value: %s", childNodeName.cString (), stringValue.cString () ));
          variation += enumVal;
          nextSiblingNode = PCPXML:: getNextSibling( childNode );
          PCPXML::deleteNode (childNode);
          childNode = nextSiblingNode;
          continue;
        }
      }
      idaTrackExcept(("XML:Request/SearchVariation/Variation: bad or missing value"));
      errorText = "XML:Request/SearchVariation/Variation: bad or missing value";
      return isNotOk;
    }
    nextSiblingNode = PCPXML:: getNextSibling( childNode );
    PCPXML::deleteNode (childNode);
    childNode = nextSiblingNode;
  }

  if (variation) searchVar.setHelper1(variation);

  // -------------------------------------------------------------------------
  // ... und jetzt noch das ganze ... in's TdsRequest-Object einhängen
  tdsRequest.setSearchVariation(searchVar);

  return isOk;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	RequestedResponse erzeugen:
//
ReturnStatus TdfAccess::createRequestedResponse ( TdsRequest & tdsRequest, ClNode * node, String & errorText )
{
  TRACE_FUNCTION("TdfAccess::createRequestedResponse(...)");

  RequestedRes requestedRes;

  ClNode * childNode = PCPXML:: getFirstChild ( node );
  ClNode * nextSiblingNode = NULL;
  ULong enumVal;
  String stringValue;

  // -------------------------------------------------------------------------
  // SearchVariation/RequestedFormat:
  // Diesen Parameter setzen wir fest ein, da es keine andere Möglichkeit gibt
  requestedRes.setRequestedFormat(RequestedRes::indentedRecordFormat);

  if ( childNode == NULL ) return isOk;

  SetOfAttributeId setOfAttributeId;

  Bool attrFlag = false;

  while ( childNode )
  {
    String childNodeName;
    PCPXML:: getNodeName ( childNode, childNodeName );

    // -------------------------------------------------------------------------
    // RequestedResponse/MaxRecords:
    if (childNodeName == "MaxRecords")
    {
      if (getValueOfDomNode(stringValue, childNode ) == isOk)
      {
        idaTrackTrace (( "xmlnode: %s with value: %s", childNodeName.cString (), stringValue.cString () ));
        requestedRes.setMaxRecords(ushortOfString(stringValue.cString()));
        nextSiblingNode = PCPXML:: getNextSibling( childNode );
        PCPXML::deleteNode (childNode);
        childNode = nextSiblingNode;
        continue;
      }
    }

    // -------------------------------------------------------------------------
    // RequestedResponse/Ordering:
    if (childNodeName == "Ordering")
    {
      if (getValueOfDomNode(stringValue, childNode ) == isOk)
      {
        idaTrackTrace (( "xmlnode: %s with value: %s", childNodeName.cString (), stringValue.cString () ));
        requestedRes.setOrdering(stringValue.cString());
        nextSiblingNode = PCPXML:: getNextSibling( childNode );
        PCPXML::deleteNode (childNode);
        childNode = nextSiblingNode;
        continue;
      }
    }

    // -------------------------------------------------------------------------
    // RequestedResponse/SearchFilter:
    if (childNodeName == "DbAttribute")
    {
      if (getValueOfDomNode(stringValue, childNode ) == isOk)
      {
        if (StringToEnum::enumOfDbAttribute(enumVal, stringValue.cString()) == isOk)
        {
          idaTrackTrace (( "xmlnode: %s with value: %s", childNodeName.cString (), stringValue.cString () ));
          setOfAttributeId.insert((DbAttribute::AttributeId)enumVal);
          attrFlag = true;
          nextSiblingNode = PCPXML:: getNextSibling( childNode );
          PCPXML::deleteNode (childNode);
          childNode = nextSiblingNode;
          continue;
        }
      }
      idaTrackExcept(("XML:Request/RequestedResponse/DbAttribute: bad or missing value"));
      errorText = "XML:Request/RequestedResponse/DbAttribute: bad or missing value";
      return isNotOk;
    }
    nextSiblingNode = PCPXML:: getNextSibling( childNode );
    PCPXML::deleteNode (childNode);
    childNode = nextSiblingNode;
  }

  // -------------------------------------------------------------------------
  // ... und jetzt noch das ganze ... in's TdsRequest-Object einhängen
  if (attrFlag) requestedRes.setSearchFilter(setOfAttributeId);
  tdsRequest.setRequestedResponse(requestedRes);

  return isOk;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	SearchAttributes erzeugen:
//
ReturnStatus TdfAccess::createSearchAttributes ( TdsRequest & tdsRequest, ClNode * node, String & errorText)
{
  TRACE_FUNCTION("TdfAccess::createSearchAttributes(...)");

  VectorOfSearchAttr vectorOfSearchAttr;

  ClNode * childNode;
  ClNode * nextSiblingNode = NULL;

  ULong enumVal;

  idaTrackTrace(("TdfAccess::createSearchAttributes after declarations"));

  // den ersten Unterknoten holen ("SearchAttr")
  childNode = PCPXML:: getFirstChild ( node );
  if ( childNode == null )
  {
    errorText = "missing search attributes";
    return isNotOk;
  }
  idaTrackTrace(("TdfAccess::createSearchAttributes after childNode = getFirstChild( childNode)"));

  // -------------------------------------------------------------------------
  // Iteration über alle SearchAttr-Knoten
  while ( childNode )
  {
    idaTrackTrace(("TdfAccess::createSearchAttributes child != null"));
    // ---------------------------------------------------------------------
    // Für alle Fälle prüfen wir mal, ob wir nicht etwas falsches bekommen haben
    String childNodeName;
    PCPXML:: getNodeName ( childNode, childNodeName );
    idaTrackTrace(("TdfAccess::createSearchAttributes after XMLString::transcode(name)"));
    if ( childNodeName != "SearchAttr")
    {
      idaTrackTrace(("TdfAccess::createSearchAttributes childNodeName != searchAttr"));
      // if no match continue with next node
      nextSiblingNode = PCPXML:: getNextSibling( childNode );
      PCPXML::deleteNode (childNode);
      childNode = nextSiblingNode;
      continue;
    }

    idaTrackTrace(("TdfAccess::createSearchAttributes before loop over grandchild nodes"));
    // ---------------------------------------------------------------------
    // If the node does not have any children, continue with the next node
    ClNode * grandchildNode = PCPXML:: getFirstChild ( childNode );

    if ( grandchildNode == NULL )
    {
      nextSiblingNode = PCPXML:: getNextSibling( childNode );
      PCPXML::deleteNode (childNode);
      childNode = nextSiblingNode;
      continue;
    }  


    idaTrackTrace(("TdfAccess::createSearchAttributes before  for (int i = 0; i < nodeList->getLength(); i++)"));
    // ---------------------------------------------------------------------
    // Wir haben eine SearchAttr Knoten gefunden, der auch Kinder hat.
    // Es folgt eine Iteration über alle Kinder
    SearchAttr searchAttr;
    searchAttr.reset();
    ULong variation = 0;			      // for the Variation
    Bool legalAttribut = false;		// Flag for loop control

    while ( grandchildNode )
    {
      String grandchildNodeName;
      PCPXML:: getNodeName ( grandchildNode, grandchildNodeName );

      String stringValue("");

      // -----------------------------------------------------------------
      // Attribut:
      if ( grandchildNodeName == "Attribute")
      {
        if (getValueOfDomNode(stringValue, grandchildNode) == isOk)
        {
          if (StringToEnum::enumOfAttributeId(enumVal, stringValue.cString()) == isOk)
          {
            searchAttr.setAttributeId((SearchAttr::AttributeId)enumVal);
            idaTrackData(("XML:Request/SearchAttributes/Attribute: "));	
            // Der Attribut-Name war gültig
            legalAttribut = true;
            nextSiblingNode = PCPXML:: getNextSibling( grandchildNode );
            PCPXML::deleteNode (grandchildNode);
            grandchildNode = nextSiblingNode;
            continue;
          }
        }
        idaTrackExcept(("XML:Request/SearchAttributes/Attribute: bad or missing value"));
        break;
      }
      // -----------------------------------------------------------------
      // Value:
      if ( grandchildNodeName == "Value")
      {
        if (getValueOfDomNode(stringValue, grandchildNode) == isOk)
        {
          searchAttr.setAttributeValue(stringValue.cString());
          idaTrackData(("XML:Request/SearchAttributes/Attribute/Value : %s", stringValue.cString() ));
          nextSiblingNode = PCPXML:: getNextSibling( grandchildNode );
          PCPXML::deleteNode (grandchildNode);
          grandchildNode = nextSiblingNode;
          continue;
        }
        idaTrackExcept(("XML:Request/SearchAttributes/Attribute/Value: bad or missing value"));

      }
      // -----------------------------------------------------------------
      // Variation:
      if ( grandchildNodeName ==  "Variation")
      {
        if ( getValueOfDomNode ( stringValue, grandchildNode ) == isOk)
        {
          if (StringToEnum::enumOfSearchAttr(enumVal, stringValue.cString()) == isOk)
          {
            variation += enumVal;
            idaTrackData(("XML:Request/SearchAttributes/Variation : %s", stringValue.cString() ));
            nextSiblingNode = PCPXML:: getNextSibling( grandchildNode );
            PCPXML::deleteNode (grandchildNode);
            grandchildNode = nextSiblingNode;
            continue;
          }
        }
        idaTrackExcept(("XML:Request/SearchAttributes/Variation: bad or missing value"));
        errorText = "XML:Request/SearchAttributes/Variation: bad or missing value";
        return isNotOk;
      }

      nextSiblingNode = PCPXML:: getNextSibling( grandchildNode );
      PCPXML::deleteNode (grandchildNode);
      grandchildNode = nextSiblingNode;
    }
    // ---------------------------------------------------------------------
    // Wenn ein gültiges Attribut gefunden wurde ...
    if (legalAttribut)
    {
      // ... dann muß noch geprüft werden, ob variations gesetzt wurden ...
      if (variation != 0) searchAttr.setHelper1(variation);

      // ... bevor man das SearchAttr-Objekt in den Vector eintragen kann
      vectorOfSearchAttr.push_back(searchAttr);
    }

    nextSiblingNode = PCPXML:: getNextSibling( childNode );
    PCPXML::deleteNode (childNode);
    childNode = nextSiblingNode;
  }

  tdsRequest.setSearchAttributes(vectorOfSearchAttr);

  return isOk;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Holt den Wert eines DOM-Knotens. Das Ergebnis der Methode ist nur dann
//	"isOk", wenn ein geeigneter Unterknoten vom Typ TEXT_NODE existiert und
//	dieser einen String mit einer Länge größer Null beinnhaltet.
//
ReturnStatus TdfAccess::getValueOfDomNode ( String& textValue, ClNode* node )
{
  TRACE_FUNCTION("TdfAccess::getValueOfDomNode(...)");

  ClNode * textNode = PCPXML:: getFirstChild( node );

  // The first child of the node must be a test node
  if ( textNode != null )
  {
    if ( PCPXML:: getNodeType( textNode ) == ClTextNode )
    {
      // get node value without character conversion
      PCPXML:: getNodeValue ( textNode, textValue, null );
      if ( !textValue.isEmpty() )
      {
        PCPXML:: deleteNode (textNode);
        return isOk;
      }
    }
    else
    {
      idaTrackData((" PCPXML::getNodeType() != ClTextNode !"));
    }
    PCPXML:: deleteNode (textNode);
  }

  idaTrackData(("getValueOfDomNode(): could not retreive node value, or it was empty !"));

  return isNotOk;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Erzeugt ein TdfArgument-Objekt 
//
ReturnStatus TdfAccess::createTdfArgument(TdfArgument*	tdfArgument,
                                          TdsRequest&	tdsRequest,
                                          ClNode * rootNode,
                                          UShort        requestId,
                                          String&       errorText)
{
  TRACE_FUNCTION("TdfAccess::createTdfArgument(...)");

  ReturnStatus result = isOk;


  // -------------------------------------------------------------------------
  // Einige Parameter aus dem DOM holen
  String clientIpAddressString("127.0.0.1");		// Default 
  String linkContextString;
  getParametersFromDom(rootNode, clientIpAddressString, linkContextString);

  TdfOriginId tdfOriginId;
  assembleTdfOriginId(&tdfOriginId, clientIpAddressString.cString());


  // -------------------------------------------------------------------------
  // TdfArgument Objekt anlegen
  if (tdfArgument == null)
  {
    idaTrackFatal(("out of memory during creation of TdfArgument"));
    errorText = "out of memory";
    return isNotOk;
  }

  // Attribute des TdfArgument-Objektes setzen
  tdfArgument->setApplicationId(applicationId);	// Kennung des OSA-Clients
  tdfArgument->setOsaTicket(osaTicket);			// Zugriffskontrolle
  tdfArgument->setReference(requestId);			// eindeutige Nummer des Requestes
  tdfArgument->setDataFormat(tdsDataFormat);		// zeigt an, daß die Daten im TDS-Format kommen
  tdfArgument->setData(tdsRequest);				// die eigentlichen Daten
  // die Daten des Objektes werden hier kopiert !
  tdfArgument->setSourceId(0);					// Hier nur ein Dummy für die Channel Nummer
  // damit der Test gut geht
  tdfArgument->setRequestAddress(tdfOriginId);	// Client-Info für MIS Statistik
  if (!linkContextString.isEmpty())
  {
    tdfArgument->setLinkContext(hexToData(linkContextString.cString()));	// TDF LinkContext setzen
  }

  // Test, ob alle obligatorischen Daten vorhanden sind !
  if (tdfArgument->areMandatoryItemsSet() == false)
  {
    idaTrackExcept(("TdfAccess::handleRequest: tdfArgument - args missing"));

    errorText = "TdfArgument: args missing";

    return isNotOk;
  }

  return isOk;		
}


ReturnStatus TdfAccess::createTdfArgument(TdfArgument*  tdfArgument,
                                          ModifyRequest&   modifyRequest,
                                          ClNode * rootNode,
                                          UShort        requestId,
                                          String&       errorText)
{
  TRACE_FUNCTION("TdfAccess::createTdfArgument(...)");

  // -------------------------------------------------------------------------
  // Einige Parameter aus dem DOM holen
  String clientIpAddressString("127.0.0.1");              // Default
  String linkContextString;
  getParametersFromDom ( rootNode, clientIpAddressString, linkContextString );

  TdfOriginId tdfOriginId;
  assembleTdfOriginId(&tdfOriginId, clientIpAddressString.cString());

  // -------------------------------------------------------------------------
  // TdfArgument Objekt anlegen
  if (tdfArgument == null)
  {
    idaTrackFatal(("out of memory during creation of TdfArgument"));
    errorText = "out of memory";
    return isNotOk;
  }

  // Attribute des TdfArgument-Objektes setzen
  tdfArgument->setApplicationId(applicationId);   // Kennung des OSA-Clients
  tdfArgument->setOsaTicket(osaTicket);                   // Zugriffskontrolle
  tdfArgument->setReference(requestId);                   // eindeutige Nummer des Requestes
  tdfArgument->setDataFormat(tdsModifyDataFormat);              // zeigt an, daß die Daten im TDS-ModifyFormat kommen
  tdfArgument->setData(modifyRequest);                               // die eigentlichen Daten
  // die Daten des Objektes werden hier kopiert !
  tdfArgument->setSourceId(0);                                    // Hier nur ein Dummy für die Channel Nummer
  // damit der Test gut geht
  tdfArgument->setRequestAddress(tdfOriginId);    // Client-Info für MIS Statistik
  if (!linkContextString.isEmpty())
  {
    tdfArgument->setLinkContext(hexToData(linkContextString.cString()));    // TDF LinkContext setzen
  }

  // Test, ob alle obligatorischen Daten vorhanden sind !
  if (tdfArgument->areMandatoryItemsSet() == false)
  {
    idaTrackExcept(("TdfAccess::handleRequest: tdfArgument - args missing"));
    errorText = "TdfArgument: args missing";
    return isNotOk;
  }

  return isOk;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Diese Methode hat die Aufgabe die Liste der vorbereiteten Request-Objekte (Sendqueue)
//	abzuarbeiten. Sie prüft, ob Requests darauf warten verarbeitet zu werden und entscheidet
//	dann anhand ihres Status, ob z.B. zunächst eine Authentifizierung beim SES-Prozeß 
//	eingeholt werden muß, oder eine Such-Anfrage an das OSA-GW gesendet werden kann.
//
Void TdfAccess::processSendQueue()
{
  TRACE_FUNCTION("TdfAccess::processSendQueue(...)");

  idaTrackData(("process send queue:"));

  // -------------------------------------------------------------------------
  // Wir brauchen nur zu senden, wenn die Registrierung steht
  // Dadurch werden überflüssige Sendeversuche vermieden
  //	if (getStatus() != registered)
  //	{
  //		// Wenn nicht, dann wird der Timer gestartet, damit diese Methode
  //		// regelmäßig wieder aufgerufen wird
  //		startSendRetryTimer();
  //		return;
  //	}


  // -------------------------------------------------------------------------
  // Der älteste Request kommt zuerst dran
  // Wenn das Ergebnis "false" ist, dann gibt es nichts zu tun
  RequestContainer requestContainer;
  if (!requestPool.getOldestWaitingRequest(requestContainer)) return;

  // -------------------------------------------------------------------------
  // Wir versuchen einen freien Channel zu bekommen
  UShort channel;
  channel = channelMgr.reserveChannel();
  if (channel <= 0)
  {
    idaTrackExcept(("no free channel found"));
    return;
  }


  // -------------------------------------------------------------------------
  // Channel eintragen
  requestContainer.setChannel(channel);
  requestContainer.getTdfArgument()->setSourceId(channel);
  idaTrackData(("sending on channel: %d", channel));


  // -------------------------------------------------------------------------
  // Feuer frei !!
  idaTrackTrace (( "%s: channel is: %d", dbIdString, channel ));

  ReturnStatus rs = isNotOk;
  TdfDataFormat format = requestContainer.getTdfArgument()->getDataFormat();

  if ( format == tdsModifyDataFormat )
  {
    rs = modifyRequest(*(requestContainer.getTdfArgument()));
  }
  else
  {
    rs = searchRequest(*(requestContainer.getTdfArgument()));
  }

  if (rs == isOk)
  {

    {
      PcpTime time;
      idaTrackData(("*** Request send to OSA-Gateway      at %d:%d:%d,%d",
        time.getHour(), time.getMinute(), time.getSec(), time.getMilliSec()));
    }

    // Nachricht konnte gesendet werden
    requestContainer.setStatus(RequestContainer::sendAndWaiting);
    requestPool.removeRequest(requestContainer.getRequestId());
    requestPool.addRequest(requestContainer);

    // Und weil es so schoen war, geht vieleicht noch einer
    // ACHTUNG: Rekursion
    processSendQueue();
  }
  else
  {
    OsaComError error = getLastError();
    idaTrackData(("search/modifyRequest() failed:  error.getErrorCode() = %d", error.getErrorCode() ));
    idaTrackData(("search/modifyRequest() failed:  error.getErrorSource() = %s", error.getErrorSource() ));
    idaTrackData(("search/modifyRequest() failed:  error.getErrorText() = %s", error.getErrorText() ));


    // leider hat's nicht geklappt !
    ALARM(0, iDAMinRepClass, 1, " TdfAccess --> IDA_DAP");
    idaTrackExcept(("search/modifyRequest() failed"));

    // Den Channel müssen wir wieder freigeben
    requestContainer.setChannel(0);
    requestContainer.getTdfArgument()->setSourceId(0);
    channelMgr.releaseChannel(channel);

    // Es kann nur die Registrierung verloren geganngen sein
    // also :
    startRegistration();
  }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Wandelt einen HEX-formatierten String in einen Byte-String um
//
//	>>> die Funktion ist zur Zeit noch nicht ganz dicht !!!
//
Data& TdfAccess::hexToData(const char * hex)
{
  TRACE_FUNCTION("TdfAccess::hexToData(...)");
  //idaTrackTrace(("TdfAccess::hexToData(...)"));

  // Buffer zurücksetzen
  tempData.reset();
  const Uint buflen = 1024;
  static char buffer[1024];
  String result;

  // Schutz vor Null-Pointer Übergabe
  if (hex == 0) return tempData;

  // Schutz vor Fehlern wg. ungerader Länge des hex strings
  if ((strlen(hex) % 2) != 0) return tempData;

  size_t i = 0;
  size_t pos = 0;
  UShort l = 0;
  for (; i < strlen(hex) - 1; i += 2)
  {
    l++;
    char c1 = hex[i];
    char c2 = hex[i+1];

    c1 = (c1 >= 'A') ? (c1 - 'A' + 10) : (c1 - '0');
    c2 = (c2 >= 'A') ? (c2 - 'A' + 10) : (c2 - '0');

    buffer[pos] = (c1 << 4) + c2;
    pos++;

    if( pos == buflen )
    {
      result += String( buffer, pos );
      pos = 0;
    }
  }

  result += String( buffer, pos );


  tempData.assign(result.cString(), l);

  return tempData;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
UChar TdfAccess::ucharOfString(const char* str)
{
  return (UChar)atoi(str);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
ULong TdfAccess::ulongOfString(const char* str)
{
  return (ULong)atol(str);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
UShort TdfAccess::ushortOfString(const char* str)
{
  return (UShort)atol(str);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Die folgende Funktion liefert zwei Werte aus dem DOM und zwar:
//		"ClientIpAddress" und "TdfLinkContext"
//
ReturnStatus TdfAccess::getParametersFromDom( ClNode *	rootNode,
                                             String&			clientIpAddressString,
                                             String&			linkContextString)
{
  TRACE_FUNCTION("TdfAccess::getParametersFromDom(...)");

  if (rootNode==null) return isNotOk;

  ClNode * childNode = PCPXML:: getFirstChild ( rootNode );
  ClNode * nextSiblingNode = NULL;

  while ( childNode )
  {
    // Koten-Typ prüfen
    if ( PCPXML:: getNodeType ( childNode ) == ClElementNode )
    {
      // get node name
      String childNodeName;
      PCPXML:: getNodeName ( childNode, childNodeName );

      // <ClientIpAddress> found?
      if ( childNodeName == "ClientIpAddress")
      {
        getValueOfDomNode ( clientIpAddressString, childNode );
        idaTrackData(("clientIp: %s", linkContextString.cString()));
      }

      // <TdfLinkContext> found?
      if ( childNodeName == "TdfLinkContext")
      {
        getValueOfDomNode ( linkContextString, childNode );
        idaTrackData(("TdfLinkContext: %s", linkContextString.cString()));
      }
    }
    nextSiblingNode = PCPXML:: getNextSibling( childNode );
    PCPXML::deleteNode (childNode);
    childNode = nextSiblingNode;
  }

  return isOk;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Die folgende Funktion liefert zwei Werte aus dem DOM und zwar:
//		"ClientIpAddress" und "TdfLinkContext"
//
ReturnStatus TdfAccess::getTimeoutFromDom ( ClNode * rootNode, ULong& timeout)
{
  TRACE_FUNCTION("TdfAccess::getTimeoutFromDom(...)");

  ClNode * timeoutNode = PCPXML:: selectSingleChildNode ( rootNode, "Timeout" );

  String timeoutString;
  if ( getValueOfDomNode ( timeoutString, timeoutNode ) == isOk )
  {
    // String nach ULong wandeln
    long tmpVal = 0;
    tmpVal = atol(timeoutString.cString());
    if (tmpVal > 0) timeout = tmpVal;
  }

  PCPXML::deleteNode(timeoutNode);

  return isOk;
}


ReturnStatus TdfAccess::createModifySpecification(ModifyRequest& modifyRequest, ClNode* node, String& errorText)
{
  TRACE_FUNCTION("TdfAccess::createModifySpecification(...)");

  ULong enumVal;

  ClNode * childNode = PCPXML:: getFirstChild ( node );
  ClNode * nextSiblingNode = NULL;

  if ( childNode == NULL )
  { 
    errorText = "XML:Request/ModifySpecification: missing parameter";
    return isOk;
  }

  while ( childNode )
  {
    String childNodeName;
    String childNodeValue;
    PCPXML::getNodeName ( childNode, childNodeName );

    // -------------------------------------------------------------------------
    // ModifySpecification/Operation:
    if ( childNodeName == String ( "Operation" ) )
    {
      // get node value without character conversion

      if ( getValueOfDomNode ( childNodeValue, childNode ) == isOk )
      {
        if (StringToEnum::enumOfModifyOperation(enumVal, childNodeValue.cString()) == isOk)
        {
          idaTrackData (( "Operation is: %s", childNodeValue.cString() ));
          modifyRequest.setOperation((ModifyRequest::Operation)enumVal);
        }
      }
      else
      {
        idaTrackExcept(("XML:Request/ModifySpecification/Operation: bad or missing value"));
        errorText = "XML:Request/ModifySpecification/Operation: bad or missing value";
        PCPXML::deleteNode (childNode);
        return isNotOk;
      }  
    }

    // -------------------------------------------------------------------------
    // ModifySpecification/LinkContext:
    if ( childNodeName == String ( "LinkContext" ) )
    {
      if ( getValueOfDomNode ( childNodeValue, childNode ) == isOk ) 
      {
        idaTrackTrace (( "LinkContext is: %s", childNodeValue.cString() ));
        modifyRequest.setLinkContext(hexToData(childNodeValue.cString()));
      }
    }

    nextSiblingNode = PCPXML:: getNextSibling( childNode );
    PCPXML::deleteNode (childNode);
    childNode = nextSiblingNode;
  }

  return isOk;
}


ReturnStatus TdfAccess::createModifyAttributes(ModifyRequest& modifyRequest, ClNode* node, String& errorText)
{
  TRACE_FUNCTION("TdfAccess::createModifyAttributes(...)");

  SetOfDbAttribute set;

  ClNode * childNode;
  ClNode * nextSiblingNode = NULL;
  ULong enumVal;

  idaTrackTrace(("TdfAccess::createModifyAttributes after definition of parameters"));

  // den ersten Unterknoten holen ("ModifyAttr")

  childNode = PCPXML:: getFirstChild( node );
  if (childNode == null)
  {
    errorText = "missing modify attributes";
    return isNotOk;
  }
  idaTrackTrace(("TdfAccess::createModifyAttributes after child = >getFirstChild( node )"));

  // -------------------------------------------------------------------------
  // Iteration über alle ModifyAttr-Knoten
  while (!childNode==null)
  {
    String childNodeName;
    String childNodeValue;
    idaTrackTrace(("TdfAccess::createModifyAttributes after !childNode==null"));
    // ---------------------------------------------------------------------
    // Für alle Faelle pruefen wir mal, ob wir nicht etwas falsches bekommen haben
    PCPXML:: getNodeName ( childNode, childNodeName );

    idaTrackTrace(("TdfAccess::createModifyAttributes after child->getNodeName(childNode,childNodeName)"));

    if ( childNodeName != "ModifyAttr")
    {
      idaTrackTrace(("TdfAccess::createModifyAttributes after comparing node name to ModifyAttr"));
      // wenn doch, dann mit dem naechsten Knoten weiter machen
      nextSiblingNode = PCPXML:: getNextSibling( childNode );
      PCPXML::deleteNode (childNode);
      childNode = nextSiblingNode;
      continue;
    }

    idaTrackTrace(("TdfAccess::createModifyAttributes before grandchildNode = PCPXML:: getFirstChild ( childNode )"));
    // ---------------------------------------------------------------------
    // Wenn der Knoten keine Kinder hat, machen wir auch mit dem nächsten weiter
    ClNode * grandchildNode;
    grandchildNode = PCPXML:: getFirstChild ( childNode );
    if ( grandchildNode == NULL )
    {
      nextSiblingNode = PCPXML:: getNextSibling( childNode );
      PCPXML::deleteNode (childNode);
      childNode = nextSiblingNode;
      continue;
    }

    idaTrackTrace(("TdfAccess::createModifyAttributes before iterating over grandchildren"));
    // ---------------------------------------------------------------------
    // Wir haben eine SearchAttr Knoten gefunden, der auch Kinder hat.
    // Es folgt eine Iteration über alle Kinder
    DbAttribute attr;
    attr.reset();
    ULong variation = 0;                    // für die Variation
    Bool legalAttribut = false;             // Flag zur Schleifensteuerung

    while ( grandchildNode )
    {
      String grandchildNodeName;
      String grandchildNodeValue;
      PCPXML:: getNodeName ( grandchildNode, grandchildNodeName );

      // -----------------------------------------------------------------
      // Attribut:
      if ( grandchildNodeName == "Attribute" )
      {
        if ( getValueOfDomNode ( grandchildNodeValue, grandchildNode ) == isOk )
        {
          if (StringToEnum::enumOfDbAttribute(enumVal, grandchildNodeValue.cString()) == isOk)
          {
            attr.setAttributeId((DbAttribute::AttributeId)enumVal);
            idaTrackData(("XML:Request/ModifyAttributes/Attribute: %s", grandchildNodeValue.cString() ));
            // Der Attribut-Name war gueltig
            legalAttribut = true;
            nextSiblingNode = PCPXML:: getNextSibling( grandchildNode );
            PCPXML::deleteNode (grandchildNode);
            grandchildNode = nextSiblingNode;
            continue;
          }
          else
          {
            errorText = String("unknown attribute ") + grandchildNodeValue;
            return isNotOk;  // unknown attribute, error according to Yu
          }                
        }
        idaTrackExcept(("XML:Request/ModifyAttributes/Attribute: bad or missing value: %s", grandchildNodeValue.cString()));
        //break;
      }  
      // -----------------------------------------------------------------
      // Value:

      if ( grandchildNodeName == "Value" )
      {
        if ( getValueOfDomNode ( grandchildNodeValue, grandchildNode ) == isOk )
        {
          attr.setData(grandchildNodeValue.cString());
          idaTrackData(("XML:Request/ModifyAttributes/Attribute/Value : %s", grandchildNodeValue.cString()));

          nextSiblingNode = PCPXML:: getNextSibling( grandchildNode );
          PCPXML::deleteNode (grandchildNode);
          grandchildNode = nextSiblingNode;
          continue;
        }   
        idaTrackExcept(("XML:Request/ModifyAttributes/Attribute/Value: bad or missing value"));
      }
      nextSiblingNode = PCPXML:: getNextSibling( grandchildNode );
      PCPXML::deleteNode (grandchildNode);
      grandchildNode = nextSiblingNode;
    }
    // ---------------------------------------------------------------------
    // Wenn ein gültiges Attribut gefunden wurde ...
    if (legalAttribut)
    {
      set.insert(attr);
    }

    nextSiblingNode = PCPXML:: getNextSibling( childNode );
    PCPXML::deleteNode (childNode);
    childNode = nextSiblingNode;
  }

  modifyRequest.setSetOfDbAttribute(set);

  return isOk;
}


ReturnStatus TdfAccess::createModifyFlags(ModifyRequest& modifyRequest, ClNode* node, String& errorText)
{
  TRACE_FUNCTION("TdfAccess::createModifyFlags(...)");

  ReturnStatus result = isOk;
  ClNode * childNode;
  ClNode * nextSiblingNode = NULL;

  ULong flags = 0;
  ULong flagsMask = 0;

  childNode = PCPXML:: getFirstChild ( node );

  if ( childNode == NULL )
  {
    errorText = "XML:Request/ModifyFlags: missing parameter";
    return isNotOk;
  }  

  clTrackTrace(("createModifyFlags searching for <Flags>"));

  while ( childNode )
  {
    String childNodeName;
    String childNodeValue;

    PCPXML:: getNodeName ( childNode, childNodeName );

    if ( childNodeName == "Flags")
    {
      clTrackTrace(("createModifyFlags <Flags> found"));

      ClNode * flagNode = PCPXML:: getFirstChild ( childNode );

      if ( flagNode == NULL )
      {
        return result;
      }

      while ( flagNode )
      {
        String flagNodeName;
        String flagNodeValue;
        PCPXML:: getNodeName ( flagNode, flagNodeName );

        if ( flagNodeName == "Flag" )
        {
          clTrackTrace(("createModifyFlags <Flag> found"));

          String name;
          ULong value = 0;
          Bool nameSet = false;
          Bool valueSet = false;

          ClNode * flagNodeChild = PCPXML:: getFirstChild (flagNode);

          while ( flagNodeChild )
          {
            String flagNodeChildName;
            String flagNodeChildValue;

            PCPXML:: getNodeName ( flagNodeChild, flagNodeChildName );

            if ( flagNodeChildName == "Name" )
            {
              idaTrackTrace(("createModifyFlags <Name> found"));
              if ( getValueOfDomNode ( name, flagNodeChild ) == isOk )
              {
                nameSet = true;
                idaTrackTrace(("createModifyFlags: got name %s from Node", name.cString() ));
              }
            }
            else if ( flagNodeChildName == "Value" )
            {
              idaTrackTrace(("createModifyFlags <Value> found"));
              if ( getValueOfDomNode ( flagNodeChildValue, flagNodeChild ) == isOk )
              {
                if ( DecString(flagNodeChildValue).getValue(value) == isOk )
                {
                  valueSet = true;
                  idaTrackTrace(("createModifyFlags: got value %ld from Node", (ULong)value ));
                }
              } 
            }
            nextSiblingNode = PCPXML:: getNextSibling( flagNodeChild );
            PCPXML::deleteNode (flagNodeChild);
            flagNodeChild = nextSiblingNode;
          } // end while (flagNodeChild)

          if ( nameSet && valueSet )
          {
            IndentRecord::Flags mask = (IndentRecord::Flags)0;

            if ( stringToEnum(name.cString(), mask) != isOk )
            {
              idaTrackExcept(("createModifyFlags: %s is unknown", name.cString() ));
              errorText = String(" unknown flag: ") + name;
              result = isNotOk;
            }
            else // set the flag value and mask:
            {
              if ( value )
              {
                flags |= (ULong) mask;
              }

              flagsMask |= (ULong) mask;	
              idaTrackTrace(("createModifyFlags: setting flag %s to %ld", name.cString(), (ULong)value));    
            }
          }
          else // missing name or value
          {
            idaTrackExcept(("createModifyFlags: name and/or value in Flag missing"));
            errorText = "name and/or value in Flag missing";
            result = isNotOk;
          }          
        } // end if ( flagNodeName == "Flag" )
        nextSiblingNode = PCPXML:: getNextSibling( flagNode );
        PCPXML::deleteNode (flagNode);
        flagNode = nextSiblingNode;

      } // end while (flagNode)
    } //  end if ( childNodeName == "Flags")
    nextSiblingNode = PCPXML:: getNextSibling( childNode );
    PCPXML::deleteNode (childNode);
    childNode = nextSiblingNode;
  } // end while ( childNode )

  modifyRequest.setFlags(flags);
  modifyRequest.setFlagsMask(flagsMask);

  return result;
}


Void TdfAccess::convModifyResponseToXml(ModifyResponse & modifyResponse, String & xml, String & typeString)
{
  TRACE_FUNCTION("TdfAccess::convModifyResponseToXml(...)");

  VisibleString visibleString;
  Bool isError = false;

  // -----------------------------------------------------------------------------
  // ModifyResult
  xml.cat("<ModifyResult>");

  if (modifyResponse.isErrorSourceSet())
  {
    enumToString(modifyResponse.getErrorSource(), visibleString);
    XmlHelper::createXmlNode(xml, tagErrorSource, visibleString.getDataString());
    isError = true;
  }

  if (modifyResponse.isErrorCodeSet())
  {
    //errorFlag = true;
    enumToString(modifyResponse.getErrorCode(), visibleString);
    XmlHelper::createXmlNode(xml, tagErrorCode, visibleString.getDataString());
    isError = true;
  }

  xml.cat("</ModifyResult>");

  if ( isError )
  {
    typeString.assign("error");
  }  
  else
  {
    typeString.assign("modify");
  }
}

/*static*/ ReporterClient* 
TdfAccess::reporter()
{
  return Process::getToolBox()->getReporter();
}

// *** EOF ***

