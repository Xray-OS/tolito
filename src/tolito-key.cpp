#include "tolito-key.h"

#include <cctype>
#include <iostream>
#include <cstdlib>
#include <string>
#include <sys/wait.h>
#include <algorithm>

static constexpr char KEYSERVER[] = "keyserver.ubuntu.com";

static int runCmd(const std::string& cmd) {
    std::cout << "[~] " << cmd << "\n";
    int rc = std::system(cmd.c_str());
    return (rc == -1 ? -1 : WEXITSTATUS(rc));
}

bool isValidKeyId(const std::string& keyId) {
    // GPG Key IDs are hex strings. They should only contain 0-9 and A-F.
    if (keyId.empty() || keyId.length() > 40) return false;

    return std::all_of(keyId.begin(), keyId.end(), [](unsigned char c) {
        return std::isxdigit(c);
    });
}

bool fetchAndTrustgKey(const std::string& keyId) {
    if (!isValidKeyId(keyId)) {
        std::cerr << "[!] Invalid Key ID format: " << keyId << "\n";
        return false;
    }

    std::string fetchCmd = "gpg --batch --keyserver " + std::string(KEYSERVER) +
                      " --recv-keys " + keyId + " 2>&1";

    if (runCmd(fetchCmd)!= 0) {
        std::cerr << "[!] GPG failed to fetch key: " << keyId << "\n";
        return false;
    }

    // If success also sign in pacman-key
    std::cout << "[*] Signing key with pacman-key...\n";
    std::string signCmd = "sudo pacman-key --lsign-key " + keyId;

    if (runCmd(signCmd) != 0) {
        std::cerr << "[!] pacman-key failed for " << keyId << "\n";
        return false;
    }

    std::cout << "[✓] Successfully imported & signed PGP key " << keyId << "\n";
    return true;
}
