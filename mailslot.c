#include "mailslot.h"
#include <Windows.h>
#include <stdio.h>

wchar_t* Slot = L"\\\\.\\mailslot\\teamspeak3";

BOOL WINAPI MakeSlot(LPHANDLE hSlot)
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

BOOL ReadSlot(LPHANDLE hSlot, LPTSTR lpszBuffer, DWORD sizeBuffer, DWORD dwMilliseconds)
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

BOOL WriteSlot(HANDLE hSlot, LPTSTR lpszMessage)
{
	BOOL fResult;
	DWORD cbWritten;

	fResult = WriteFile(hSlot,
		lpszMessage,
		(DWORD) (lstrlen(lpszMessage)+1)*sizeof(TCHAR),
		&cbWritten,
		NULL);

	if(!fResult)
	{
		printf("WriteFile failed with %d.\n", GetLastError());
		return FALSE;
	}

	return TRUE;
}
