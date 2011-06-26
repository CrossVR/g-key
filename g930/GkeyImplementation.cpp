
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

// This is an action conmmand list supported by this application 
// module in English language.
// A NUll terminated list must be placed at the end of a command list.
//
static WCHAR* EnglishCmdList[] =	{ L"Calculator",
									  L"Solitaire Game",
							          L"Pinball Game",
							          NULL
							        };

//-----------------------------------------------------------------
// DebugTRACE
//		This function is used to print a debug information. The
//      function is a local to this module.
//
// Parameters:
//	pszFormat - an output format string likes the format use in the 
//              printf() statement
//  ...       - the parameter(s) whose value will be output based on
//              the specified format.(optional)
//        
//-----------------------------------------------------------------

#ifdef _DEBUG
void DebugTRACE(LPTSTR pszFormat, ...)
{
	WCHAR szBuffer[1024];
	va_list pArgList;

	va_start(pArgList, pszFormat);
	_vsntprintf(szBuffer, sizeof(szBuffer) / sizeof(WCHAR), pszFormat, pArgList);
	va_end(pArgList);

	OutputDebugString(szBuffer);
}
#endif

//---------------------------------------------------------------------
// ExecuteCmd - This function is a local to this module.
//		This function uses a Windows ShellExecute function to execute a
//		requested command.
//
// Parameters:
//		szCmdPath - a command line to be passed to the ShellExecute
//                  function.
//      szPars - additional parameters string to the ShellExecution 
//               function.               
//---------------------------------------------------------------------
void ExecuteCmd(const WCHAR* szCmdPath, const WCHAR* szPars)
{
	HINSTANCE hInst = ::ShellExecute(NULL, L"open", szCmdPath, szPars, 0, SW_SHOWNORMAL);
}

//---------------------------------------------------------------------
// LaunchPinball - This function is a local to this module.
//		This function calls a ExecuteCmd function to launch a pin ball 
//      game. Assume that the game is installed in a location:
//      C:\Program Files\Windows NT\Pinball\PINBALL.EXE in a system. 
//      If the game does not exist, the execution will fail and user
//
//      will not see the action.
// Parameters:
//	none
//---------------------------------------------------------------------
void LaunchPinball(void)
{
	ExecuteCmd(L"C:\\Program Files\\Windows NT\\Pinball\\PINBALL.EXE", NULL);
}

//---------------------------------------------------------------------
// LaunchCal - This function is a local to this module.
//	This function calls a ExecuteCmd function to launch a calculator.
//  Assume that a Windows calculator is installed in a location:
//  C:\WINDOWS\system32\calc.exe.
//
// Parameters:
//	none
//---------------------------------------------------------------------
void LaunchCal(void)
{
	ExecuteCmd(L"C:\\WINDOWS\\system32\\calc.exe", NULL);
}

//---------------------------------------------------------------------
// LaunchSolitaire	- This function is a local to this module.
//	This function calls a ExecuteCmd function to launch a Solitaire game.
//  Assume that a Windows calculator is installed in a location:
//  C:\WINDOWS\system32\calc.exe. 
//
// Parameters:
//	none
//---------------------------------------------------------------------
void LaunchSolitaire(void)
{
	ExecuteCmd(L"C:\\WINDOWS\\system32\\sol.exe", NULL);
}

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
	if (languageCode == 1033) {
#ifdef _DEBUG
		DebugTRACE(_T("GetGkeyCommandList() languageCode=%d is English\n"), languageCode);
#endif
	}
	else {
#ifdef _DEBUG
		DebugTRACE(_T("GetGkeyCommandList() languageCode=%d other than English\n"), languageCode);
#endif
	}

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
	BOOL retVal = TRUE;

	switch (commandID)
	{
	case 0: LaunchCal();
		break;
	case 1: LaunchSolitaire();
		break;
	case 2: LaunchPinball();
		break;
	default:
#ifdef _DEBUG
		DebugTRACE(_T("Unsupported commnad code: %d.\n"), commandID);
#endif
		retVal = FALSE;
		break;
	};

	return retVal;
}