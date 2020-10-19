#ifndef IdaDecls_h
#define IdaDecls_h
//CB>-------------------------------------------------------------------
//
//   File:      IdaDecls.h
//   Revision:  1.1
//   Date:      29-SEP-2009 17:58:02
//
//   DESCRIPTION:
//     Declaration of variables used in IDA main parameter file.
//
//<CE-------------------------------------------------------------------
static const char * SCCS_Id_IdaDecls_h = "@(#) IdaDecls.h 1.1";

#ifdef IDADLL   
#ifdef IDADLLEXPORT
#define IDADECLSPEC	__declspec(dllexport)
#else
#define IDADECLSPEC	__declspec(dllimport)
#endif
#else
#define IDADECLSPEC
#endif


#ifndef _WINDOWS
#include <strstream.h>
#else
#include <strstream>
#endif

#include <pcpstring.h>
//#include <pcptrace.h>
#include <reporttypes.h>


// An dieser Stelle kann Ausgabe nach stdout via "cout" komplett
// also für WebProcess und TdfProcess abgeschaltet werden
// #define ALLOW_STDOUT


#define FUNCTION_TRACING

#ifdef FUNCTION_TRACING
#define TRACE_FUNCTION(x) FunctionTrace xXxXxX(x)
#else
#define TRACE_FUNCTION(x) ;
#endif

#define PROBLEM(a,b,c,d) reporter()->reportProblem((a),(b),(c),(d))
#define ALARM(a,b,c,d) reporter()->reportAlarm((a),(b),(c),(d))
#define EVENT(a,b,c,d) reporter()->reportEvent((a),(b),(c),(d))




// Bezeichner genau wie in "ida.par" inkl. Reihenfolge  !
static s_istrstream idaDecls
(
	"TdfProcessGroup "			// TdsProcess
		"DECLARE "
		"dbid; "
		"dbserver_objectid; "
		"service_name; "
		"application_name; "
		"nrof_channels; "
		"osa_ticket; "
		"ENDDECL "
	
	"TimerAndMaxValueGroup "	// Timer, MaxValue, etc.
		"DECLARE "
		"register_timer; "
		"deregister_timer; "
		"search_intervall_timer; "
		"search_timeout; "
		"statusreport_timer; "
		"max_registration; "
		"max_search_request; "
		"ENDDECL "
	
	"WebInterfaceGroup "			// WebInterface
		"DECLARE "
		"web_interface_port; "
		"ENDDECL "
	
);





struct SesConfig
{
	enum SesConfigMode
	{
		NONE	= 0,
		DEFAULT,
		NORMAL
	};

	long			loSesClientOID;
	SesConfigMode	enumMode;
	String			stDefaultUser;
	String			stDefaultPwd;
	int				iTicketId;

	//ctor
	SesConfig(SesConfigMode mode) : enumMode(mode) {}
};

static const char* szSesConfigModeNONE		= "NONE";
static const char* szSesConfigModeDEFAULT	= "DEFAULT";
static const char* szSesConfigModeNORMAL	= "NORMAL";


#endif	// IdaDecls_h

// *** EOF ***


