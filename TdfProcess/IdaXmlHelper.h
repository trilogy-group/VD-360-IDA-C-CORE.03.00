#ifndef IdaXmlHelper_h
#define IdaXmlHelper_h

#include <pcpdefs.h>
#include <pcpstring.h>

class XmlHelper
{
public:
  static String requestTerminationTag1;
  static String requestTerminationTag2;
  static String requestTerminationTag3;
  static String requestTerminationTag4;
  static String requestTerminationTag5;
  static String requestStartTag1;
  static String requestStartTag2;
  static String requestStartTag3;
  static String requestStartTag4;
  static String requestStartTag5;

  /** Erzeugt ein XML Dokument mit Fehlerbeschreibung und legt es in xmlString ab.
      errorText ist der einzubauende Fehlertext */
  static ReturnStatus formatXMLErrorResponse(
            String& xmlString,
            const char* errorSource,
            const char* errorCode,
            const char* errorText);

  /** Erzeugt ein volständiges XML-Element mit dem Inhalt val */
  static ReturnStatus createXmlNode(String& xmlString, const String& tag, const char* val);
  static ReturnStatus createXmlNode(String& xmlString, const String& tag, const Int val);
  static ReturnStatus createXmlNode(String& xmlString, const String& tag, const ULong val);
};

#endif // IdaXmlHelper_h
