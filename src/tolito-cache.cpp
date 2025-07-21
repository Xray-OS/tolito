// src/tolito-cache.cpp

#include "tolito-cache.hpp"

#include <iostream>
#include <cstdlib>
#include <sys/wait.h>

void clearCache() {
    std::cout << "[*] Clearing pacman cache...\n";

    const char* cmd = "yes | sudo pacman -Scc";
    int raw = std::system(cmd);
    if (raw == -1) {
        std::cerr << "[!] system() call failed\n";
        return;
    }

    int exitCode = WEXITSTATUS(raw);
    if (exitCode != 0) {
        std::cerr << "[!] Failed to clear cache (exit code "
                  << exitCode << ")\n";
    } else {
        std::cout << "[✓] Pacman cache cleared\n";
    }
}
