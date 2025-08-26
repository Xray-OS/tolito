// src/tolito-query.cpp

#include "tolito-query.hpp"

#include <iostream>
#include <cstdlib>
#include <string>
#include <sys/wait.h>

void queryPkg(const std::string& pkg) {
    std::string cmd = "pacman -Q " + pkg + " 2>/dev/null";
    int raw = std::system(cmd.c_str());
    if (raw == -1) {
        std::cerr << "[!] system() call failed\n";
        return;
    }
    int exitCode = WEXITSTATUS(raw);
    if (exitCode != 0) {
        std::cout << "[!] Package \"" << pkg << "\" is not installed\n";
    }
}

void showInfo(const std::string& pkg) {
    std::string cmd = "pacman -Qi " + pkg + " 2>/dev/null";
    int raw = std::system(cmd.c_str());
    if (raw == -1) {
        std::cerr << "[!] system() call failed\n";
        return;
    }
    int exitCode = WEXITSTATUS(raw);
    if (exitCode != 0) {
        std::cout << "[!] No information found for \"" << pkg << "\"\n";
    }
}
