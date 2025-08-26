// src/tolito-install.cpp

#include "tolito-install.hpp"
#include "tolito-key.hpp"

#include <iostream>
#include <cstdlib>
#include <string>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <regex>
#include <cstdio>

namespace fs = std::filesystem;

// Helper: extract directory name from a Git URL (dropping “.git”)
static std::string getRepoName(const std::string& url) {
    auto pos = url.find_last_of("/\\");
    std::string name = (pos == std::string::npos
                        ? url
                        : url.substr(pos + 1));
    if (name.size() > 4 && name.substr(name.size() - 4) == ".git")
        name.resize(name.size() - 4);
    return name;
}

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
        || s.rfind("git@",    0) == 0;
}

// Ensure ~/.config/tolito/tolito.conf exists and read the ask flag
static bool askBeforeAUR() {
    fs::path cfgdir = fs::path(std::getenv("HOME")) / ".config" / "tolito";
    fs::path conf   = cfgdir / "tolito.conf";

    if (!fs::exists(cfgdir))
        fs::create_directories(cfgdir);

    if (!fs::exists(conf)) {
        std::ofstream out(conf);
        out << "# tolito configuration\n"
               "ask_before_fallback_into_aur = 1\n";
        return true;
    }

    std::ifstream in(conf);
    std::string line;
    while (std::getline(in, line)) {
        if (auto c = line.find('#'); c != std::string::npos) line.erase(c);
        if (auto eq = line.find('='); eq != std::string::npos) {
            std::string key = line.substr(0, eq),
                        val = line.substr(eq + 1);
            auto trim = [](std::string &s){
                s.erase(s.begin(),
                        std::find_if(s.begin(), s.end(),
                                     [](char c){ return !std::isspace(c); }));
                s.erase(std::find_if(s.rbegin(), s.rend(),
                                     [](char c){ return !std::isspace(c); }).base(),
                        s.end());
            };
            trim(key); trim(val);
            std::transform(val.begin(), val.end(), val.begin(),
                           [](unsigned char c){ return std::tolower(c); });
            if (key == "ask_before_fallback_into_aur")
                return (val == "1" || val == "true");
        }
    }
    return false;
}

// Determine a writable work directory under $HOME (~/tolito)
static fs::path getWorkDir() {
    const char* home = std::getenv("HOME");
    if (!home) {
        std::cerr << RED << "[!] $HOME not set\n" << RESET;
        std::exit(1);
    }
    fs::path dir = fs::path(home) / "tolito";
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (ec) {
        std::cerr << RED
                  << "[!] Cannot create workdir " << dir
                  << ": " << ec.message() << RESET << "\n";
        std::exit(1);
    }
    return dir;
}

// Build-only step: makepkg -s with PGP auto-import, no interactive install
static bool buildOnlyWithPgpHandling(const std::string& prevKey = "") {
    const char* cmd = "makepkg -s 2>&1";
    std::string output;
    FILE* pipe = popen(cmd, "r");
    if (!pipe) {
        std::cerr << RED << "[!] Failed to launch makepkg\n" << RESET;
        return false;
    }

    char buf[256];
    while (fgets(buf, sizeof(buf), pipe)) {
        output += buf;
        std::cout << buf;
    }
    int rc = pclose(pipe);
    if (rc == 0)
        return true;

    std::regex re(R"(unknown public key ([0-9A-F]+))", std::regex::icase);
    std::smatch m;
    if (std::regex_search(output, m, re) && m.size() >= 2) {
        std::string keyId = m[1].str();
        std::cout << YELLOW
                  << "[*] Missing PGP key " << keyId << ", importing..."
                  << RESET << "\n";
        if (keyId != prevKey && importPgpKey(keyId))
            return buildOnlyWithPgpHandling(keyId);
    }

    std::cerr << RED << "[!] makepkg build failed\n" << RESET;
    return false;
}

// Install-only step: interactive prompt with pacman -U
static bool runInstallInteractive(const std::string& successMsg) {
    // locate the built package file (*.pkg.tar.*)
    std::string pkgfile;
    for (auto &ent : fs::directory_iterator(fs::current_path())) {
        auto p = ent.path().string();
        if (p.find(".pkg.tar.") != std::string::npos) {
            pkgfile = p;
            break;
        }
    }
    if (pkgfile.empty()) {
        std::cerr << RED << "[!] Cannot locate built package file\n" << RESET;
        return false;
    }

    std::cout << YELLOW << "[~] sudo pacman -U " << pkgfile << RESET << "\n";
    int rc = std::system(("sudo pacman -U \"" + pkgfile + "\"").c_str());
    if (rc == 0) {
        std::cout << GREEN << successMsg << RESET << "\n";
        return true;
    }
    std::cerr << RED << "[!] pacman -U failed\n" << RESET;
    return false;
}

// Clone & two-step build/install from a URL or AUR
static void cloneAndBuild(const std::string& url,
                          const std::string& targetDir,
                          const std::string& successMsg)
{
    runCmd("rm -rf " + targetDir + "/*", true);
    runCmd("mkdir -p " + targetDir,    true);
    std::cout << GREEN << "[*] Cloning " << url << RESET << "\n";

    if (runCmd("git clone " + url + " " + targetDir) != 0) {
        std::cerr << RED << "[!] git clone failed: " << url << RESET << "\n";
        return;
    }

    fs::current_path(targetDir);

    if (!buildOnlyWithPgpHandling()) {
        std::cerr << RED << "[!] Build aborted in " << targetDir << RESET << "\n";
        return;
    }

    runInstallInteractive(successMsg);
}

void installPkg(const std::string& spec) {
    static const fs::path WORK = getWorkDir();
    const std::string workDir = WORK.string();

    // 1) Pre-flight checks
    if (runCmd("which git > /dev/null", true) != 0 ||
        runCmd("which makepkg > /dev/null", true) != 0)
    {
        std::cerr << RED << "[!] git or makepkg not in PATH\n" << RESET;
        return;
    }

    // 2) URL override — clone from any Git URL
    if (isUrl(spec)) {
        std::string repo = getRepoName(spec);
        fs::path target = WORK / repo;
        cloneAndBuild(spec, target.string(),
                      "[✓] Installed from URL");
        return;
    }

    // 3) Monorepo sparse-checkout
    runCmd("rm -rf " + workDir + "/*", true);
    runCmd(
        "git clone --depth 1 --filter=blob:none --sparse "
        + std::string(MONOREPO)
        + " " + workDir,
        true
    );
    runCmd("git -C " + workDir + " sparse-checkout set " + spec, true);

    fs::path pkgdir = WORK / spec;
    if (fs::exists(pkgdir / "PKGBUILD")) {
        fs::current_path(pkgdir);
        std::string msg = "[✓] Installed " + spec + " from curated GitLab repo";

        if (!buildOnlyWithPgpHandling()) {
            std::cerr << RED
                      << "[!] Build failed for " << spec
                      << " from monorepo\n" << RESET;
            return;
        }

        runInstallInteractive(msg);
        return;
    }

    // 4) Fallback to AUR
    if (askBeforeAUR()) {
        std::cout << YELLOW
                  << "Package '" << spec
                  << "' not found in curated repo. Install from AUR? [Y/n] "
                  << RESET << std::flush;
        std::string resp;
        std::getline(std::cin, resp);
        if (!resp.empty() &&
            (resp[0] == 'n' || resp[0] == 'N'))
        {
            std::cout << GREEN
                      << "Aborted '" << spec << "' install.\n"
                      << RESET;
            return;
        }
    }

    std::string aurUrl = std::string(AUR_NS) + spec + ".git";
    fs::path target   = WORK / spec;
    cloneAndBuild(aurUrl, target.string(),
                  "[✓] Installed from AUR");
}
