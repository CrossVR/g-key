
/****************************************************************************

 Module:	stdafx.h

 Created 02/27/2010 by Len Chau
 
 Description:	Project standard system include header file
 
 Copyright (c) 2009-2010 Logitech, Inc.  All Rights Reserved

 This program is a trade secret of LOGITECH, and it is not to be reproduced,
 published, disclosed to others, copied, adapted, distributed or displayed
 without the prior authorization of LOGITECH.

 Licensee agrees to attach or embed this notice on all copies of the program,
 including partial copies or modified versions thereof.

****************************************************************************/

// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

// Modify the following defines if you have to target a platform prior to the ones specified below.
// Refer to MSDN for the latest info on corresponding values for different platforms.
#ifndef WINVER				// Allow use of features specific to Windows XP or later.
#define WINVER 0x0501		// Change this to the appropriate value to target other versions of Windows.
#endif

#ifndef _WIN32_WINNT		// Allow use of features specific to Windows XP or later.                   
#define _WIN32_WINNT 0x0501	// Change this to the appropriate value to target other versions of Windows.
#endif						

#ifndef _WIN32_WINDOWS		// Allow use of features specific to Windows 98 or later.
#define _WIN32_WINDOWS 0x0410 // Change this to the appropriate value to target Windows Me or later.
#endif

#ifndef _WIN32_IE			// Allow use of features specific to IE 6.0 or later.
#define _WIN32_IE 0x0600	// Change this to the appropriate value to target other versions of IE.
#endif

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

#include <windows.h>			// Windows Header Files


// TODO: reference additional headers your program requires here
