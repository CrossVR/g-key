#include "ipc.h"
#include <Windows.h>
#include <stdio.h>
#include <string.h>

#define BUF_SIZE 1024

wchar_t* szName = L"ts3_gkey";

BOOL IpcCreate(LPHANDLE hMapFile)
{
	*hMapFile = CreateFileMapping(
		INVALID_HANDLE_VALUE,    // use paging file
		NULL,                    // default security 
		PAGE_READWRITE,          // read/write access
		0,                       // maximum object size (high-order DWORD) 
		BUF_SIZE,                // maximum object size (low-order DWORD)  
		szName);                 // name of mapping object\

	if (hMapFile == NULL) 
	{ 
		printf("Could not create file mapping object (%d).\n", 
				GetLastError());
		return FALSE;
	}
}

BOOL IpcOpen(LPHANDLE hMapFile)
{
	*hMapFile = OpenFileMapping(
		FILE_MAP_ALL_ACCESS,   // read/write access
		FALSE,                 // do not inherit the name
		szName);               // name of mapping object 
 
	if (hMapFile == NULL) 
	{ 
		printf("Could not open file mapping object (%d).\n", 
				GetLastError());
		return 1;
	} 
}

BOOL IpcRead(HANDLE hMapFile, LPSTR lpszBuffer, DWORD sizeBuffer, DWORD dwMilliseconds)
{
	LPCTSTR pBuf;

	pBuf = (LPTSTR) MapViewOfFile(hMapFile, // handle to map object
               FILE_MAP_ALL_ACCESS,  // read/write permission
               0,                    
               0,                    
               BUF_SIZE);                   
 
	if (pBuf == NULL) 
	{ 
		printf("Could not map view of file (%d).\n", 
				GetLastError()); 

		CloseHandle(hMapFile);

		return 1;
	}

	// Read

	UnmapViewOfFile(pBuf);
}

BOOL IpcWrite(HANDLE hMapFile, LPSTR lpszMessage)
{
	LPCTSTR pBuf;

	pBuf = (LPTSTR) MapViewOfFile(hMapFile,   // handle to map object
						FILE_MAP_ALL_ACCESS, // read/write permission
						0,                   
						0,                   
						BUF_SIZE);           
 
	if (pBuf == NULL) 
	{ 
		printf("Could not map view of file (%d).\n", 
				GetLastError());

		return FALSE;
	}

	//Write

	memcpy((PVOID)pBuf, szMsg, (_tcslen(szMsg) * sizeof(TCHAR)));

	UnmapViewOfFile(pBuf);
}