#include "tolito-remove.h"
#include "tolito-install.h"

#include <iostream>
#include <cstdlib>
#include <string>
#include <sys/wait.h>

// ANSI colors
static constexpr char RED[]    = "\033[31m";
static constexpr char GREEN[]  = "\033[32m";
static constexpr char YELLOW[] = "\033[33m";
static constexpr char RESET[]  = "\033[0m";

// Run a shell command; return exit code or -1
static int runCmd(const std::string& cmd, bool quiet = false) {
    if (!quiet) std::cout << YELLOW << "[~] " << cmd << RESET << "\n";
    int r = std::system(cmd.c_str());
    return (r == -1 ? -1 : WEXITSTATUS(r));
}

bool removePkg(const std::string& pkg) {
    if (pkg.empty()) return false;

    int exitCode = runCmd("sudo pacman -Rns " + pkg);

    if (exitCode == -1) {
        std::cerr << RED << "[!] system() call failed" << RESET << "\n";
        return false;
    } else if (exitCode != 0) {
        std::cerr << RED << "[!] pacman failed (Code: " << exitCode << ")" << RESET << "\n";
        return false;
    } else {
        std::cout << GREEN << "[✓] Package \"" << pkg << "\" removed successfully" << RESET << "\n";
        removePackageSource(pkg);
        return true;
    }
}
