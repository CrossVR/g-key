****************************************************************************

 Module:	ReadMe.txt

 Created 02/27/2010 by Len Chau
 
 Description:	project read me file.
 
 Copyright (c) 2009-2010 Logitech, Inc.  All Rights Reserved

 This program is a trade secret of LOGITECH, and it is not to be reproduced,
 published, disclosed to others, copied, adapted, distributed or displayed
 without the prior authorization of LOGITECH.

 Licensee agrees to attach or embed this notice on all copies of the program,
 including partial copies or modified versions thereof.

****************************************************************************

============================================================================
    DYNAMIC LINK LIBRARY : GKeyDLLSample1 Project Overview
============================================================================

AppWizard has created this GKeyDLLSample1 DLL for you.  

This file contains a summary of what you will find in each of the files 
that make up your GKeyDLLSample1 application.

GKeyDLLample1.vcproj
    This is the main project file for VC++ projects generated using an 
    Application Wizard. 
    It contains information about the version of Visual C++ that 
    generated the file, and information about the platforms, configurations, 
    and project features selected with the application Wizard.

GKeyDLLSample1.cpp
    This is the main DLL source file.

GKeyDLLSample1.def
	This file contains the name of the function(s) to be exported so that 
	the main application can access to this function(s)
	
GkeyImplementation.h
	This header file contains the C function prototype and other defines 
	for implementation.
	 
GkeyImplementation.cpp
    The file contains the application implementation for the DLL
    
/////////////////////////////////////////////////////////////////////////////
Other standard files:

StdAfx.h, StdAfx.cpp
    These files are used to build a precompiled header (PCH) file
    named GKeyDLLSample1.pch and a precompiled types file named StdAfx.obj.

/////////////////////////////////////////////////////////////////////////////
Other notes:

When modifying this sample project for your own DLL module. The user need 
to change the result output .dll file name to a specific module name for
your recognition.
The two function "GetGkeyCommandList() and RunGkeyCommand()" functions must 
remain the same name for the headset software to interface with the DLL.
 
Compile the project and load a result .dll files into the G-key application 
graphical interface and use it. The details for writing and using a G-key 
user DLL is described in a "GkeyPluginDevelopmentKit.doc or .PDF" file

/////////////////////////////////////////////////////////////////////////////
