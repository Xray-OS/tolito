// src/tolito-install.cpp

#include "tolito-install.hpp"

#include <iostream>
#include <cstdlib>
#include <string>
#include <sys/wait.h>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;

// Constants
static constexpr char MONOREPO[] = "https://gitlab.com/Arch7z/arch7z-pkgbuilds.git";
static constexpr char AUR_NS[]   = "https://aur.archlinux.org/";

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

// Detect if the spec is a URL
static bool isUrl(const std::string& s) {
    return s.rfind("http://", 0) == 0
    || s.rfind("https://", 0) == 0
    || s.rfind("git@", 0) == 0;
}

// Clone & build from a URL into tmpdir
static void cloneAndBuild(const std::string& url,
                          const std::string& tmpdir,
                          const char* successMsg)
{
    runCmd("rm -rf " + tmpdir, true);
    runCmd("mkdir -p " + tmpdir, true);

    std::cout << GREEN << "[*] Cloning " << url << RESET << "\n";
    if (runCmd("git clone " + url + " " + tmpdir) != 0) {
        std::cerr << RED << "[!] git clone failed: " << url << RESET << "\n";
        return;
    }
    fs::current_path(tmpdir);
    // if (runCmd("makepkg -si --noconfirm") == 0)
    if (runCmd("makepkg -si") == 0) {
        std::cout << GREEN << successMsg << RESET << "\n";
    } else {
        std::cerr << RED << "[!] makepkg failed in " << tmpdir << RESET << "\n";
    }
}

// Ensure ~/.config/tolito/tolito.conf exists and read the ask flag
static bool askBeforeAUR() {
    fs::path dir = fs::path(std::getenv("HOME")) / ".config" / "tolito";
    fs::path conf = dir / "tolito.conf";

    if (!fs::exists(dir)) {
        fs::create_directories(dir);
    }
    if (!fs::exists(conf)) {
        // write default
        std::ofstream out(conf);
        out << "# tolito configuration\n"
        "ask_before_fallback_into_aur = 1\n";
        return true;
    }

    std::ifstream in(conf);
    std::string line;
    while (std::getline(in, line)) {
        if (auto p = line.find('#'); p != std::string::npos) {
            line.erase(p);
        }
        if (auto eq = line.find('='); eq != std::string::npos) {
            std::string key = line.substr(0, eq);
            std::string val = line.substr(eq + 1);
            auto trim = [](std::string &s) {
                s.erase(s.begin(),
                        std::find_if(s.begin(), s.end(),
                                     [](char c){ return !std::isspace(c); }));
                s.erase(std::find_if(s.rbegin(), s.rend(),
                                     [](char c){ return !std::isspace(c); }).base(),
                        s.end());
            };
            trim(key); trim(val);
            if (key == "ask_before_fallback_into_aur") {
                std::transform(val.begin(), val.end(), val.begin(),
                               [](unsigned char c){ return std::tolower(c); });
                return (val == "1" || val == "true");
            }
        }
    }
    return false;
}

void installPkg(const std::string& spec) {
    // 1) Pre-flight
    if (runCmd("which git > /dev/null", true) != 0 ||
        runCmd("which makepkg > /dev/null", true) != 0)
    {
        std::cerr << RED << "[!] git or makepkg not in PATH" << RESET << "\n";
        return;
    }

    // 2) URL override
    if (isUrl(spec)) {
        cloneAndBuild(spec,
                      "/tmp/tolito_url",
                      "[✓] Installed from URL");
        return;
    }

    // 3) Try sparse-checkout from monorepo
    {
        const std::string tmp = "/tmp/tolito_monorepo";
        runCmd("rm -rf " + tmp, true);
        runCmd("git clone --depth 1 --filter=blob:none --sparse "
        + std::string(MONOREPO) + " " + tmp, true);
        runCmd("git -C " + tmp + " sparse-checkout set " + spec, true);

        fs::path pkgdir = fs::path(tmp) / spec;
        if (fs::exists(pkgdir / "PKGBUILD")) {
            fs::current_path(pkgdir);
            // if (runCmd("makepkg -si --noconfirm") == 0)
            if (runCmd("makepkg -si") == 0) {
                std::cout << GREEN
                << "[✓] Installed " << spec
                << " from curated GitLab repo\n"
                << RESET;
            } else {
                std::cerr << RED
                << "[!] build failed for " << spec
                << " from monorepo\n"
                << RESET;
            }
            return;
        }
    }

    // 4) Fallback to AUR, optional prompt
    bool ask = askBeforeAUR();
    if (ask) {
        std::cout << YELLOW
        << "Package '" << spec
        << "' not found in curated repo. Install from AUR? [Y/n] "
        << RESET << std::flush;
        std::string resp;
        std::getline(std::cin, resp);
        if (!resp.empty() && (resp[0]=='n' || resp[0]=='N')) {
            std::cout << GREEN
            << "Aborted '" << spec << "' install."
            << RESET << "\n";
            return;
        }
    }

    // 5) AUR clone & build
    std::string aurUrl = std::string(AUR_NS) + spec + ".git";
    cloneAndBuild(aurUrl,
                  "/tmp/tolito_aur_" + spec,
                  "[✓] Installed from AUR");
}
