//CB>-------------------------------------------------------------------
//
//   File:          idaprocs.par.tpl
//   Revision:      1.2
//   Date:          05-DEC-2009 20:16:49
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
END
ENDSET
