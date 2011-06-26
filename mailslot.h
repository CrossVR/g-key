#include <WinDef.h>

BOOL WINAPI MakeSlot(LPHANDLE hSlot);
BOOL ReadSlot(HANDLE hSlot, LPSTR lpszBuffer, DWORD sizeBuffer, DWORD dwMilliseconds);
BOOL WriteSlot(HANDLE hSlot, LPSTR lpszMessage);