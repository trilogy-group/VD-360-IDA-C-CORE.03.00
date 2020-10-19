//CB>-------------------------------------------------------------------
//
//   File:          productgroups.par.tpl
//   Revision:      1.1
//   Date:          20-NOV-2009 15:36:28
//
//   DESCRIPTION:
//   Template for a parameter file for the supervisor's processgroups
//   specification
//
//<CE-------------------------------------------------------------------


Configuration
BEGIN
   netProcess="true"
   reporterProcess="true"
   routerServer="false"
   adminServer="true"
END

Group SET
BEGIN
    groupName        = "ida"
    parameterFile    = "${IDA_WORKDIR_PATH}/config/idaprocs.par"
    startNo          = #1
    autoStartSuccess = "dontcare"
    shutdown         = "explicit"
END
ENDSET

