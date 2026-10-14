#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <string>

#include "../ClevoEcAccessor.h"

using namespace std;

void printHelp() {
    cout<<"Usage:\n";
    cout<<"Set speed: ClevoFanControl-cmd s [fan index] [target speed percentage]\n";
    cout<<"Query RPM: ClevoFanControl-cmd q [fan index]\n";
    cout<<"[fan index]: cpu fan is 1, gpu is 2, some laptops have 3rd fan\n";
    cout<<"[target speed percentage]: 1-100, use \"a\" to use clevo auto\n";
}

void controlFan(vector<string> args) {
    int index=0;
    int percentage=0;
    try { //first convert index
        index=stoi(args[2]);
    }
    catch(std::invalid_argument &exc) {
        cout<<"Invalid arg!\n";
        return;
    }

    bool useAuto=0;
    useAuto= args[3]=="a";
    if(!useAuto) {
        try { //then convert speed
            percentage=stoi(args[3]);
        }
        catch(std::invalid_argument &exc) {
            cout<<"Invalid arg!\n";
            return;
        }
    }

    //require confirm
    cout<<"******************************\n";
    cout<<"You are going to set the speed of fan ["<<index<<"] to ["<<(useAuto ? "auto" : to_string(percentage))<<"]\n";
    cout<<"******************************\n";
    cout<<"Input [a] to proceed!\n";
    cout<<"Input [c] to cancel!\n";

    //input
    while(1) {
        char input=0;
        input=std::cin.get();
        if(input=='a')
            break;
        else if(input=='c') {
            cout<<"Operation canceled!\n";
            return;
        }
        else
            cout<<"You need to input [a] or [c]!\n";
    }

    //apply
    ClevoEcAccessor accessor;
    accessor.setFanSpeed(useAuto ? -1 : percentage, index);

    cout<<"Fan speed applied!\n";
}

void queryFan(vector<string> args) {
    int index=0;
    try {
        index=stoi(args[2]);
    }
    catch(std::invalid_argument &exc) {
        cout<<"Invalid arg!\n";
        return;
    }

    //query
    ClevoEcAccessor accessor;
    cout<<"Rpm of Fan ["+to_string(index)+"] is ["+to_string(accessor.getRpm(index))+"]\n";
}

int main(int argc, char* argv[]) {
    //args
    vector<string> args(argv,argv+argc);

    if(args.size()==1)
        cout<<"ClevoFanControl cmd mode!\nYou need some args to use it!\n";
    else if(args[1]=="/?" || args[1]=="--help")
        printHelp();
    else if(args.size()<3)
        cout<<"Only 1 arg found!\nUse --help or /? to check the usage.\n";
    else if(args.size()==3) {
        if(args[1]=="q")
            queryFan(args);
        else if(args[1]=="s")
            cout<<"Wrong usage, use --help or /? to check the usage.\n";
        else
            cout<<"Unknown command, use --help or /? to check the usage.\n";
    }
    else if(args.size()==4) {
        if(args[1]=="s")
            controlFan(args);
        else if(args[1]=="q")
            cout<<"Wrong usage, use --help or /? to check the usage.\n";
        else
            cout<<"Unknown command, use --help or /? to check the usage.\n";
    }
    else
        cout<<"More than 4 args detected!\nUse --help or /? to check the usage.\n";
    
    return 0;
}
