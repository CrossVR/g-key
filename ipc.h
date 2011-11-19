#ifndef GKEY_IPC
#define GKEY_IPC

#include <Windows.h>

#define IPC_BUFSIZE 256

typedef struct
{
	char message[IPC_BUFSIZE];
} IpcMessage;

BOOL IpcInit(void);
BOOL IpcRead(IpcMessage* message, DWORD dwMilliseconds);
BOOL IpcWrite(IpcMessage* message, DWORD dwMilliseconds);
BOOL IpcClose(void);

#endif