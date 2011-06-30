#include "ipc.h"
#include <Windows.h>
#include <stdio.h>
#include <string.h>

wchar_t* Path = L"TS3GKEY";
wchar_t* BufferReadyEvent = L"TS3GKEY_BUFFER_READY";
wchar_t* DataReadyEvent = L"TS3GKEY_DATA_READY";

HANDLE hBufferReadyEvent;
HANDLE hDataReadyEvent;

BOOL IpcCreate(LPHANDLE hMapFile)
{
	*hMapFile = CreateFileMapping(
		INVALID_HANDLE_VALUE,	// use paging file
		NULL,					// default security 
		PAGE_READWRITE,			// read/write access
		0,						// maximum object size (high-order DWORD) 
		IPC_BUFSIZE,			// maximum object size (low-order DWORD)  
		Path);					// name of mapping object

	if (hMapFile == NULL)
	{
		printf("Could not create file mapping object (%d).\n", 
				GetLastError());
		return FALSE;
	}
	
	hBufferReadyEvent = CreateEvent(
		NULL,				// no security descriptor
		FALSE,				// auto-reset event
		TRUE,				// initial state signaled
		BufferReadyEvent);	// name of the event
	
	if (hBufferReadyEvent == NULL)
	{
		printf("Could not create buffer ready event (%d).\n", 
				GetLastError());
		return FALSE;
	}

	hDataReadyEvent = CreateEvent(
		NULL,				// no security descriptor
		FALSE,				// auto-reset event
		FALSE,				// initial state non-signaled
		DataReadyEvent);	// name of the event
	
	if (hDataReadyEvent == NULL)
	{
		printf("Could not create data ready event (%d).\n", 
				GetLastError());
		return FALSE;
	}

	return TRUE;
}

BOOL IpcOpen(LPHANDLE hMapFile)
{
	*hMapFile = OpenFileMapping(
		FILE_MAP_ALL_ACCESS,	// read/write access
		FALSE,					// do not inherit the name
		Path);					// name of mapping object 

	if (hMapFile == NULL)
	{
		printf("Could not open file mapping object (%d).\n", 
				GetLastError());
		return FALSE;
	}
	
	hBufferReadyEvent = OpenEvent(
		EVENT_MODIFY_STATE,	// modify access
		FALSE,				// do not inherit the handle
		BufferReadyEvent);	// name of the event
	
	if (hBufferReadyEvent == NULL)
	{
		printf("Could not open buffer ready event (%d).\n", 
				GetLastError());
		return FALSE;
	}

	hDataReadyEvent = OpenEvent(
		EVENT_MODIFY_STATE,	// modify access
		FALSE,				// do not inherit the handle
		DataReadyEvent);	// name of the event
	
	if (hDataReadyEvent == NULL)
	{
		printf("Could not open data ready event (%d).\n", 
				GetLastError());
		return FALSE;
	}

	return TRUE;
}

BOOL IpcRead(HANDLE hMapFile, IpcMessage* message, DWORD dwMilliseconds)
{
	LPCTSTR pBuf;

	pBuf = (LPTSTR) MapViewOfFile(hMapFile, // handle to map object
		FILE_MAP_READ,  // read permission
		0,
		0,
		IPC_BUFSIZE);
 
	if (pBuf == NULL) 
	{ 
		printf("Could not map view of file (%d).\n",
				GetLastError());

		CloseHandle(hMapFile);

		return 1;
	}

	WaitForSingleObject(hDataReadyEvent, dwMilliseconds);
	
	memcpy(&message, (PVOID)pBuf, sizeof(IpcMessage));

	SetEvent(hBufferReadyEvent);

	UnmapViewOfFile(pBuf);

	return TRUE;
}

BOOL IpcWrite(HANDLE hMapFile, IpcMessage message, DWORD dwMilliseconds)
{
	LPCTSTR pBuf;

	pBuf = (LPTSTR) MapViewOfFile(hMapFile,   // handle to map object
		FILE_MAP_WRITE, // write permission
		0,
		0,
		IPC_BUFSIZE);
 
	if (pBuf == NULL) 
	{ 
		printf("Could not map view of file (%d).\n",
				GetLastError());

		return FALSE;
	}

	WaitForSingleObject(hBufferReadyEvent, dwMilliseconds);

	memcpy((PVOID)pBuf, &message, sizeof(IpcMessage));

	SetEvent(hDataReadyEvent);

	UnmapViewOfFile(pBuf);

	return TRUE;
}