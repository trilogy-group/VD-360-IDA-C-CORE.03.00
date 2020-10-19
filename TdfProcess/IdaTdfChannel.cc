
//CB>---------------------------------------------------------------------------
//
//   File, Component, Release:
//                  IdaTdfChannel.cc 1.1
//
//   File:      IdaTdfChannel.cc
//   Revision:  1.1
//   Date:      09-JAN-2009 09:42:57
//
//   DESCRIPTION:
//     Representation of one TDF channel.
//
//
//
//<CE---------------------------------------------------------------------------

static const char * SCCS_Id_IdaTdfChannel_cc = "@(#) IdaTdfChannel.cc 1.1";




#include <IdaDecls.h>
#include <idatraceman.h>
#include <IdaTdfChannel.h>

#ifdef ALLOW_STDOUT
	#define MONITORING
#endif


////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Defaultkonstruktor
//
TdfChannel::TdfChannel()
    : number	     (255),
	  status         (available)
{
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Konstruktor mit Angabe der Channel - Nummer
//
TdfChannel::TdfChannel(UShort no)
    : number         (no),
      status         (available)
{
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Destruktor
//
TdfChannel::~TdfChannel()
{
	// nothing
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Die Methode versucht belegen den Channel und setzt den Zeitstempel
//
ReturnStatus TdfChannel::reserve()
{
	// Testen, ob wir den Channel belegen k�nnen
    if (status == available)
	{
		status = reserved;
		lastAccessTime = PcpTime();	// Zeitstempel setzen
		return isOk;

	}
    return isNotOk;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Gibt den Channel frei
//
ReturnStatus TdfChannel::release()
{
    if (status == reserved)
    {
        status = available;
		return isOk;
    }
	return isNotOk;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Schaltet den Channel ab
// 
ReturnStatus TdfChannel::disable()
{
    if (status != reserved)
	{
		status = disabled;
		return isOk;
	}
	return isNotOk;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Schaltet den Channel wieder ein
// 
ReturnStatus TdfChannel::enable()
{
    if (status == disabled)
	{
		status = available;
		return isOk;
	}
	return isNotOk;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
void TdfChannel::dump(s_ostream & out) const
{
    out << "------ Channel ---------"                   << s_endl;
    out << "number        : "         << number         << s_endl;
    out << "lastAccessTime: "         << lastAccessTime << s_endl;
    out << "status        : "         << status         << s_endl;
    out << "------------------------"                   << s_endl;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
int operator == (const TdfChannel& ch1, const TdfChannel& ch2)
{
    return (ch1.number == ch2.number);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
int operator < (const TdfChannel& ch1, const TdfChannel& ch2)
{
    return (ch1.number < ch2.number);
}



// *** EOF ***


