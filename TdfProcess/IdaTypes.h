#ifndef IdaTypes_h
#define IdaTypes_h

//CB>-------------------------------------------------------------------
//
//   File, Component, Release:
//                  IdaTypes.h 1.1
//
//   File:      IdaTypes.h
//   Revision:  1.1
//   Date:      09-JAN-2009 09:42:55
//
//   DESCRIPTION:
//     Yellow Page wide used constants and types.
//
//
//<CE-------------------------------------------------------------------

static const char * SCCS_Id_IdaTypes_h = "@(#) IdaTypes.h 1.1";


class Types
{
	private:
		
		// Class is uninstantiable
		Types () {}
	

	public:
		
		// Miscellaneous defines
		enum MaximumLength
		{
			MAX_LEN_QUERY_STRING	= 16384
		};

};

#endif	// IdaTypes_h

// *** EOF ***


