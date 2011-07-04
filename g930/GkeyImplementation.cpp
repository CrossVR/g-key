
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
// Action commands for this DLL
//	
//	1) Launch a Windows calculator if it is installed and find in
//     the system at the following file path.
//     C:\WINDOWS\system32\calc.exe
//	2) Launch a Windows Solitaire game if it is installed and find 
//     in the system at the following file path.
//     C:\WINDOWS\system32\sol.exe
//	3) Launches a Windows Pinball game if it is installed and find
//     in the system at the following file path.
//		C:\Program Files\Windows NT\Pinball\PINBALL.EXE
//     
//     The file path for the applications are hardcoded in this sample
//     project and if it does not exist at the expected path, the user
//     will not see an action for that Gkey command.
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
	L"Push-to-talk (toggle)",
	L"Mute the Microphone",
	L"Unmute the Microphone",
	L"Toggle Microphone mute on/off",
	L"Mute the Speakers/Headphones",
	L"Unmute the Speakers/Headphones",
	L"Toggle Speakers/Headphones mute on/off",
	L"Turn on globally away status",
	L"Turn off globally away status",
	L"Toggle globally away status on/off",
	NULL
};

// This is the G-Key plugin command list corresponding with the
// EnglishCmdList
static char* GKeyCmdList[] = {
	"TS3_PTT_ACTIVATE",
	"TS3_PTT_DEACTIVATE",
	"TS3_INPUT_MUTE",
	"TS3_INPUT_UNMUTE",
	"TS3_INPUT_TOGGLE",
	"TS3_OUTPUT_MUTE",
	"TS3_OUTPUT_UNMUTE",
	"TS3_OUTPUT_TOGGLE",
	"TS3_AWAY_ZZZ",
	"TS3_AWAY_NONE",
	"TS3_AWAY_TOGGLE"
};

BOOL pttActive = FALSE;

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

	if(commandID>9) return FALSE; // Invalid command ID

	switch(commandID)
	{
		case 0:
			strcpy(msg.message, GKeyCmdList[(pttActive)?0:1]);
			pttActive = !pttActive;
		break;
		default:
			strcpy(msg.message, GKeyCmdList[commandID+1]);
		break;

	}

	IpcWrite(&msg, 1000);

	return TRUE;
}