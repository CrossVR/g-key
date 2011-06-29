#include <WinDef.h>

BOOL MakeSlot(LPHANDLE hSlot);
BOOL OpenSlot(LPHANDLE hSlot);
BOOL ReadSlot(HANDLE hSlot, LPSTR lpszBuffer, DWORD sizeBuffer, DWORD dwMilliseconds);
BOOL WriteSlot(HANDLE hSlot, LPSTR lpszMessage);