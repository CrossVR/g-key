/* Based on code by Ken Zhang */

#ifndef __WIN_DEBUG_BUFFER_H__
#define __WIN_DEBUG_BUFFER_H__

#include <WinDef.h>

struct dbwin_buffer
{
	DWORD   dwProcessId;
	char    data[4096-sizeof(DWORD)];
};
typedef struct dbwin_buffer DBWIN_BUFFER;

DWORD WinDebugInitialize(void);
void WinDebugUnintialize(void);
DWORD WinDebugWaitForMessage(DBWIN_BUFFER *buffer, DWORD timeout);

#endif
