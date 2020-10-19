//CB>-------------------------------------------------------------------
//
//   File:      IdaXmlHelper.cc
//   Revision:  1.3
//   Date:      11-AUG-2010 15:47:58
//
//   DESCRIPTION:
//      Do some standard xml stuff
//
//<CE-------------------------------------------------------------------

static const char * SCCS_Id_IdaXmlHelper_cc = "@(#) IdaXmlHelper.cc 1.3";

#include "IdaXmlHelper.h"
#include <IdaDecls.h>
#include <idatraceman.h>
#include <IdaXmlTags.h>

/*static*/ String XmlHelper::requestTerminationTag1("</Request>");
/*static*/ String XmlHelper::requestTerminationTag2("</Login>");
/*static*/ String XmlHelper::requestTerminationTag3("</Logout>");
/*static*/ String XmlHelper::requestTerminationTag4("</ChangePwd>");
/*static*/ String XmlHelper::requestTerminationTag5("</ModifyRequest>");
/*static*/ String XmlHelper::requestStartTag1("<Request>");
/*static*/ String XmlHelper::requestStartTag2("<Login>");
/*static*/ String XmlHelper::requestStartTag3("<Logout>");
/*static*/ String XmlHelper::requestStartTag4("<ChangePwd>");
/*static*/ String XmlHelper::requestStartTag5("<ModifyRequest>");
    
////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Die Methode erzeugt aus einem als Text übergebenen Fehler ein gültiges 
//	XML Dokument
//
/*static*/ ReturnStatus 
XmlHelper::formatXMLErrorResponse(String& xmlString,
                                  const char* errorSource,
                                  const char* errorCode,
                                  const char* errorText)
{
    TRACE_FUNCTION("XmlHelper::formatXMLErrorResponse(...)");
	
	
	xmlString.cat(tagHEADER);

	xmlString.cat(tagoOsaResponse);
	xmlString.cat(" type=\"error\">\n");
	
	xmlString.cat(tagoSearchResult);

	createXmlNode(xmlString, tagErrorSource, errorSource);
	createXmlNode(xmlString, tagErrorCode, errorCode);
	createXmlNode(xmlString, tagErrorText, errorText);

	xmlString.cat(tagcSearchResult);
	xmlString.cat(tagcOsaResponse);

	return isOk;
}


/////////////////////////////////////////////////////////////////////////
//
ReturnStatus XmlHelper::createXmlNode(String& xmlString, const String& tag, const char* val)
{
//	TRACE_FUNCTION("XmlHelper::createXmlNode(...)");


	xmlString.cat(tagStart);
	xmlString.cat(tag);
	xmlString.cat(tagEnd);

	// an dieser Stelle muessen wir einige Zeichen maskieren

    // DE_MR_5606, cp, 2010-08-05: increased saveCounter (was 255)
	Int saveCounter = 4096;
	for (char* value = (char *)val; *value; value++)
	{
		switch (*value)
		{
			case '<':
				xmlString.cat(tagLT);
				break;
			case '>':
				xmlString.cat(tagGT);
				break;
			case '&':
				xmlString.cat(tagAMP);
				break;
			case '\'':
				xmlString.cat(tagAPOS);
				break;
			case '\"':
				xmlString.cat(tagQUOT);
				break;
			default:
				xmlString.cat(*value);
		}
		// Zur Sicherheit, dass keine Endlosschleife enstehen kann
		if (!--saveCounter)	break;
	}

	xmlString.cat(tagcStart);
	xmlString.cat(tag);
	xmlString.cat(tagEndNL);

	return isOk;
}

/////////////////////////////////////////////////////////////////////////
//
ReturnStatus XmlHelper::createXmlNode(String& xmlString, const String& tag, const Int val)
{
	TRACE_FUNCTION("TdfAccess::createXmlNode(...)");

	char buffer[32];
	sprintf(buffer, "%d", val);
	createXmlNode(xmlString, tag, (const char*)buffer);

	return isOk;
}


/////////////////////////////////////////////////////////////////////////
//
ReturnStatus XmlHelper::createXmlNode(String& xmlString, const String& tag, const ULong val)
{
	TRACE_FUNCTION("TdfAccess::createXmlNode(...)");

	char buffer[32];
	sprintf(buffer, "%u", val);
	createXmlNode(xmlString, tag, (const char*)buffer);

	return isOk;
}

