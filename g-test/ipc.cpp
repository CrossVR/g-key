#include "../ipc.h"
#include <Windows.h>
#include <stdio.h>
#include <string.h>

wchar_t* Path = L"TS3GKEY";
wchar_t* BufferReadyEvent = L"TS3GKEY_BUFFER_READY";
wchar_t* DataReadyEvent = L"TS3GKEY_DATA_READY";

HANDLE hMapFile;
HANDLE hBufferReadyEvent;
HANDLE hDataReadyEvent;

BOOL IpcInit(void)
{
	hMapFile = CreateFileMapping(
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

BOOL IpcRead(IpcMessage* message, DWORD dwMilliseconds)
{
	void* pBuf;
	int ret;

	pBuf = MapViewOfFile(hMapFile, // handle to map object
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
	
	ret = WaitForSingleObject(hDataReadyEvent, dwMilliseconds);

	if(ret != WAIT_OBJECT_0) return FALSE;
	
	memcpy(message, pBuf, sizeof(IpcMessage));

	SetEvent(hBufferReadyEvent);

	UnmapViewOfFile(pBuf);

	return TRUE;
}

BOOL IpcWrite(IpcMessage* message, DWORD dwMilliseconds)
{
	void* pBuf;
	int ret;

	pBuf = MapViewOfFile(hMapFile,   // handle to map object
		FILE_MAP_WRITE, // write permission
		0,
		0,
		IPC_BUFSIZE);

	if(pBuf == NULL)
	{
		printf("Could not map view of file (%d).\n",
				GetLastError());

		return FALSE;
	}

	ret = WaitForSingleObject(hBufferReadyEvent, dwMilliseconds);

	if(ret != WAIT_OBJECT_0) return FALSE;

	memcpy(pBuf, message, sizeof(IpcMessage));

	SetEvent(hDataReadyEvent);

	UnmapViewOfFile(pBuf);

	return TRUE;
}

BOOL IpcClose(void)
{
	if(!CloseHandle(hMapFile))
	{
		printf("Could not close file mapping object (%d).\n", 
				GetLastError());
		return FALSE;
	}

	if(!CloseHandle(hBufferReadyEvent))
	{
		printf("Could not close buffer ready event (%d).\n", 
				GetLastError());
		return FALSE;
	}
	
	if(!CloseHandle(hDataReadyEvent))
	{
		printf("Could not close data ready event (%d).\n", 
				GetLastError());
		return FALSE;
	}

	return TRUE;
}
