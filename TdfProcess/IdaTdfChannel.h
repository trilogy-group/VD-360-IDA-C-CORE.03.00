#ifndef IdaTdfChannel_h
#define IdaTdfChannel_h

//CB>-------------------------------------------------------------------
// 
//   File:      IdaTdfChannel.h
//   Revision:  1.1
//   Date:      28-MAY-2010 10:53:58
// 
//   DESCRIPTION:
//     Representation of one TDF channel within IDA TDF client.
//     
//     
//<CE-------------------------------------------------------------------

static const char * SCCS_Id_IdaTdfChannel_h = "@(#) IdaTdfChannel.h 1.1";

#include <pcpdefs.h>
#ifdef _HPUX_10
	#include <iostream.h>
#endif


#include <pcptime.h>

#ifndef _LINUX 
#ifndef _MSC90 
   class ostream;
#endif
#endif

class TdfChannel
{

	public :
	
		enum Status
		{
			available,		// frei, benutzbar
			reserved,		// gerade in Benutzung
			disabled		// nicht verfügbar (evtl. temporär)
		};
	
		// ---------------------------------------------------------------------
	
		/** Default constructor */
		TdfChannel();
	
		/** Constructor */
		TdfChannel(UShort no);
	
		/** Destructor */
		~TdfChannel();
	
		// ---------------------------------------------------------------------
	
		/** Get number of registered channels */
		UShort getNumber() const			{ return number;			}
	
		/** Get timestamp of last channel access */
		const PcpTime&  getLastAccessTime() const	{ return lastAccessTime;	}
	
		/** Get channel status */
		Status getStatus() const			{ return status;			}
	
		// ---------------------------------------------------------------------

		/** Reserve this channel */
		ReturnStatus	reserve();
	
		/** Free channel */
		ReturnStatus	release();
	
		/** Deactivates channel. */
		ReturnStatus	disable();
	
		/** Activates channel after disabling */
		ReturnStatus	enable();
	
		// ---------------------------------------------------------------------
		
		/** Dump channel info */
		void dump(s_ostream& out) const;
	
		/** Equal operator */
		friend int operator == ( const TdfChannel& ch1,
								 const TdfChannel& ch2 );
	
	
		/** Less than operator */
		friend int operator < ( const TdfChannel& ch1,
								const TdfChannel& ch2 );
	
	private :
		/** Number of the channel which is represented by this object */
		UShort	number;
		/** Timestamp of the last access */
		PcpTime	lastAccessTime;
		/** Actual status of the channel */
		Status	status;

};

#endif	// IdaTdfChannel_h


// *** EOF ***


