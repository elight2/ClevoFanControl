#include "winRing0Api.h"

std::atomic<HMODULE> winRing0Api::dll=NULL;
std::atomic_bool winRing0Api::apiInit=false;

_InitializeOls InitializeOls;
_DeinitializeOls DeinitializeOls;
_ReadIoPortByte ReadIoPortByte;
_WriteIoPortByte WriteIoPortByte;
_Rdmsr Rdmsr;

BOOL winRing0Api::initApi() {
    dll = LoadLibrary(_T("WinRing0x64.dll"));
    Rdmsr =					(_Rdmsr)				GetProcAddress (dll, "Rdmsr");
    ReadIoPortByte =		(_ReadIoPortByte)		GetProcAddress (dll, "ReadIoPortByte");
    WriteIoPortByte =		(_WriteIoPortByte)		GetProcAddress (dll, "WriteIoPortByte");
    InitializeOls =			(_InitializeOls)		GetProcAddress (dll, "InitializeOls");
	DeinitializeOls =		(_DeinitializeOls)		GetProcAddress (dll, "DeinitializeOls");

    return InitializeOls();
}

BOOL winRing0Api::deinitApi() {
    BOOL result = FALSE;

	if(dll == NULL)
	{
		return TRUE;
	}
	else
	{
		DeinitializeOls();
		result = FreeLibrary(dll);
		dll = NULL;

		return result;
    }
}
