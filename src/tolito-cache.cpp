// src/tolito-cache.cpp

#include "tolito-cache.hpp"

#include <iostream>
#include <cstdlib>
#include <sys/wait.h>
#include <filesystem>

namespace fs = std::filesystem;

void clearCache() {
	// clear pacman's cache
	std::cout << "[*] Clearing pacman cache...\n";
	const char* cmd = "yes | sudo pacman -Scc";
	int raw = std::system(cmd);
	if (raw == -1) {
		std::cerr << "[!] system() call failed\n";
		return;
	}
	int exitCode = WEXITSTATUS(raw);
	if (exitCode != 0) {
		std::cerr << "[!] Failed to clear pacman cache (exit code "
		<< exitCode << ")\n";
	} else {
		std::cout << "[✓] Pacman cache cleared\n";
	}

	// clear tolito's working directory
	std::cout << "[*] Clearing ~/tolito working directory...\n";
	const char* home = std::getenv("HOME");
	if (!home) {
		std::cerr << "[!] $HOME not set, cannot clear tolito cache\n";
		return;
	}

	fs::path cacheDir = fs::path(home) / "tolito";
	std::error_code ec;

	// remove everything under ~/tolito
	    // for (auto &entry : fs::directory_iterator(cacheDir, ec)) {
	    //     fs::remove_all(entry.path(), ec);
	    // }

    fs::remove_all(cacheDir, ec);
    if (ec) {
        std::cerr << "[!] Failed to remove " << cacheDir
        << ": " << ec.message() << "\n";
        return;
    }

	// recreate the directory
	fs::create_directories(cacheDir, ec);
	if (ec) {
		std::cerr << "[!] Cannot recreate " << cacheDir
		<< ": " << ec.message() << "\n";
	} else {
		std::cout << "[✓] Tolito working directory cleared\n";
	}
}
