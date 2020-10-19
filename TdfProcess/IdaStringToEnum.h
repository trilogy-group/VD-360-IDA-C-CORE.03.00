
















    //////////////////////////////////////////
    //                                      //
    //  THIS FILE IS AUTOMATICLY GENERATED  //
    //                                      //
    //  ****  DO NOT MODIFY DIRECTLY  ****  //
    //                                      //
    //         (modify gencode.pl)          //
    //                                      //
    //////////////////////////////////////////




















//CB>---------------------------------------------------------------------------
//
//   File, Component, Release:
//                  TdfProcessgencode.pl 1.0 12-APR-2008 18:52:13 DMSYS
//
//   File:      TdfProcessgencode.pl
//   Revision:      1.0
//   Date:          12-APR-2008 18:52:13
//
//   DESCRIPTION:
//     
//     This modul converts a string into the int value of the enum declaration
//     with the same identifier
//
//
//<CE---------------------------------------------------------------------------

static const char * SCCS_Id_IdaStringToEnum_h = "@(#) TdfProcessgencode.pl 1.0 12-APR-2008 18:52:13 DMSYS";


// You can not instanciate this class

class StringToEnum
{
   private:
      StringToEnum();
      ~StringToEnum();

   public:
   
      enum __ret_value
	  {
	     invalid = 0xFFFFFFFF
      };

	static ReturnStatus enumOfOperation(ULong& enumValue, const char* strValue);
	static ReturnStatus enumOfModifyOperation(ULong& enumValue, const char* strValue);
	static ReturnStatus enumOfAttributeId(ULong& enumValue, const char* strValue);
	static ReturnStatus enumOfSearchAttr(ULong& enumValue, const char* strValue);
	static ReturnStatus enumOfProtocol(ULong& enumValue, const char* strValue);
	static ReturnStatus enumOfSpecialAddress(ULong& enumValue, const char* strValue);
	static ReturnStatus enumOfProduct(ULong& enumValue, const char* strValue);
	static ReturnStatus enumOfLanguage(ULong& enumValue, const char* strValue);
	static ReturnStatus enumOfCharacterSet(ULong& enumValue, const char* strValue);
	static ReturnStatus enumOfSearchType(ULong& enumValue, const char* strValue);
	static ReturnStatus enumOfSearchVar(ULong& enumValue, const char* strValue);
	static ReturnStatus enumOfExpansion(ULong& enumValue, const char* strValue);
	static ReturnStatus enumOfRequestedFormat(ULong& enumValue, const char* strValue);
	static ReturnStatus enumOfDbAttribute(ULong& enumValue, const char* strValue);
};

 
// *** EOF ***

