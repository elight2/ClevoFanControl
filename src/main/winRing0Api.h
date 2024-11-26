#include <Windows.h>
#include <qwindowdefs_win.h>
#include <tchar.h>

#include <atomic>

typedef BOOL (WINAPI *_InitializeOls) ();
typedef VOID (WINAPI *_DeinitializeOls) ();
typedef BYTE  (WINAPI *_ReadIoPortByte) (WORD address);
typedef VOID (WINAPI *_WriteIoPortByte) (WORD address, BYTE value);
typedef DWORD (WINAPI *_Rdmsr) (DWORD index, PDWORD eax, PDWORD edx);

extern _InitializeOls InitializeOls;
extern _DeinitializeOls DeinitializeOls;
extern _ReadIoPortByte ReadIoPortByte;
extern _WriteIoPortByte WriteIoPortByte;
extern _Rdmsr Rdmsr;

class winRing0Api {
public:
    static BOOL initApi();
    static BOOL deinitApi();

private:
    static std::atomic<HMODULE> dll;
    static std::atomic_bool apiInit;
};
