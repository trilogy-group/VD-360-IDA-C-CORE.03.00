#ifndef IdaTypes_h
#define IdaTypes_h

//CB>-------------------------------------------------------------------
//
//   File:      IdaTypes.h
//   Revision:  1.2
//   Date:      28-MAY-2010 10:54:01
//
//   DESCRIPTION:
//     Yellow Page wide used constants and types.
//
//<CE-------------------------------------------------------------------

static const char * SCCS_Id_IdaTypes_h = "@(#) IdaTypes.h 1.2";


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


