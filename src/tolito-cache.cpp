#include "tolito-cache.h"

#include <cerrno>
#include <iostream>
#include <cstdlib>
#include <sys/wait.h>
#include <filesystem>
#include <system_error>

namespace fs = std::filesystem;

void clearCache() {
    // clear tolito's working directory
    std::cout << "[*] Clearing ~/tolito working directory...\n";
    const char* home = std::getenv("HOME");
    if (!home) {
        std::cerr << "[!] $HOME not set, cannot clear tolito cache\n";
        return;
    }

    fs::path cacheDir = fs::path(home) / "tolito";
    std::error_code ec;
    if (!fs::exists(cacheDir)) return;

    auto it = fs::directory_iterator(cacheDir, ec);
    if (ec) {
        std::cerr << "[!] Could not open directory: " << ec.message() << "\n";
        return;
    }

    for (const auto& entry : it) {
        std::error_code deleteEc;

        fs::remove_all(entry.path(), deleteEc);
        if(deleteEc.value() == EBUSY || deleteEc.value() == 32) {
            std::cerr << "[!] Skipping " << entry.path().filename()
            << " (currently in use)\n";
        }
        else if(deleteEc) {
            std::cerr << "[!] Failed to remove " << entry.path()
            << ": " << deleteEc.message() << "\n";
        }
    }
}
