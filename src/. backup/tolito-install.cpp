// src/tolito-install.cpp

#include "tolito-install.hpp"

#include <iostream>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <sys/wait.h>    // for WEXITSTATUS

namespace fs = std::filesystem;

// ANSI color codes
static constexpr char RED[]    = "\033[31m";
static constexpr char GREEN[]  = "\033[32m";
static constexpr char YELLOW[] = "\033[33m";
static constexpr char RESET[]  = "\033[0m";

// Execute a shell command, returning its exit status
int runCommand(const std::string& cmd, bool verbose = true) {
    if (verbose) {
        std::cout << YELLOW << "[~] " << cmd << RESET << "\n";
    }
    int ret = std::system(cmd.c_str());
    return WEXITSTATUS(ret);
}

// RAII helper for a temporary working directory
class TempDir {
public:
    explicit TempDir(const fs::path& p) : path_{p} {
        fs::remove_all(path_);
        fs::create_directories(path_);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path_, ec);
        if (ec) {
            std::cerr << RED
                      << "[!] Warning: could not remove " << path_ << ": "
                      << ec.message() << RESET << "\n";
        }
    }
    const fs::path& path() const noexcept { return path_; }

private:
    fs::path path_;
};

void installPkg(const std::string& pkg) {
    // 1) Pre-flight checks
    if (runCommand("which git > /dev/null") != 0) {
        std::cerr << RED << "[!] git not found in PATH" << RESET << "\n";
        return;
    }
    if (runCommand("which makepkg > /dev/null") != 0) {
        std::cerr << RED << "[!] makepkg not found in PATH" << RESET << "\n";
        return;
    }

    // 2) Setup temporary build directory
    fs::path buildDir = fs::temp_directory_path() / ("tolito_" + pkg);
    TempDir workdir(buildDir);

    // 3) Clone the AUR repository
    std::string repo     = "https://aur.archlinux.org/" + pkg + ".git";
    std::string cloneCmd = "git clone " + repo + " " + buildDir.string();
    std::cout << GREEN << "[*] Cloning " << repo << RESET << "\n";
    if (runCommand(cloneCmd) != 0) {
        std::cerr << RED << "[!] Failed to clone " << pkg << RESET << "\n";
        return;
    }

    // 4) Build and install
    fs::current_path(buildDir);
    std::string buildCmd = "makepkg -si --noconfirm";
    std::cout << GREEN << "[*] Building and installing " << pkg << RESET << "\n";
    if (runCommand(buildCmd) != 0) {
        std::cerr << RED << "[!] makepkg failed for " << pkg << RESET << "\n";
        return;
    }

    // 5) Success
    std::cout << GREEN << "[✓] " << pkg
              << " installed successfully!" << RESET << "\n";
}
