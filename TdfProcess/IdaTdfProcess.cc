//CB>-------------------------------------------------------------------
//
//   File:      IdaTdfProcess.cc
//   Revision:  1.1
//   Date:      29-SEP-2009 17:58:03
//
//   DESCRIPTION:
//
//
//<CE-------------------------------------------------------------------

static const char * SCCS_Id_TdfProcess_cc = "@(#) IdaTdfProcess.cc 1.1";

//--------------------- include files ------------------------------------
#include <stdafx.h>
#include <pcpdefs.h>
#include <IdaDecls.h>
#include <idatraceman.h>
#include <pcpprocess.h>
#include <pcptime.h>
#include <pcpstring.h>
#include <tdsresponse.h>
#include <syspar.h>
#include <stdlib.h>

#ifndef _WINDOWS
#include <unistd.h>
#include <sys/ipc.h>
#include <iostream.h>
#include <fstream.h>
#else
#include <iostream>
#include <fstream>
#endif

#include <ioformat.h>
#include <toolbox.h>
#include <reporterclient.h>
#include <sys/resman.h>

#include <IdaRequestContainer.h>

#ifdef _HPUX_11
#include <locale.h>
#endif

#include "IdaTdfProcess.h"
#include "IdaDatabaseList.h"
#include "IdaTdfAccess.h"
#include "IdaWebInterface.h"
#include <osalimits.h>
#include <IdaDecls.h>

#include <cltracecategory.h> 
#include <pcpdispatcher.h>
#include <sys/tracemanager.h>




#ifdef ALLOW_STDOUT
	#define MONITORING
#endif


    const	long	tdfproc	=	 890;


////////////////////////////////////////////////////////////////////////////////////////////////////
//
TdfProcess::TdfProcess(Int argc, Char * argv[], Char * envp[])
//    : Process (argc, argv, envp)
      : Process ( argc, argv, envp, tdfproc, "ida" )
      , _webInterface(0)
{

  // Traceausgabe initialisieren
  String traceFileName;

  // trace file name ist set by classlib Process constructor!
  // trace level is set by classlib constructor

  char *locale;
  locale = setlocale( LC_ALL, "");
  if (NULL == locale) {
    idaTrackExcept(( "unable to read locale\n"));
  }
  else {
    idaTrackData(("Locale = %s", locale));
  }



    // Reporterausgabe initialisieren
    ReporterClient* repClient = Process::getToolBox()->getReporter();
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
ReturnStatus TdfProcess::initialize(const String& parFileName)
{
  TRACE_FUNCTION("TdfProcess::initialize(...)");

	idaTrackTrace(("TdfProcess::initialize"));


    ReporterClient* repClient = Process::getToolBox()->getReporter();

	// der OSA-API vorgaukeln, dass "osaApiInit()" aufgerufen wurde
    OsaLimits::getInstance()->setOsaComErrorCode(0);

	// register ohne login erlauben
	OsaLimits::getInstance()->setDoCheck(false);


	// -------------------------------------------------------------------------
    SysParam sysParam;

    // Read declaration file
    if (sysParam.readDeclaration(idaDecls) == isNotOk)
    {
        idaTrackFatal(("TdfProcess::initialize : Error in Decl :\n%s\n%s",
                   sysParam.getErrorText().cString(),
                   sysParam.getErrorEnvironment().cString()));
		
		#ifdef MONITORING
			cout << "sysParam.readDeclaration(idaDecls) failed" << endl;
		#endif
		
		return isNotOk;
    }

    // Read parameter file

	#ifdef MONITORING
		cout << "parFileName = " << parFileName.cString() << endl;
	#endif

	idaTrackData(("parFileName = %s", parFileName.cString()));

#ifdef _WINDOWS
	s_ifstream parFile ( parFileName. cString (), std::ios::in  );
#else
  ifstream parFile ( parFileName. cString () );
#endif

    if (!parFile)
    {
		idaTrackFatal(("TdfProcess::initialize : creating infilestream failed"));
		
		#ifdef MONITORING
			cout << "creation of \"ifstream parFile()\" failed" << endl;
		#endif
		
		return isNotOk;
    }
    if (sysParam.readAllParams(parFile) == isNotOk)
    {
        idaTrackFatal(("Error reading par file %s:\n%s\n%s",
                   parFileName.cString(), sysParam.getErrorText().cString(),
                   sysParam.getErrorEnvironment().cString()));
		
		#ifdef MONITORING
			cout << "sysParam.readAllParams(parFile) failed" << endl;
		#endif
		
		return isNotOk;
    }
    parFile.close();


	// -------------------------------------------------------------------------
    // Read timer values and system limits
    ULong  registerTimer		= 0;
    ULong  deRegisterTimer		= 0;
    ULong  searchIntervalTimer	= 0;
    ULong  searchTimeout		= 0;
    ULong  statusReportTimer	= 0;
    Short  maxRegistration		= 0;
    UShort maxSearchRequest		= 0;

    SysParamGroup timerAndMaxValueGroup("TimerAndMaxValueGroup", true);
    if (sysParam.getFirstParamGroup("TimerAndMaxValueGroup", timerAndMaxValueGroup) == isOk)
    {
        if (timerAndMaxValueGroup.getParameter("register_timer", registerTimer) == isNotOk)
			registerTimer = 0;
		idaTrackData(("register_timer = %d", registerTimer));
        
		if (timerAndMaxValueGroup.getParameter("deregister_timer", deRegisterTimer) == isNotOk)
			deRegisterTimer = 0;
        
        if (timerAndMaxValueGroup.getParameter("search_interval_timer", searchIntervalTimer) == isNotOk)
			searchIntervalTimer = 0;
        
        if (timerAndMaxValueGroup.getParameter("search_timeout", searchTimeout) == isNotOk)
			searchTimeout = 0;
		idaTrackData(("search_timeout = %d", searchTimeout));
        
        if (timerAndMaxValueGroup.getParameter("statusreport_timer", statusReportTimer) == isNotOk)
			statusReportTimer = 0;
        
        if (timerAndMaxValueGroup.getParameter("max_registration", maxRegistration) == isNotOk)
			maxRegistration = 0;
		idaTrackData(("max_registration = %d", maxRegistration));
        
        if (timerAndMaxValueGroup.getParameter("max_search_request", maxSearchRequest) == isNotOk)
			maxSearchRequest = 0;
        
    }
	else
	{
		idaTrackTrace(("TdfProcess::initialize : Cannot access group \"TimerAndMaxValueGroup\""));
		#ifdef MONITORING
			cout << "Cannot access group \"TimerAndMaxValueGroup\"" << endl;
		#endif
	}




	// -------------------------------------------------------------------------
	// Basis OID für den WebProcess holen. Auf diesen wird später für jede
	// Prozeß-Instanz ein entsprechender Offset hinzu addiert.
    SysParamGroup webInterfaceGroup("WebInterfaceGroup", true);
    if (sysParam.getFirstParamGroup("WebInterfaceGroup", webInterfaceGroup) == isNotOk)
    {
        idaTrackFatal(("Parameter file %s invalid. Reason : WebInterfaceGroup missing",
                   parFileName.cString()));
		#ifdef MONITORING
			cout << "Cannot access group \"WebInterfaceGroup\"" << endl;
		#endif
		return isNotOk;
    }
	// BasePort für die Socket Kommunikation
    UShort webInterfacePort;
    if (webInterfaceGroup.getParameter("web_interface_port", webInterfacePort) == isNotOk)
    {
        idaTrackFatal(("Parameter file %s invalid. Reason : \"web_interface_port\" missing", parFileName.cString()));
        s_cerr<<"Parameter file invalid. Reason : \"web_interface_port\" missing"<<s_endl;
        return isNotOk;
    }
    idaTrackData(("Socket-Port for WebInterface: %u", webInterfacePort));
    s_cout<<"Socket-Port for WebInterface:" <<webInterfacePort<<s_endl;


    // -------------------------------------------------------------------------
	// Und jetzt wird fuer jede TdfProcessGroup ein TdfAccess-Objekt angelegt
    // Initialize attribute mapping handler
	ReturnStatus returnStatus;
    int clientCount = 1;
	SysParamGroup tdfProcessGroup("TdfProcessGroup", true);
	returnStatus = sysParam.getFirstParamGroup("TdfProcessGroup", tdfProcessGroup);
    if (returnStatus == isNotOk)
    {
		idaTrackFatal(("TdfProcess::initialize : There must exist at least one \"TdfProcessGroup\""));
		#ifdef MONITORING
			cout << "Cannot access group \"TdfProcessGroup\"" << endl;
		#endif
		return isNotOk;
    }

    // -------------------------------------------------------------------------
	// Jetzt wird das WebInterface Objekt angelegt
    _webInterface = new WebInterface(webInterfacePort, searchTimeout);
    if ( isNotOk == _webInterface->init() )
    {
		idaTrackFatal(("WebInterface initialization failed."));
        return isNotOk;
    }
    
    returnStatus = sysParam.getFirstParamGroup("TdfProcessGroup", tdfProcessGroup);
    while (returnStatus == isOk)
    {
        long	dbId				= 0;
        long	dbServerOid			= 0;
        
        UShort	noOfChannels;
        String	serviceName;
        String	applName;
        String	osaTicket;

        int		dataMissing = 0;
        
        if (tdfProcessGroup.getParameter("dbid", dbId) == isNotOk)
        {

          idaTrackFatal (("TdfProcess::initialize : parameter 'dbId' missing or syntax error in definition"));
        }

        if (tdfProcessGroup.getParameter("dbserver_objectid", dbServerOid) == isNotOk)
        {
          idaTrackFatal (("TdfProcess::initialize : parameter 'dbserver_objectid' missing or syntax error in definition"));
          dataMissing = 1;
        }

        if (tdfProcessGroup.getParameter("service_name", serviceName) == isNotOk)
        {
          idaTrackFatal (("TdfProcess::initialize : parameter 'service_name' missing or syntax error in definition"));
          dataMissing = 1;
        }
        if (tdfProcessGroup.getParameter("application_name", applName) == isNotOk)
        {
          idaTrackFatal (("TdfProcess::initialize : parameter 'application_name' missing or syntax error in definition"));
          dataMissing = 1;
        }
        if (tdfProcessGroup.getParameter("nrof_channels", noOfChannels) == isNotOk)
        {
          idaTrackFatal (("TdfProcess::initialize : parameter 'nrof_channels' id missing or syntax error in definition"));
          dataMissing = 1;
        }
        if (tdfProcessGroup.getParameter("osa_ticket", osaTicket) == isNotOk)
        {
          idaTrackFatal (("TdfProcess::initialize : parameter 'osa_ticket' id missing or syntax error in definition"));
          dataMissing = 1;
        }

        if (dataMissing)
        {
          idaTrackFatal(("TdfProcess::initialize : Data for TDF client group missing exiting"));
          return isNotOk;
        }
        else
        {
            // Anlegen der TDF_Client Instanzen
            TdfAccess* tdfAccess = new TdfAccess(*_webInterface,
												 dbId,
												 ObjectId(dbServerOid),
												 serviceName,
												 applName,
												 noOfChannels,
												 osaTicket,
												 registerTimer,
												 searchTimeout,
												 maxRegistration
												 );

			// Aktivieren der des Dispatchers zur Message-Übermittlung
            tdfAccess->enableApplicationMessageBox();
            idaTrackData(("enableApplicationMessageBox() ...  done"));

			// Wir merken uns die Instanz in einer Liste, damit sie später
			// wieder abgebaut werden kann
            tdfAccessList.push_back(tdfAccess);
            idaTrackTrace(("TDF client #%d created, DBID:%d",
                                         clientCount, dbId));
            
			// Bekanntgeben des DB zu TdfAccess mappings beim WebInterface
            _webInterface->addDb(dbId, tdfAccess);
        }

        ++clientCount;
        returnStatus = sysParam.getNextParamGroup("TdfProcessGroup", tdfProcessGroup);
    }

    initComplete();

	return isOk;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
TdfProcess::~TdfProcess()
{
    idaTrackTrace(("Destructor TdfProcess"));

    // Freigeben der TdfAccess Instanzen
    s_vector<TdfAccess*>::iterator cursor = tdfAccessList.begin();
    while (cursor != tdfAccessList.end())
    {
        delete *cursor;
        cursor++;
    }

    // Freigeben der WebInterface Instanz
    if (_webInterface)
    {
        delete _webInterface;
        _webInterface = 0;
    }

	// Den eigenen Prozess auschecken
     Long ownOid;
    if (Process::getResMan()->getOwnProcRefId(ownOid) == isNotOk)
    {
//        ownOid = noKnownId;
    }
    Process::getComMan()->dispatcherCheckOut(ownOid);
    Process::getComMan()->checkOut(ownOid);
    shutdownComplete(ownOid);


    ReporterClient* repClient = Process::getToolBox()->getReporter();
//    repClient->reportEvent(noKnownId, iDAMinRepClass, 202, " TdfProcess");
}




/*CB>---------------------------------------------------------------------

  DESCRIPTION:
        Main process loop

--------------------------------------------------------------------------
<CE*/
void TdfProcess::run()
{
  TRACE_FUNCTION("TdfProcess::run(...)");
  idaTrackTrace(("TdfProcess::run"));
  ComMan::getDispatcher()->dispatchForever();
}




// *** EOF ***





