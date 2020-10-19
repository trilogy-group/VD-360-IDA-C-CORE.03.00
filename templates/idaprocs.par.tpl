//CB>-------------------------------------------------------------------
//
//   File:          idaprocs.par.tpl
//   Revision:      1.2
//   Date:          19-NOV-2009 15:16:25
//
//   DESCRIPTION:
//
//   #############################################################
//   #     Licensed material - property of Volt Delta GmbH       #
//   #              Copyright (c) Volt Delta GmbH                #
//   #############################################################
//
//
//<CE-------------------------------------------------------------------

Process SET
BEGIN
	name        = "${IDA_ROOT}/modules/IdaTdfProcess"
	args        = "\"${IDA_WORKDIR_PATH}config/ida.par\" -tf \"${TRACE_DIR}IdaTdfProcess\""
	groupName   = "ida"
	controlMode = "supervisor"
	processId   = #890
	startNo     = #1
	autoStart   = "true"
	restartable = #20
END
ENDSET
