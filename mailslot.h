#include <WinDef.h>

BOOL WINAPI MakeSlot(LPHANDLE hSlot);
BOOL ReadSlot(HANDLE hSlot, LPTSTR lpszBuffer, DWORD sizeBuffer, DWORD dwMilliseconds);
BOOL WriteSlot(HANDLE hSlot, LPTSTR lpszMessage);