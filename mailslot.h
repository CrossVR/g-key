#include <WinDef.h>

BOOL MakeSlot(LPHANDLE hSlot);
BOOL ConnectSlot(LPHANDLE hSlot);
BOOL ReadSlot(HANDLE hSlot, LPSTR lpszBuffer, DWORD sizeBuffer, DWORD dwMilliseconds);
BOOL WriteSlot(HANDLE hSlot, LPSTR lpszMessage);