#include "tolito-update.h"
#include "tolito-install.h"

#include <iostream>
#include <filesystem>
#include <fstream>
#include <map>
#include <vector>
#include <algorithm>
#include <sstream>
#include <regex>

namespace fs = std::filesystem;

// ANSI colors
static constexpr char RED[]    = "\033[31m";
static constexpr char GREEN[]  = "\033[32m";
static constexpr char YELLOW[] = "\033[33m";
static constexpr char RESET[]  = "\033[0m";

// Get installed packages with their sources
static std::map<std::string, std::string> getInstalledPackages() {
    std::map<std::string, std::string> packages;
    fs::path cfgdir = fs::path(std::getenv("HOME")) / ".config" / "tolito";
    fs::path sourceFile = cfgdir / "package_sources.json";
    
    if (!fs::exists(sourceFile)) {
        return packages;
    }
    
    std::ifstream in(sourceFile);
    if (!in) {
        return packages;
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
                if (!key.empty() && !val.empty()) {
                    packages[key] = val;
                }
            }
        }
    }
    return packages;
}

// Get current version of installed package
static std::string getCurrentVersion(const std::string& pkgName) {
    std::string cmd = "pacman -Q " + pkgName + " 2>/dev/null | awk '{print $2}'";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";
    
    char buffer[256];
    std::string result = "";
    if (fgets(buffer, sizeof(buffer), pipe)) {
        result = buffer;
        if (!result.empty() && result.back() == '\n') {
            result.pop_back();
        }
    }
    pclose(pipe);
    return result;
}

// Compare versions (simplified version comparison)
static int compareVersions(const std::string& v1, const std::string& v2) {
    if (v1 == v2) return 0;
    
    // Use vercmp if available
    std::string cmd = "vercmp '" + v1 + "' '" + v2 + "' 2>/dev/null";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (pipe) {
        char result[16];
        if (fgets(result, sizeof(result), pipe)) {
            int cmp = std::atoi(result);
            pclose(pipe);
            return cmp;
        }
        pclose(pipe);
    }
    
    // Fallback to string comparison
    return v1 < v2 ? -1 : 1;
}

// Get version from AUR
static std::string getAURVersion(const std::string& pkgName) {
    std::string cmd = "curl -s 'https://aur.archlinux.org/rpc/?v=5&type=info&arg=" + pkgName + "' | grep -o '\"Version\":\"[^\"]*\"' | cut -d'\"' -f4";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";
    
    char buffer[256];
    std::string result = "";
    if (fgets(buffer, sizeof(buffer), pipe)) {
        result = buffer;
        if (!result.empty() && result.back() == '\n') {
            result.pop_back();
        }
    }
    pclose(pipe);
    return result;
}

// Get version from curated repo
static std::string getCuratedVersion(const std::string& pkgName) {
    std::string homeDir = std::string(std::getenv("HOME"));
    std::string monorepoPath = homeDir + "/tolito/viper-pkgbuilds";
    std::string workDir = monorepoPath + "/" + pkgName;
    
    // Ensure monorepo exists and is updated
    if (!std::filesystem::exists(monorepoPath)) {
        std::string cloneCmd = "git clone --depth 1 --filter=blob:none --sparse https://github.com/Xray-OS/viper-pkgbuilds " + monorepoPath + " >/dev/null 2>&1";
        if (std::system(cloneCmd.c_str()) != 0) {
            return "";
        }
    }
    
    // Update repository and set sparse-checkout
    std::string updateCmd = "cd '" + monorepoPath + "' && git pull >/dev/null 2>&1 && git sparse-checkout init --cone >/dev/null 2>&1 && git sparse-checkout set " + pkgName + " >/dev/null 2>&1";
    std::system(updateCmd.c_str());
    
    if (!std::filesystem::exists(workDir + "/PKGBUILD")) {
        return "";
    }
    
    std::string cmd = "cd '" + workDir + "' && bash -c 'source PKGBUILD && echo $pkgver-$pkgrel' 2>/dev/null";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";
    
    char buffer[256];
    std::string result = "";
    if (fgets(buffer, sizeof(buffer), pipe)) {
        result = buffer;
        if (!result.empty() && result.back() == '\n') {
            result.pop_back();
        }
    }
    pclose(pipe);
    return result;
}

// Get version from repository (Chaotic)
static std::string getRepoVersion(const std::string& pkgName, const std::string& repoName) {
    std::string cacheDir = std::string(std::getenv("HOME")) + "/.cache/tolito/repos/" + repoName;
    std::string cmd = "find '" + cacheDir + "' -name 'desc' -exec grep -l '^" + pkgName + "$' {} \\; | head -1 | xargs dirname | xargs -I {} cat {}/desc | awk '/^%VERSION%/{getline; print; exit}'";
    
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";
    
    char buffer[256];
    std::string result = "";
    if (fgets(buffer, sizeof(buffer), pipe)) {
        result = buffer;
        if (!result.empty() && result.back() == '\n') {
            result.pop_back();
        }
    }
    pclose(pipe);
    return result;
}

// Read configuration (simplified version for update module)
static std::map<std::string, std::map<std::string, std::string>> readUpdateRules() {
    std::map<std::string, std::map<std::string, std::string>> rules;
    std::filesystem::path conf = std::filesystem::path(std::getenv("HOME")) / ".config" / "tolito" / "tolito.conf";
    
    if (!std::filesystem::exists(conf)) return rules;
    
    std::ifstream in(conf);
    std::string line, currentSection, currentRule;
    
    while (std::getline(in, line)) {
        if (auto c = line.find('#'); c != std::string::npos) line.erase(c);
        line.erase(line.begin(), std::find_if(line.begin(), line.end(), [](char c){ return !std::isspace(c); }));
        line.erase(std::find_if(line.rbegin(), line.rend(), [](char c){ return !std::isspace(c); }).base(), line.end());
        
        if (line.empty()) continue;
        
        if (line.front() == '[' && line.back() == ']') {
            currentSection = line.substr(1, line.length() - 2);
            currentRule = "";
        } else if (line.back() == ':' && currentSection == "UpdateRules") {
            currentRule = line.substr(0, line.length() - 1);
        } else if (auto eq = line.find('='); eq != std::string::npos && !currentRule.empty()) {
            std::string key = line.substr(0, eq);
            std::string val = line.substr(eq + 1);
            key.erase(key.begin(), std::find_if(key.begin(), key.end(), [](char c){ return !std::isspace(c); }));
            key.erase(std::find_if(key.rbegin(), key.rend(), [](char c){ return !std::isspace(c); }).base(), key.end());
            val.erase(val.begin(), std::find_if(val.begin(), val.end(), [](char c){ return !std::isspace(c); }));
            val.erase(std::find_if(val.rbegin(), val.rend(), [](char c){ return !std::isspace(c); }).base(), val.end());
            rules[currentRule][key] = val;
        }
    }
    return rules;
}

// Check for updates using priority rules
static std::string checkUpdateWithPriority(const std::string& pkgName, const std::string& currentSource, const std::string& currentVersion) {
    auto updateRules = readUpdateRules();
    std::string ruleKey = "_" + currentSource + "_";
    std::transform(ruleKey.begin(), ruleKey.end(), ruleKey.begin(), ::toupper);
    
    if (updateRules.find(ruleKey) == updateRules.end()) {
        return ""; // No rules for this source
    }
    
    auto& rule = updateRules[ruleKey];
    std::vector<std::string> sources = {rule["main"], rule["alternative"], rule["fallback"]};
    
    std::string bestVersion = currentVersion;
    std::string bestSource = currentSource;
    
    for (const auto& source : sources) {
        std::string version;
        
        if (source == "CURATED") {
            version = getCuratedVersion(pkgName);
        } else if (source == "AUR") {
            version = getAURVersion(pkgName);
        } else if (source == "CHAOTIC") {
            version = getRepoVersion(pkgName, "chaotic-aur");
        }
        
        if (!version.empty() && compareVersions(bestVersion, version) < 0) {
            bestVersion = version;
            bestSource = source;
        }
    }
    
    return bestVersion != currentVersion ? bestVersion + " (from " + bestSource + ")" : "";
}
std::vector<std::string> checkUpdates() {
    std::vector<std::string> updatesAvailable;
    auto installedPackages = getInstalledPackages();
    
    std::cout << YELLOW << "[*] Checking for updates..." << RESET << "\n";
    
    for (const auto& [pkgName, source] : installedPackages) {
        std::string currentVersion = getCurrentVersion(pkgName);
        if (currentVersion.empty()) continue;
        
        // Check for updates based on priority rules
        std::string updateInfo = checkUpdateWithPriority(pkgName, source, currentVersion);
        if (!updateInfo.empty()) {
            updatesAvailable.push_back(pkgName + " " + currentVersion + " -> " + updateInfo);
        }
    }
    
    return updatesAvailable;
}

int updatePkg(const std::string& spec) {
    if (spec.empty()) {
        // Update all packages
        auto updates = checkUpdates();
        if (updates.empty()) {
            std::cout << GREEN << "[✓] All packages are up to date" << RESET << "\n";
            return 1;
        }
        
        std::cout << YELLOW << "Updates available:" << RESET << "\n";
        for (const auto& update : updates) {
            std::cout << "  " << update << "\n";
        }
        
        std::cout << YELLOW << "\nProceed with updates? [Y/n] " << RESET << std::flush;
        std::string resp;
        std::getline(std::cin, resp);
        
        if (!resp.empty() && std::tolower(resp[0]) == 'n') {
            std::cout << YELLOW << "[*] Update cancelled by user" << RESET << "\n";
            return 2;
        }
        
        // Perform updates
        int successCount = 0;
        for (const auto& update : updates) {
            std::string pkgName = update.substr(0, update.find(' '));
            std::cout << YELLOW << "\n[*] Updating " << pkgName << "..." << RESET << "\n";
            
            // Use installPkg to handle the update (it will reinstall with newer version)
            // This leverages all existing logic for source detection and building
            std::string installCmd = "./tolito -S " + pkgName;
            if (std::system(installCmd.c_str()) == 0) {
                successCount++;
            }
        }
        
        std::cout << GREEN << "\n[✓] Updated " << successCount << "/" << updates.size() << " packages" << RESET << "\n";
        return successCount > 0 ? 1 : 0;
        
    } else {
        // Update specific package
        std::cout << YELLOW << "[*] Checking for updates for " << spec << RESET << "\n";
        
        auto installedPackages = getInstalledPackages();
        if (installedPackages.find(spec) == installedPackages.end()) {
            std::cout << RED << "[!] Package " << spec << " is not installed" << RESET << "\n";
            return 0;
        }
        
        std::string currentVersion = getCurrentVersion(spec);
        std::string source = installedPackages[spec];
        
        std::string updateInfo = checkUpdateWithPriority(spec, source, currentVersion);
        if (updateInfo.empty()) {
            std::cout << GREEN << "[✓] " << spec << " is up to date" << RESET << "\n";
            return 1;
        }
        
        std::cout << YELLOW << "Update available: " << spec << " " << currentVersion << " -> " << updateInfo << RESET << "\n";
        std::cout << YELLOW << "Proceed with update? [Y/n] " << RESET << std::flush;
        
        std::string resp;
        std::getline(std::cin, resp);
        
        if (!resp.empty() && std::tolower(resp[0]) == 'n') {
            std::cout << YELLOW << "[*] Update cancelled by user" << RESET << "\n";
            return 2;
        }
        
        // Perform update
        std::string installCmd = "./tolito -S " + spec;
        return std::system(installCmd.c_str()) == 0 ? 1 : 0;
    }
}