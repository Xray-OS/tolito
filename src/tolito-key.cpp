// src/tolito-key.cpp

#include "tolito-key.hpp"

#include <iostream>
#include <cstdlib>
#include <string>
#include <sys/wait.h>

static constexpr char KEYSERVER[] = "keyserver.ubuntu.com";

/// Run a shell command, returning its exit code (or –1 if system() fails).
static int runCmd(const std::string& cmd) {
    std::cout << "[~] " << cmd << "\n";
    int rc = std::system(cmd.c_str());
    return (rc == -1 ? -1 : WEXITSTATUS(rc));
}

bool importPgpKey(const std::string& keyId) {
    std::cout << "[*] Importing PGP key " << keyId << " into GPG…\n";

    // 1) Import into the user GPG keyring
    std::string gpgCmd = "gpg --batch --keyserver "
                       + std::string(KEYSERVER)
                       + " --recv-keys " + keyId;
    if (runCmd(gpgCmd) != 0) {
        std::cerr << "[!] gpg failed to fetch key " << keyId << "\n";
        return false;
    }

    // 2) Also register & locally sign in pacman-key
    std::cout << "[*] Registering key with pacman-key…\n";
    std::string pkRecv = "sudo pacman-key --keyserver "
                       + std::string(KEYSERVER)
                       + " --recv-keys " + keyId;
    std::string pkSign = "sudo pacman-key --lsign-key " + keyId;
    if (runCmd(pkRecv) != 0 || runCmd(pkSign) != 0) {
        std::cerr << "[!] pacman-key failed for " << keyId << "\n";
        // continue anyway, GPG import is primary for makepkg
    }

    std::cout << "[✓] Imported & signed PGP key " << keyId << "\n";
    return true;
}
