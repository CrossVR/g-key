#include "mailslot.h"
#include <Windows.h>
#include <stdio.h>
#include <string.h>

wchar_t* Slot = L"\\\\.\\mailslot\\teamspeak3";

BOOL MakeSlot(LPHANDLE hSlot)
{ 
    *hSlot = CreateMailslot(Slot, 
        0,                             // no maximum message size 
        MAILSLOT_WAIT_FOREVER,         // no time-out for operations 
        (LPSECURITY_ATTRIBUTES) NULL); // default security
	
    if (hSlot == INVALID_HANDLE_VALUE)
    {
        printf("CreateMailslot failed with %d\n", GetLastError());
        return FALSE;
    }
    return TRUE; 
}

BOOL OpenSlot(LPHANDLE hSlot)
{ 
    *hSlot = CreateFile(
		Slot,
		GENERIC_WRITE,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

     
     if(*hSlot == INVALID_HANDLE_VALUE) 
     {
          printf("\nError occurred while connecting" 
                 " to the server: %d", GetLastError()); 
          return FALSE;  //Error
     }

	 return TRUE;
}

BOOL ReadSlot(HANDLE hSlot, LPSTR lpszBuffer, DWORD sizeBuffer, DWORD dwMilliseconds)
{ 
    DWORD cbMessage, cMessage, cbRead; 
    BOOL fResult;
	
    fResult = GetMailslotInfo(hSlot,
		&sizeBuffer,
        &cbMessage,
        &cMessage,
        &dwMilliseconds);
 
    if(!fResult)
    {
        printf("GetMailslotInfo failed with %d.\n", GetLastError());
        return FALSE;
    }
	
    if(cbMessage == MAILSLOT_NO_MESSAGE)
        return FALSE;

    fResult = ReadFile(hSlot,
		lpszBuffer,
		cbMessage,
		&cbRead,
		NULL);
	
    if (!fResult)
    {
        printf("ReadFile failed with %d.\n", GetLastError());
        return FALSE;
    }

    return TRUE; 
}

BOOL WriteSlot(HANDLE hSlot, LPSTR lpszMessage)
{
	BOOL fResult;
	DWORD cbWritten;

	fResult = WriteFile(hSlot,
		lpszMessage,
		(DWORD) (strlen(lpszMessage)+1)*sizeof(CHAR),
		&cbWritten,
		NULL);

	if(!fResult)
	{
		printf("WriteFile failed with %d.\n", GetLastError());
		return FALSE;
	}

	return TRUE;
}
