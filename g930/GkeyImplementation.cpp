
/****************************************************************************

 Module:	GkeyImplementation.cpp

 Created 02/27/2010 by Len Chau
 
 Description:	Implementation of the Logitech gaming headset G-key DLL.
 
 Copyright (c) 2009-2010 Logitech, Inc.  All Rights Reserved

 This program is a trade secret of LOGITECH, and it is not to be reproduced,
 published, disclosed to others, copied, adapted, distributed or displayed
 without the prior authorization of LOGITECH.

 Licensee agrees to attach or embed this notice on all copies of the program,
 including partial copies or modified versions thereof.

****************************************************************************/

//==========================================================================
// 
// This file contain the 2 main interface functions for the headset
// application to use this library. The interface functions are:
//
//	WCHAR**		GetGkeyCommandList(unsigned int languageCode)
//	BOOL		RunGkeyCommand (unsigned int commandID)
//
// All other functions are local to the library. 
//
//=========================================================================

#include "stdafx.h"
#include <tchar.h>
#include <windows.h>
#include <shellapi.h>				// support shell execution
#include "GkeyImplementation.h"
#include "../ipc.h"

// This is an action conmmand list supported by this application 
// module in English language.
// A NUll terminated list must be placed at the end of a command list.
//
static WCHAR* EnglishCmdList[] = {
	L"Toggle Push-to-talk",
	L"Activate Push-to-talk",
	L"Deactivate Push-to-talk",
	L"Toggle Microphone mute on/off",
	L"Mute the Microphone",
	L"Unmute the Microphone",
	L"Toggle Speakers/Headphones mute on/off",
	L"Mute the Speakers/Headphones",
	L"Unmute the Speakers/Headphones",
	L"Toggle globally away status on/off",
	L"Turn on globally away status",
	L"Turn off globally away status",
	NULL
};

// This is the G-Key plugin command list corresponding with the
// EnglishCmdList
static char* GKeyCmdList[] = {
	"TS3_PTT_TOGGLE",
	"TS3_PTT_ACTIVATE",
	"TS3_PTT_DEACTIVATE",
	"TS3_INPUT_TOGGLE",
	"TS3_INPUT_MUTE",
	"TS3_INPUT_UNMUTE",
	"TS3_OUTPUT_TOGGLE",
	"TS3_OUTPUT_MUTE",
	"TS3_OUTPUT_UNMUTE",
	"TS3_AWAY_TOGGLE",
	"TS3_AWAY_ZZZ",
	"TS3_AWAY_NONE"
};

//---------------------------------------------------------------------
// GetGkeyCommandList - This function interfaces with the Logitech 
//                      headset software.
//	This function checks a language code and returns a command list for 
//	a requesting language. Only English command list is used in this
//	example.

// Parameters:
//  languageCode - an unsigned value for a language code that is
//                 currently used by the Logitech software. The valid 
//                 language code is one of the value defined below:
// 
//	 	languageCode    Languages 
//		1028			Chinese (Traditional)
//		1029			Czech
//		1030			Danish
//		1031			German
//		1033/default	English
//		1034			Spanish(Spain)
//		1035			Finnish
//		1036			French
//		1038			Hungarian
//		1040			Italian
//		1041			Japanese
//		1042			Korean
//		1043			Dutch
//		1044			Norwegian
//		1045			Polish
//	    1046            Portuguese(Brazil)
//		1053			Swedish
//		2070			Portuguese(Portugal)
//		1049			Russian
//		2052			Chinese (Simplified)
//
//--------------------------------------------------------------------- 
WCHAR** GetGkeyCommandList(unsigned int languageCode)
{
	return EnglishCmdList;  // This application only support English language.
}

//---------------------------------------------------------------------
// RunGkeyCommand - This function interfaces with the Logitech headset 
//                  software.
//	This function executes a command that requested from the headset 
//  software.
//
// Parameters:
//	commandID - a command ID is a location of a command in the command
//              list. A valid value range from 0..n-1. 0 is a first 
//              command in a command list and n-1 is a last command in
//              the command list.
//---------------------------------------------------------------------
BOOL RunGkeyCommand (unsigned int commandID)
{
	IpcMessage msg;
	strcpy(msg.message, GKeyCmdList[commandID]);
	return IpcWrite(&msg, 1000);
}