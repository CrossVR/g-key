#include <Windows.h>

#define IPC_BUFSIZE 256

typedef struct
{
	char message[IPC_BUFSIZE];
} IpcMessage;

BOOL IpcInit(LPHANDLE hSlot);
BOOL IpcRead(HANDLE hMapFile, IpcMessage* message, DWORD dwMilliseconds);
BOOL IpcWrite(HANDLE hMapFile, IpcMessage* message, DWORD dwMilliseconds);