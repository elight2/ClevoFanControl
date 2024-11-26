#ifndef CLEVO_EC_ACCESSOR_H
#define CLEVO_EC_ACCESSOR_H

#include <mutex>
#include <atomic>

#ifdef _WIN32
#include <Windows.h>
#include <tchar.h>
#endif

class ClevoEcAccessor {
public:
    ClevoEcAccessor();
    ~ClevoEcAccessor();
    void setFanSpeed(int percentage, int index); //percentage: -1 means auto
    int getRpm(int index);

private:
    //core IO funcs
    void ioOutByte(unsigned short port, unsigned char value);
    unsigned char ioInByte(unsigned short port);
    void waitEc(unsigned short port, unsigned char index, unsigned char value);
    unsigned char readEc_1(unsigned char addr);
    void doEc(unsigned char cmd, unsigned char addr, unsigned char value);

#ifdef _WIN32
    bool InitOpenLibSys_m();
    bool DeinitOpenLibSys_m();
#endif

    //protect EC
    static std::mutex EClock;
#ifdef _WIN32
    static HMODULE WinRing0m;
#endif
    //only init once
    static std::atomic_bool ioInitialized;
    //avoid crash
    static std::atomic_bool havePrivilege;
};

#endif
