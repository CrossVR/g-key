
/****************************************************************************

 Module:	GKeyDLLSample1.cpp

 Created 02/27/2010 by Len Chau
 
 Description:	Defines the entry point for the DLL module.
 
 Copyright (c) 2009-2010 Logitech, Inc.  All Rights Reserved

 This program is a trade secret of LOGITECH, and it is not to be reproduced,
 published, disclosed to others, copied, adapted, distributed or displayed
 without the prior authorization of LOGITECH.

 Licensee agrees to attach or embed this notice on all copies of the program,
 including partial copies or modified versions thereof.

****************************************************************************/
#include "stdafx.h"
#include "../ipc.h"

#ifdef _MANAGED
#pragma managed(push, off)
#endif

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	// Any initialization and setup go here
	//
	switch(ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH: IpcInit(); break;
		case DLL_PROCESS_DETACH: IpcClose(); break;
	}

    return TRUE;
}

#ifdef _MANAGED
#pragma managed(pop)
#endif

