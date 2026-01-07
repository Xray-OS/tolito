#include <iostream>
#include <cstdlib>
#include <string>
#include <vector>

#include "tolito-install.h"
#include "tolito-remove.h"
#include "tolito-query.h"
#include "tolito-cache.h"
#include "tolito-update.h"

// Colors for better visibility
#define RED      "\033[31m"
#define GREEN    "\033[32m"
#define YELLOW   "\033[33m"
#define RESET    "\033[0m"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: tolito <option> [pkg-name1] [pkg-name2] ...\n"
                  << "Options:\n"
                  << " -S  <pkg>   Install package(s)\n"
                  << " -Sr <pkg>   Install from repository only\n"
                  << " -Syu        Update all packages\n"
                  << " -Su <pkg>   Update specific package\n"
                  << " -R  <pkg>   Remove package(s)\n"
                  << " -Qi <pkg>   Show package info\n"
                  << " clean       Clear build cache\n";
        return 1;
    }

    std::string option = argv[1];

    if (option == "clean") {
        clearCache();
        return 0;
    }

    if (option == "-Syu") {
        return updatePkg(""); // Update all packages
    }

    if (option == "-Su" && argc >= 3) {
        return updatePkg(argv[2]); // Update specific package
    }

    if (argc < 3) {
        std::cerr << RED << "[!] Error: Option " << option
                  << " requires at least one package name." << RESET << "\n";
        return 1;
    }

    std::vector<std::string> failedPkgs;
    std::vector<std::string> declinedPkgs;
    std::vector<std::string> alreadyInstalledPkgs;
    int successCount = 0;

    for (int i = 2; i < argc; ++i) {
        std::string pkg = argv[i];
        int result = 0; // 0 = failure, 1 = success, 2 = declined, 3 = already installed

        if (option == "-S") {
            result = installPkg(pkg);
        } else if (option == "-Sr") {
            result = installPkgFromRepo(pkg);
        } else if (option == "-R") {
            result = removePkg(pkg) ? 1 : 0;
        } else if (option == "-Q") {
            std::system(("pacman -Q " + pkg).c_str());
            result = 1;
        } else if (option == "-Qi") {
            showInfo(pkg);
            result = 1; // Info display usually treated as success
        } else {
            std::cerr << RED << "[!] Invalid option: " << option << RESET << "\n";
            return 2;
        }

        if (result == 1) {
            successCount++;
        } else if (result == 2) {
            declinedPkgs.push_back(pkg);
        } else if (result == 3) {
            alreadyInstalledPkgs.push_back(pkg);
        } else {
            failedPkgs.push_back(pkg);
        }
    }

    if (option == "-S" || option == "-Sr" || option == "-R") {
        // Show transaction summary if there were any activities (successes, failures, or declines)
        if (successCount > 0 || !failedPkgs.empty() || !declinedPkgs.empty()) {
            std::cout << "\n" << YELLOW << "── Transaction Summary ──" << RESET << "\n";
            
            // Special case: if only declined packages and no successes/failures
            if (successCount == 0 && failedPkgs.empty() && !declinedPkgs.empty()) {
                std::cout << YELLOW << "Operation cancelled by the user" << RESET << "\n";
            } else {
                if (successCount > 0) {
                    std::cout << GREEN << "[✓] Packages processed: " << successCount << RESET << "\n";
                }

                if (!declinedPkgs.empty()) {
                    std::cout << YELLOW << "[*] Installations declined: " << declinedPkgs.size() << RESET << "\n";
                    std::cout << YELLOW << "    Declined list: ";
                    for (const auto& name : declinedPkgs) {
                        std::cout << name << " ";
                    }
                    std::cout << RESET << "\n";
                }

                if (!failedPkgs.empty()) {
                    std::cout << RED << "[!] Packages failed:    " << failedPkgs.size() << RESET << "\n";
                    std::cout << RED << "    Failed list: ";
                    for (const auto& name : failedPkgs) {
                        std::cout << name << " ";
                    }
                    std::cout << RESET << "\n";
                }
            }
            std::cout << YELLOW << "─────────────────────────" << RESET << "\n";
        }
    }

    return (failedPkgs.empty() ? 0 : 1);
}
