#ifndef IdaDatabaseList_h
#define IdaDatabaseList_h

//CB>-------------------------------------------------------------------
//
//   File, Component, Release:
//                  IdaDatabaseList.h 1.1
//
//   File:      IdaDatabaseList.h
//   Revision:  1.1
//   Date:      09-JAN-2009 09:42:57
//
//   DESCRIPTION:
//
//
//<CE-------------------------------------------------------------------


static const char * SCCS_Id_IdaDatabaseList_h = "@(#) IdaDatabaseList.h 1.1";



#include <map.h>

class TdfAccess;

////////////////////////////////////////////////////////////////////////////////////////////////////
/**
	Diese Klasse speichert eine Liste von Paaren bestehend aus
	einer DatabaseID und der zugehoerigen ObjektID.
	Die Klasse wird vom WebProcess benoetigt um die Requests an das
	richtige TdfAccess Objekt senden zu koennen
*/
class DatabaseList
{
	private:
		typedef s_map < RefId, TdfAccess* > tDatabaseList;
		tDatabaseList _databaseList;

	public:
		DatabaseList() {};
		~DatabaseList() {};

		/**
		  Einen DbId / ObjectId - Paar eintragen
		  */
		Void addDb(RefId dbid, TdfAccess* tdfAccess)
		{
			// Objekt in die Liste eintragen
#ifdef OS_AIX_4
			s_pair<RefId, TdfAccess*> listItem(dbid, tdfAccess);
#else
			s_pair<const RefId, TdfAccess*> listItem(dbid, tdfAccess);
#endif
			_databaseList.insert(listItem);

		}

		/**
		  Einen TdfAccess anhand ihrer DbId finden
		  Wenn die DbId nicht vorhanden ist wird
		  NULL zurueck geliefert.
		  */
		TdfAccess* findTdfAccessOfDB(RefId dbid) const
		{
			tDatabaseList::const_iterator it = _databaseList.find(dbid);
			if (it != _databaseList.end())
			{
				return it->second;
			}
			return NULL;
		}

};


#endif	// IdaDatabaseList_h


// *** EOF ***


