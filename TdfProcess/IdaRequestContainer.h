#ifndef IdaRequestContainer_h
#define IdaRequestContainer_h

//CB>---------------------------------------------------------------------------
// 
//   File:      IdaRequestContainer.h
//   Revision:  1.2
//   Date:      28-MAY-2010 10:53:56
// 
//<CE---------------------------------------------------------------------------

static const char * SCCS_Id_IdaRequestContainer_h = "@(#) IdaRequestContainer.h 1.2";

#include <pcpdefs.h>
#include <pcptime.h>
#include <objectid.h>

class TdfArgument;


////////////////////////////////////////////////////////////////////////////////////////////////////
/**
	Die folgende Klasse b�ndelt alle relevanten Informationen eines Requests 
	zu einem Request-Container.
	Sie wird von der Klasse "RequestList" verwendet, in der alle Requests w�hrend
	ihrer teilweise mehrstufigen Verarbeitung abgelegt/verwaltet werden.
*/
class RequestContainer
{

	public:
		// Art der Anfrage
		enum RequestMode
		{
			login			= 0,
			logout			= 1,
			changePwd		= 2,
			request			= 3
		};

		// Zust�nde des Request-Containers
		enum RequestStatus
		{
			notReady					= 0,		// Anfangszustand
			authenticationRequired		= 1,		// bereit f�r Authentifizierung
			waitingForAuthentication	= 2,		// Authent. angefordert, wartet auf Antwort
			readyToSend					= 3,		// bereit gesendet zu werden
			sendAndWaiting				= 4			// gesendet und wartet auf Antwort
		};


	/** Equal operator */
	friend int operator == ( const RequestContainer& d1,
							 const RequestContainer& d2 );

	/** Less than operator */
	friend int operator < ( const RequestContainer& d1,
							const RequestContainer& d2 );

	public:
		RequestContainer();
		
		/** Konstruktor zur Erzeugung eines RequestContainers */
		RequestContainer(UShort			reqId);

		/** Destruktor */
		~RequestContainer();

		// ---------------------------------------------------------------------

		/** Methode zur Pr�fung, ob der Request veraltet ist */
		Bool			expired() const;

		// get-Methoden f�r alle Member
		RequestMode		getMode()			const	{ return mode;			};
		RequestStatus	getStatus()			const	{ return status;		};
		UShort			getRequestId()		const	{ return requestId;		};
		const String&	getUser()			const	{ return user;			};
		const String&	getPasswd()			const	{ return passwd;		};
		const String&	getNewPasswd()		const	{ return newPasswd;		};
		TdfArgument*	getTdfArgument()	const	{ return tdfArgument;	};
		UShort			getChannel()		const	{ return channel;		};
		ULong			getTimeout()		const	{ return timeout;		};
		RefId			getTimerId()		const	{ return timerId;		};
		const PcpTime&		getBirth()			const	{ return birth;			};

		Void			setMode(RequestMode rm)
							{ mode = rm; }
		Void			setStatus(RequestStatus rs)
							{ status = rs; }
		Void			setUser(const char *  usr)
							{ user = String(usr); }
		Void			setPasswd(const char *  pwd)
							{ passwd = String(pwd); }
		Void			setNewPasswd(const char *  npw)
							{ newPasswd = String(npw); }
		Void			setTdfArgument(TdfArgument* ta)
							{ tdfArgument = ta; /*checkStatus();*/ }
		Void			setChannel(UShort ch)
							{ channel = ch; /*checkStatus();*/ }
		Void			setTimeout(ULong to)
							{ timeout = to; /*checkStatus();*/ }
		Void			setTimerId(RefId ti)
							{ timerId = ti; /*checkStatus();*/ }

		Void			checkStatus();
		


	private:

		/** Art des Requestes (Login, Passwort�nderung oder DB-Anfrage) */
		RequestMode		mode;

		/** Aktueller Status des Requestes */
		RequestStatus	status;

		/** Eindeutige ID des Requests - wird vom WebProcess bereits geliefert */
		UShort			requestId;

		/** User Name, wenn SES Authentifizierung aktiviert */
		String			user;

		/** Passwort, wenn SES Authentifizierung aktiviert */
		String			passwd;

		/** Neues Passwort, bei Passwort-�nderung */
		String			newPasswd;
		
		/** Request fertig aufbereitet */
		TdfArgument*	tdfArgument;
		
		/** Channel, auf dem gesendet wurde */
		UShort			channel;
		
		/** Verfallszeit in Milli-Sekunden */
		ULong			timeout;
		
		/** ID des Timers, der f�r diesen Request gestartet wurde. */
		RefId			timerId;
		
		/** Entstehungszeitpunkt des Objektes */
		PcpTime			birth;
};

inline int operator == ( const RequestContainer& d1,
						 const RequestContainer& d2 )
{
	return(d1.requestId == d2.requestId);
}

inline int operator < ( const RequestContainer& d1,
						const RequestContainer& d2 )
{
	return(d1.requestId < d2.requestId);
}

#endif	// IdaRequestContainer_h


// *** EOF ***


