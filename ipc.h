#include <WinDef.h>

BOOL IpcCreate(LPHANDLE hSlot);
BOOL IpcOpen(LPHANDLE hSlot);
BOOL IpcRead(HANDLE hSlot, LPSTR lpszBuffer, DWORD sizeBuffer, DWORD dwMilliseconds);
BOOL IpcWrite(HANDLE hSlot, LPSTR lpszMessage);