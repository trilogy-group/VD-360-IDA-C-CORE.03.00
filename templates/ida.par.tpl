//CB>---------------------------------------------------------------------
//
//  File:      ida.par.tpl
//  Revision:  1.1
//  Date:      08-AUG-2008 15:46:56
//
//  DESCRIPTION:
//    ida.par parameter file
//
//--------------------------------------------------------------------------
//<CE

// -------------------------------------------------------------------------------
// Please follow these instructions:
// - There must be one TdsClientGroup for every DAP
// - Define one subgroup for every TdfAccess-Object.
// - Maximum number of TdfAccess-Objects is 20 !!!
// - The TdfAccess-Objects will be automaticly enumerated starting at "1"
TdfProcessGroup 
SET
BEGIN
  dbid = #1				                     // Database identification number
  dbserver_objectid = #@DB_OID@              // Database object id
  service_name = "@SERVICE_NAME@"
  application_name = "360 Web"
  nrof_channels = #20
  osa_ticket = "${IDA_OSA_TICKET}" 			// default is to set env $IDA_OSA_TICKET 
                                            // to the osa ticket file in the servlet config (including path)
END
ENDSET
// -------------------------------------------------------------------------------
TimerAndMaxValueGroup
SET
BEGIN
  register_timer         = #60000       // Registrationtimer in [ms]
  search_timeout         = #10000       // Timer for queries in [ms]
  max_registration       = #-1          // Maximum number of registration attempts (-1 is unlimited number of attempts)
END
ENDSET
// -------------------------------------------------------------------------------
WebInterfaceGroup
SET
BEGIN
//  web_interface_port = #5590                 // TCP port nummber for communication with ida servlets
  web_interface_port = #@WEB_INTERFACE_PORT@   // TCP Portnummber for communication with ida servlets
END
ENDSET



