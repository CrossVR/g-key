/* Based on code by Ken Zhang */

#include <Windows.h>
#include "WinDebug.h"

HANDLE m_hDBWinMutex;
HANDLE m_hDBMonBuffer;
HANDLE m_hEventBufferReady;
HANDLE m_hEventDataReady;

DBWIN_BUFFER* m_pDBBuffer;

DWORD WinDebugInitialize()
{
	DWORD errorCode = 0;

	SetLastError(0);

	// Mutex: DBWin
	// ---------------------------------------------------------
	m_hDBWinMutex = OpenMutex(
		MUTEX_ALL_ACCESS, 
		FALSE, 
		L"DBWinMutex"
		);

	if (m_hDBWinMutex == NULL) {
		errorCode = GetLastError();
		return errorCode;
	}

	// Event: buffer ready
	// ---------------------------------------------------------
	m_hEventBufferReady = OpenEvent(
		EVENT_ALL_ACCESS,
		FALSE,
		L"DBWIN_BUFFER_READY"
		);

	if (m_hEventBufferReady == NULL) {
		m_hEventBufferReady = CreateEvent(
			NULL,
			FALSE,	// auto-reset
			TRUE,	// initial state: signaled
			L"DBWIN_BUFFER_READY"
			);

		if (m_hEventBufferReady == NULL) {
			errorCode = GetLastError();
			return errorCode;
		}
	}

	// Event: data ready
	// ---------------------------------------------------------
	m_hEventDataReady = OpenEvent(
		SYNCHRONIZE,
		FALSE,
		L"DBWIN_DATA_READY"
		);

	if (m_hEventDataReady == NULL) {
		m_hEventDataReady = CreateEvent(
			NULL,
			FALSE,	// auto-reset
			FALSE,	// initial state: nonsignaled
			L"DBWIN_DATA_READY"
			);

		if (m_hEventDataReady == NULL) {
			errorCode = GetLastError();
			return errorCode;
		}
	}

	// Shared memory
	// ---------------------------------------------------------
	m_hDBMonBuffer = OpenFileMapping(
		FILE_MAP_READ,
		FALSE,
		L"DBWIN_BUFFER"
		);

	if (m_hDBMonBuffer == NULL) {
		m_hDBMonBuffer = CreateFileMapping(
			INVALID_HANDLE_VALUE,
			NULL,
			PAGE_READWRITE,
			0,
			sizeof(DBWIN_BUFFER),
			L"DBWIN_BUFFER"
			);

		if (m_hDBMonBuffer == NULL) {
			errorCode = GetLastError();
			return errorCode;
		}
	}

	m_pDBBuffer = (DBWIN_BUFFER*)MapViewOfFile(
		m_hDBMonBuffer,
		SECTION_MAP_READ,
		0,
		0,
		0
		);

	if (m_pDBBuffer == NULL) {
		errorCode = GetLastError();
		return errorCode;
	}

	return errorCode;
}

void WinDebugUnintialize()
{
	if (m_hDBWinMutex != NULL) {
		CloseHandle(m_hDBWinMutex);
		m_hDBWinMutex = NULL;
	}

	if (m_hDBMonBuffer != NULL) {
		UnmapViewOfFile(m_pDBBuffer);
		CloseHandle(m_hDBMonBuffer);
		m_hDBMonBuffer = NULL;
	}

	if (m_hEventBufferReady != NULL) {
		CloseHandle(m_hEventBufferReady);
		m_hEventBufferReady = NULL;
	}

	if (m_hEventDataReady != NULL) {
		CloseHandle(m_hEventDataReady);
		m_hEventDataReady = NULL;
	}

	m_pDBBuffer = NULL;
}

DWORD WinDebugWaitForMessage(DBWIN_BUFFER *buffer, DWORD timeout)
{
	DWORD ret;

	// wait for data ready
	ret = WaitForSingleObject(m_hEventDataReady, timeout);
	
	/* This part needs to be VERY efficient, as long as the buffer is not
	 * signaled all applications are blocked when they try to send another
	 * debug message.
	 * We could put a check here to find out if the message was sent by
	 * a process we're interested in. However the CPU time difference
	 * between a comparison of 2 DWORDs and copying 4KiB is negligible.
	 * (remarkably copying is slightly faster!)
	 */

	if (ret == WAIT_OBJECT_0) {
		// copy the buffer
		*buffer = *m_pDBBuffer;

		// signal buffer ready
		SetEvent(m_hEventBufferReady);
	}

	return ret;
}

