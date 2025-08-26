// src/tolito-remove.cpp

#include "tolito-remove.hpp"

#include <iostream>
#include <cstdlib>
#include <string>
#include <sys/wait.h>

void removePkg(const std::string& pkg) {
    std::string cmd = "sudo pacman -Rns " + pkg + " --noconfirm";

    std::cout << "[*] Removing " << pkg << "...\n";

    int raw = std::system(cmd.c_str());
    if (raw == -1) {
        std::cerr << "[!] system() call failed\n";
        return;
    }

    int exitCode = WEXITSTATUS(raw);
    if (exitCode != 0) {
        std::cerr << "[!] pacman exited with code " << exitCode
                  << " while removing \"" << pkg << "\"\n";
    } else {
        std::cout << "[✓] Package \"" << pkg << "\" removed successfully\n";
    }
}
