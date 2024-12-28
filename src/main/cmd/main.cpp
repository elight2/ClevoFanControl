#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <string>

#include "../ClevoEcAccessor.h"

using namespace std;

void printHelp() {
    cout<<"Usage: ClevoFanControl-cmd [fan index] [target speed percentage]\n";
    cout<<"[fan index]: cpu fan is 1, gpu is 2, some laptops have 3rd fan\n";
    cout<<"[target speed percentage]: 1-100, use \"a\" to use clevo auto\n";
}

void controlFan(vector<string> args) {
    int index=0;
    int percentage=0;
    try { //first convert index
        index=stoi(args[1]);
    }
    catch(std::invalid_argument &exc) {
        cout<<"Invalid arg!\n";
        return;
    }

    bool useAuto=0;
    useAuto=args[2]=="a";
    if(!useAuto) {
        try { //then convert speed
            percentage=stoi(args[2]);
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

int main(int argc, char* argv[]) {
    //args
    vector<string> args(argv,argv+argc);

    if(args.size()==1)
        cout<<"ClevoFanControl cmd mode!\nYou need some args to use it!\n";
    else if(args[1]=="/?" || args[1]=="--help")
        printHelp();
    else if(args.size()<3)
        cout<<"Only 1 arg found!\nUse --help or /? to check the usage.\n";
    else if(args.size()>3)
        cout<<"More than 2 args detected!\nUse --help or /? to check the usage.\n";
    else
        controlFan(args);
    
    return 0;
}
