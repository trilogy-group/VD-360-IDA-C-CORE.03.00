//CB>-------------------------------------------------------------------
//
//   File, Component, Release:
//						  templates\idaprocs.par.tpl 1.0 12-APR-2008 18:52:11 DMSYS
//
//   File:      templates\idaprocs.par.tpl
//   Revision:      1.0
//   Date:          12-APR-2008 18:52:11
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

// "@(#) templates\idaprocs.par.tpl 1.0 12-APR-2008 18:52:11 DMSYS"

Process SET
BEGIN
	name        = "${IDA_ROOT}/modules/IdaTdfProcess"
	args        = "\"${IDA_ROOT}/config/ida.par\" -tf \"${IDA_ROOT}/log/IdaTdfProcess\""
	groupName   = "ida"
	controlMode = "supervisor"
	processId   = #890
	startNo     = #1
	autoStart   = "true"
END
ENDSET
