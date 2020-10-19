
//CB>---------------------------------------------------------------------------
//
//   File:      IdaTdfProcessMain.cc
//   Revision:  1.1
//   Date:      28-MAY-2010 10:54:00
//
//<CE---------------------------------------------------------------------------

static const char * SCCS_Id_IdaTdfProcessMain_cc = "@(#) IdaTdfProcessMain.cc 1.1";


//--------------------- include files ------------------------------------
#include <pcpstring.h>
#include <IdaDecls.h>
#include <IdaTdfProcess.h>
#include <stdlib.h>
#include <errno.h>
// #include <dl.h>

#ifndef _WINDOWS
#include <iostream.h>
#include <fstream.h>
#else
#include <iostream>
#include <fstream>
#endif


#ifdef ALLOW_STDOUT
	#define MONITORING
#endif



int main(int argc, char** argv, char** envp)
{
	s_cerr << "TdfProcess started ..." << s_endl;
	
	// Check parameter
   String par1( argv[1] );
	if ( (argc < 1 ) || ( (argc < 3) && (par1 == "115") ) )
	{
		s_cerr << "error: missing arguments" << s_endl;
		s_cerr << "Usage: " << argv[0] << " <ParFile> "
			 << "[ -tr <TraceLevel> -tf <TraceFile> ]" << s_endl;
		exit(1);
	}

   String parFileName;
   
	// Extract Parameter- and Declaration-Filename
     parFileName = String(argv[1]);

	// Instantiate process object
	TdfProcess tdfProcess(argc, argv, envp);


	// Wenn die Initialisierung ok ist ...
	if (tdfProcess.initialize(parFileName) == isOk)
	{
		// ... dann können wir die Instanz starten
		tdfProcess.run();
	}

/*
#ifdef _HPUX_
	if (Data)
	{
		dlclose(Data);
	}
#endif
*/


	return 0;
}


// *** EOF ***
