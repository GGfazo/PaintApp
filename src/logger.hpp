#pragma once
#include <iostream>
#include <fstream>
#include <source_location>

static constexpr bool DEBUG_INFO = true; // true = Display debug info, false = Don't display debug info

extern std::ofstream logFile;

template <typename T>
void PrintWithoutHeader(T val, std::source_location location = std::source_location::current()){
    std::cout << "\tFunction: " << location.function_name() << ":\n\tLine: " << location.line() << ":\n\t"<< val << '\n';
    logFile << "\tFunction: "<< location.function_name() << ":\n\tLine: " << location.line() << ":\n\t"<< val << '\n' <<std::flush;
}

//Prints 'val' to the console as a Debug message 
template <typename T>
void DebugPrint(T val, std::source_location location = std::source_location::current()){
    if(!DEBUG_INFO) return;
    
    std::cout<<"[Debug]\n";
    logFile<<"[Debug]\n";
    PrintWithoutHeader(val, location);
}

//Prints 'val' to the console as an Error message 
template <typename T>
void ErrorPrint(T val, std::source_location location = std::source_location::current()){
    std::cout<<"[ERROR]\n";
    logFile<<"[ERROR]\n";
    PrintWithoutHeader(val, location);
    //std::cin.get(); //Pauses executable
}