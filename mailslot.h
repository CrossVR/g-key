#include <WinDef.h>

BOOL MakeSlot(LPHANDLE hSlot, DWORD sizeBuffer, DWORD dwMilliseconds);
BOOL OpenSlot(LPHANDLE hSlot);
BOOL ReadSlot(HANDLE hSlot, LPSTR lpszBuffer);
BOOL WriteSlot(HANDLE hSlot, LPSTR lpszMessage);