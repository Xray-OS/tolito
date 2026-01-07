#include "tolito-install.h"
#include "tolito-key.h"

#include <iostream>
#include <cstdlib>
#include <string>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <regex>
#include <cstdio>
#include <map>

namespace fs = std::filesystem;

// Helper: extract directory name from a Git URL (dropping ".git")

    // .. DEPRECATED VERSION OF THIS FUNCTION
    // ..
    // static std::string getRepoName(const std::string& url) {
    //     while (!url.empty() && (url.back() == '/' || url.back() == '\\')) {
    //         url.pop_back();
    //     }

    //     auto pos = url.find_last_of("/\\");
    //     std::string name = (pos == std::string::npos
    //                         ? url
    //                         : url.substr(pos + 1));
    //     if (name.size() > 4 && name.substr(name.size() -4) == ".git")
    //         name.resize(name.size() -4);
    //     return name;
    // }

static std::string getRepoName(const std::string& url) {
    fs::path p(url);

    // In case there is a trailing slash
    std::string name = p.filename().string();
    if (name.empty()) {
        name = p.parent_path().filename().string();
    }

    if (name.size() > 4 && name.substr(name.size() -4) == ".git") {
        name.resize(name.size() -4);
    }
    return name;
}

// constants
static constexpr char MONOREPO[] = "https://github.com/Xray-OS/viper-pkgbuilds";
static constexpr char AUR_NS[] = "https://aur.archlinux.org/";

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
static bool isUrl(const std::string& url) {
    return url.rfind("http://", 0) == 0
         ||url.rfind("https://", 0) == 0
         ||url.rfind("git@", 0) == 0;
}

// Configuration structure to hold settings
struct Config {
    bool askBeforeAUR = true;
    bool warnAboutAUR = true;
};

// Read configuration and return settings
static Config readConfig() {
    Config config;
    fs::path cfgdir = fs::path(std::getenv("HOME")) / ".config" / "tolito";
    fs::path conf   = cfgdir / "tolito.conf";

    if (!fs::exists(cfgdir))
        fs::create_directories(cfgdir);

    if (!fs::exists(conf)) {
        std::ofstream out(conf);
        if (!out) {
            std::cerr << RED << "[!] Failed to create config at " << conf << RESET << "\n";
            return config;
        }
        out << "# tolito configuration\n"
               "ask_before_fallback_into_aur = 1\n"
               "warn_about_aur_only = 1\n";
        return config;
    }

    std::ifstream in(conf);
    if (!in) {
        std::cerr << RED << "[!] Could not open configuration: " << conf << RESET << "\n";
        return config;
    }
    
    std::string line;
    while (std::getline(in, line)) {
        if (auto c = line.find('#'); c != std::string::npos) line.erase(c);
        if (auto eq = line.find('='); eq != std::string::npos) {
            std::string key = line.substr(0, eq),
                        val = line.substr(eq + 1);
            auto trim = [](std::string &s) {
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
            
            if (key == "ask_before_fallback_into_aur") {
                config.askBeforeAUR = (val == "1" || val == "true");
            } else if (key == "warn_about_aur_only") {
                config.warnAboutAUR = (val == "1" || val == "true");
            }
        }
    }
    return config;
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

// Check if package is already built
static bool isPackageBuilt() {
    std::string findCmd = "ls *.pkg.tar.* 2>/dev/null | head -1";
    FILE* pipe = popen(findCmd.c_str(), "r");
    bool found = false;
    if (pipe) {
        char buf[256];
        if (fgets(buf, sizeof(buf), pipe)) {
            found = true;
        }
        pclose(pipe);
    }
    return found;
}

// Check if package exists in AUR by trying to access the git repo
static bool packageExistsInAUR(const std::string& spec) {
    std::string aurUrl = std::string(AUR_NS) + spec + ".git";
    std::string checkCmd = "git ls-remote " + aurUrl + " > /dev/null 2>&1";
    return std::system(checkCmd.c_str()) == 0;
}

// Check if package is built in a specific directory
static bool isPackageBuiltInDir(const std::string& dir) {
    if (!fs::exists(dir)) return false;
    
    fs::path currentDir = fs::current_path();
    fs::current_path(dir);
    bool built = isPackageBuilt();
    fs::current_path(currentDir);
    return built;
}

// Handle choice between curated and AUR when both exist
static bool handleDualSourceChoice(const std::string& spec, const fs::path& WORK, const fs::path& pkgdir) {
    std::string aurDir = (WORK / spec).string();
    std::string curatedDir = pkgdir.string();
    
    bool aurBuilt = isPackageBuiltInDir(aurDir);
    bool curatedBuilt = isPackageBuiltInDir(curatedDir);
    
    std::cout << YELLOW << "[?] '" << spec << "' available both curated repositories and AUR repositories" << RESET << "\n";
    
    std::string aurOption = "[?] Try AUR?(AUR is unstable sometimes)";
    std::string curatedOption = "[?] Choose curated repos instead (Recommended)";
    
    if (aurBuilt && curatedBuilt) {
        std::cout << YELLOW << "[*] Both versions from every source are already built, whatever you choose the build process will be skipped" << RESET << "\n";
    } else if (aurBuilt) {
        aurOption += " (1 version of this pkg is already built locally)";
    } else if (curatedBuilt) {
        curatedOption += " (1 version of this pkg is already built locally)";
    }
    
    std::cout << YELLOW << aurOption << " : y" << RESET << "\n";
    std::cout << YELLOW << curatedOption << " : n" << RESET << "\n";
    std::cout << YELLOW << "\nYour choice:" << RESET << "\n";
    std::cout << YELLOW << "[Y/n] " << RESET << std::flush;
    
    std::string resp;
    std::getline(std::cin, resp);
    
    // Return true for AUR (Y), false for curated (n)
    if (!resp.empty()) {
        char c = std::tolower(static_cast<unsigned char>(resp[0]));
        return c != 'n'; // anything other than 'n' means AUR
    }
    return true; // default to AUR
}

// Build package only, no installation
static bool buildPackage(const std::string& prevKey = "") {
    std::string buildCmd = "makepkg -s";
    std::cout << YELLOW << "[~] " << buildCmd << RESET << "\n";
    int buildRc = std::system(buildCmd.c_str());
    
    if (buildRc != 0) {
        if (WEXITSTATUS(buildRc) == 2) {
            return false;
        }
        
        // Check for PGP key issues
        std::string output;
        FILE* pipe = popen("makepkg --nobuild 2>&1", "r");
        if (pipe) {
            char buf[256];
            while (fgets(buf, sizeof(buf), pipe)) {
                output += buf;
            }
            pclose(pipe);
            
            std::regex re(R"(unknown public key ([0-9A-F]+))", std::regex::icase);
            std::smatch m;
            if (std::regex_search(output, m, re) && m.size() >= 2) {
                std::string keyId = m[1].str();
                
                if (keyId != prevKey) {
                    std::cout << YELLOW << "[*] Missing PGP key " << keyId << ", importing..." << RESET << "\n";
                    if (fetchAndTrustgKey(keyId)) {
                        return buildPackage(keyId);
                    }
                }
            }
        }
        return false;
    }
    
    return true;
}

// Remove package from source records
void removePackageSource(const std::string& pkgName) {
    fs::path cfgdir = fs::path(std::getenv("HOME")) / ".config" / "tolito";
    fs::path sourceFile = cfgdir / "package_sources.json";
    
    if (!fs::exists(sourceFile)) {
        return;
    }
    
    // Read existing data
    std::map<std::string, std::string> sources;
    std::ifstream in(sourceFile);
    if (in) {
        std::string line;
        bool inObject = false;
        while (std::getline(in, line)) {
            if (line.find('{') != std::string::npos) inObject = true;
            if (line.find('}') != std::string::npos) break;
            if (inObject) {
                size_t colon = line.find(':');
                if (colon != std::string::npos) {
                    std::string key = line.substr(0, colon);
                    std::string val = line.substr(colon + 1);
                    key.erase(std::remove_if(key.begin(), key.end(), [](char c) { return c == '"' || std::isspace(c); }), key.end());
                    val.erase(std::remove_if(val.begin(), val.end(), [](char c) { return c == '"' || c == ',' || std::isspace(c); }), val.end());
                    if (!key.empty() && !val.empty()) {
                        sources[key] = val;
                    }
                }
            }
        }
        in.close();
    }
    
    // Remove the entry
    sources.erase(pkgName);
    
    // Write back to file
    std::ofstream out(sourceFile);
    if (out) {
        out << "{\n";
        for (const auto& [pkg, src] : sources) {
            out << "  \"" << pkg << "\": \"" << src << "\",\n";
        }
        if (!sources.empty()) {
            out.seekp(-2, std::ios_base::cur);
        }
        out << "\n}\n";
    }
}
static void recordPackageSource(const std::string& pkgName, const std::string& source) {
    fs::path cfgdir = fs::path(std::getenv("HOME")) / ".config" / "tolito";
    fs::path sourceFile = cfgdir / "package_sources.json";
    
    if (!fs::exists(cfgdir)) {
        fs::create_directories(cfgdir);
    }
    
    // Read existing data
    std::map<std::string, std::string> sources;
    if (fs::exists(sourceFile)) {
        std::ifstream in(sourceFile);
        if (in) {
            std::string line;
            bool inObject = false;
            while (std::getline(in, line)) {
                // Simple JSON parsing for key-value pairs
                if (line.find('{') != std::string::npos) inObject = true;
                if (line.find('}') != std::string::npos) break;
                if (inObject) {
                    size_t colon = line.find(':');
                    if (colon != std::string::npos) {
                        std::string key = line.substr(0, colon);
                        std::string val = line.substr(colon + 1);
                        // Remove quotes and whitespace
                        key.erase(std::remove_if(key.begin(), key.end(), [](char c) { return c == '"' || std::isspace(c); }), key.end());
                        val.erase(std::remove_if(val.begin(), val.end(), [](char c) { return c == '"' || c == ',' || std::isspace(c); }), val.end());
                        if (!key.empty() && !val.empty()) {
                            sources[key] = val;
                        }
                    }
                }
            }
        }
    }
    
    // Update the entry
    sources[pkgName] = source;
    
    // Write back to file
    std::ofstream out(sourceFile);
    if (out) {
        out << "{\n";
        for (const auto& [pkg, src] : sources) {
            out << "  \"" << pkg << "\": \"" << src << "\",\n";
        }
        // Remove last comma and close
        out.seekp(-2, std::ios_base::cur);
        out << "\n}\n";
    }
}

// Get package installation source from records
static std::string getRecordedPackageSource(const std::string& pkgName) {
    fs::path cfgdir = fs::path(std::getenv("HOME")) / ".config" / "tolito";
    fs::path sourceFile = cfgdir / "package_sources.json";
    
    if (!fs::exists(sourceFile)) {
        return "Unknown";
    }
    
    std::ifstream in(sourceFile);
    if (!in) {
        return "Unknown";
    }
    
    std::string line;
    bool inObject = false;
    while (std::getline(in, line)) {
        if (line.find('{') != std::string::npos) inObject = true;
        if (line.find('}') != std::string::npos) break;
        if (inObject) {
            size_t colon = line.find(':');
            if (colon != std::string::npos) {
                std::string key = line.substr(0, colon);
                std::string val = line.substr(colon + 1);
                // Remove quotes and whitespace
                key.erase(std::remove_if(key.begin(), key.end(), [](char c) { return c == '"' || std::isspace(c); }), key.end());
                val.erase(std::remove_if(val.begin(), val.end(), [](char c) { return c == '"' || c == ',' || std::isspace(c); }), val.end());
                if (key == pkgName && (val == "AUR" || val == "Curated")) {
                    return val;
                }
            }
        }
    }
    return "Unknown";
}

// Check if package is already installed
static bool isPackageInstalled(const std::string& pkgName) {
    std::string checkCmd = "pacman -Q " + pkgName + " > /dev/null 2>&1";
    return std::system(checkCmd.c_str()) == 0;
}

// Check if package is already installed and get its source
static std::string getPackageSource(const std::string& pkgName) {
    if (!isPackageInstalled(pkgName)) {
        return ""; // Not installed
    }
    
    std::string source = getRecordedPackageSource(pkgName);
    if (source == "AUR") {
        return "AUR repositories";
    } else if (source == "Curated") {
        return "Curated repositories";
    } else {
        return "AUR or Curated repositories (source not tracked)";
    }
}

// Install built package with proper user interaction handling
static int installBuiltPackage(const std::string& expectedPkgName = "") {
    // Find the built package file
    std::string findCmd = "ls *.pkg.tar.* 2>/dev/null | head -1";
    FILE* pipe = popen(findCmd.c_str(), "r");
    std::string pkgFile;
    if (pipe) {
        char buf[256];
        if (fgets(buf, sizeof(buf), pipe)) {
            pkgFile = buf;
            if (!pkgFile.empty() && pkgFile.back() == '\n') {
                pkgFile.pop_back();
            }
        }
        pclose(pipe);
    }
    
    if (pkgFile.empty()) {
        std::cerr << RED << "[!] No package file found" << RESET << "\n";
        return 0; // Failure
    }
    
    // Use expected package name if provided, otherwise extract from filename
    std::string pkgName = expectedPkgName;
    if (pkgName.empty()) {
        pkgName = pkgFile;
        size_t dashPos = pkgName.find('-');
        if (dashPos != std::string::npos) {
            pkgName = pkgName.substr(0, dashPos);
        }
    }
    
    // Check if package is already installed
    if (isPackageInstalled(pkgName)) {
        std::cout << GREEN << "[✓] Package '" << pkgName << "' is already installed" << RESET << "\n";
        return 1; // Success (already installed)
    }
    
    std::string installCmd = "sudo pacman -U " + pkgFile;
    std::cout << YELLOW << "[~] " << installCmd << RESET << "\n";
    int installRc = std::system(installCmd.c_str());
    
    if (installRc == 0) {
        return 1; // Success
    } else {
        return 2; // Declined
    }
}

static int cloneAndBuild(const std::string& url, const std::string& targetDir, const std::string& source = "AUR") {
    // Check if directory exists and has built packages
    bool hasBuiltPkg = false;
    if (fs::exists(targetDir)) {
        fs::path currentDir = fs::current_path();
        fs::current_path(targetDir);
        hasBuiltPkg = isPackageBuilt();
        fs::current_path(currentDir);
        
        if (hasBuiltPkg) {
            std::cout << YELLOW << "[*] Package already built, skipping clone and build" << RESET << "\n";
        }
    }
    
    if (!hasBuiltPkg) {
        runCmd("rm -rf " + targetDir, true);
        std::cout << GREEN << "[*] Cloning " << url << RESET << "\n";

        if (runCmd("git clone " + url + " " + targetDir) != 0) {
            std::cerr << RED << "[!] git clone failed\n" << RESET;
            return 0; // Failure
        }

        fs::current_path(targetDir);
        
        // Build the package
        if (!buildPackage()) {
            std::cerr << RED << "[!] Build failed" << RESET << "\n";
            return 0; // Failure
        }
    } else {
        fs::current_path(targetDir);
    }
    
    // Install the package
    fs::path dirPath(targetDir);
    std::string pkgName = dirPath.filename().string();
    int installResult = installBuiltPackage(pkgName);
    if (installResult == 1) {
        recordPackageSource(pkgName, source);
        
        if (!hasBuiltPkg) {
            std::cout << GREEN << "[✓] Package installed successfully!" << RESET << "\n";
        }
        return 1; // Success
    } else if (installResult == 2) {
        std::cout << YELLOW << "[*] Installation declined by user" << RESET << "\n";
        return 2; // Declined
    } else {
        return 0; // Failure
    }
}

int installPkg(const std::string &spec) {
    static const fs::path WORK = getWorkDir();
    const std::string workDir = WORK.string();
    const fs::path originalPath = fs::current_path();

    // Check if package is already installed
    std::string source = getPackageSource(spec);
    if (!source.empty()) {
        std::cout << GREEN << "[✓] Package '" << spec << "' is already installed from " << source << RESET << "\n";
        return 3; // Already installed (not processed)
    }

    // 1. Pre-flight (Fixed return)
    if (runCmd("which git > /dev/null", true) != 0 ||
        runCmd("which makepkg > /dev/null", true) != 0) {
            std::cerr << RED << "[!] git or makepkg not found\n" << RESET;
            return 0; // Failure
        }

    // 2. Direct URL (Fixed return)
    if (isUrl(spec)) {
        int success = cloneAndBuild(spec, (WORK / getRepoName(spec)).string());
        fs::current_path(originalPath);
        return success;
    }

    // 3. Monorepo Logic
    fs::path monorepoPath = WORK / "viper-pkgbuilds";
    if (!fs::exists(monorepoPath)) {
        runCmd("git clone --depth 1 --filter=blob:none --sparse " +
            std::string(MONOREPO) + " " + monorepoPath.string(), true);
    }

    // Check if sparse-checkout is already initialized
    std::string checkSparseCmd = "git -C " + monorepoPath.string() + " config core.sparseCheckout";
    bool sparseInitialized = (std::system((checkSparseCmd + " > /dev/null 2>&1").c_str()) == 0);
    
    if (!sparseInitialized) {
        runCmd("git -C " + monorepoPath.string() + " sparse-checkout init --cone", true);
    }
    
    // Clean untracked files to avoid sparse-checkout warnings
    runCmd("git -C " + monorepoPath.string() + " clean -fd > /dev/null 2>&1", true);
    runCmd("git -C " + monorepoPath.string() + " sparse-checkout set " + spec, true);
    fs::path pkgdir = monorepoPath / spec;

    if (fs::exists(pkgdir / "PKGBUILD")) {
        std::cout << GREEN << "[*] Found " << spec << " in curated repo." << RESET << "\n";
        
        // Check if package also exists in AUR for dual source handling
        Config config = readConfig();
        if (config.askBeforeAUR && packageExistsInAUR(spec)) {
            bool useAUR = handleDualSourceChoice(spec, WORK, pkgdir);
            
            if (useAUR) {
                // User chose AUR, proceed to AUR section
                std::string aurUrl = std::string(AUR_NS) + spec + ".git";
                int success = cloneAndBuild(aurUrl, (WORK / spec).string());
                fs::current_path(originalPath);
                return success;
            }
            // User chose curated, continue with curated repo logic below
        }
        
        fs::current_path(pkgdir);

        // Check if package is already built
        if (isPackageBuilt()) {
            std::cout << YELLOW << "[*] Package already built, skipping build step" << RESET << "\n";
        } else {
            // Build the package
            if (!buildPackage()) {
                std::cerr << RED << "[!] Failed to build " << spec << " from curated repo." << RESET << "\n";
                fs::current_path(originalPath);
                return 0; // Failure
            }
        }
        
        // Install the package
        int installed = installBuiltPackage(spec);
        fs::current_path(originalPath);
        
        if (installed == 1) {
            recordPackageSource(spec, "Curated");
            std::cout << GREEN << "[✓] Installed " << spec << " from curated repo." << RESET << "\n";
            return 1; // Success
        } else if (installed == 2) {
            std::cout << YELLOW << "[*] Installation declined by user" << RESET << "\n";
            return 2; // Declined
        } else {
            return 0; // Failure
        }
    }

    // 4. Fallback to AUR
    Config config = readConfig();
    
    if (config.askBeforeAUR) {
        std::string prompt = "[?] '" + spec + "' not in curated repo. Try AUR? [Y/n]";
        if (config.warnAboutAUR) {
            prompt += " (Unstable sometimes)";
        }
        
        std::cout << YELLOW << prompt << RESET << std::flush;
        std::string resp;
        std::getline(std::cin, resp);

        if (!resp.empty()) {
            char c = std::tolower(static_cast<unsigned char>(resp[0]));
            if (c == 'n') return 2; // User declined AUR
        }
    } else if (config.warnAboutAUR) {
        // Show warning but don't ask, proceed automatically
        std::cout << YELLOW << "[*] '" << spec << "' not in curated repo. Trying AUR (Unstable sometimes)..." << RESET << "\n";
    }

    std::string aurUrl = std::string(AUR_NS) + spec + ".git";
    int success = cloneAndBuild(aurUrl, (WORK / spec).string());

    fs::current_path(originalPath);
    return success;
}
