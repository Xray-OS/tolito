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
#include <vector>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <curl/curl.h>
#include <cstring>
#include <sys/ioctl.h>
#include <unistd.h>

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

// Repository configuration
struct Repository {
    std::string name;
    std::vector<std::string> servers;
    std::string siglevel;
    std::string includePath;
};

// Parse mirrorlist file and return servers
static std::vector<std::string> parseMirrorlist(const std::string& path) {
    std::vector<std::string> servers;
    std::ifstream file(path);
    std::string line;
    
    while (std::getline(file, line)) {
        // Remove comments
        if (auto c = line.find('#'); c != std::string::npos) line.erase(c);
        
        // Trim whitespace
        line.erase(line.begin(), std::find_if(line.begin(), line.end(), [](char c){ return !std::isspace(c); }));
        line.erase(std::find_if(line.rbegin(), line.rend(), [](char c){ return !std::isspace(c); }).base(), line.end());
        
        if (line.empty()) continue;
        
        // Look for Server = URL lines
        if (line.find("Server") == 0) {
            size_t eq = line.find('=');
            if (eq != std::string::npos) {
                std::string url = line.substr(eq + 1);
                url.erase(url.begin(), std::find_if(url.begin(), url.end(), [](char c){ return !std::isspace(c); }));
                url.erase(std::find_if(url.rbegin(), url.rend(), [](char c){ return !std::isspace(c); }).base(), url.end());
                if (!url.empty()) {
                    servers.push_back(url);
                }
            }
        }
    }
    return servers;
}

// Update rules configuration
struct UpdateRule {
    std::string main;
    std::string alternative;
    std::string fallback;
    bool getFromAUR = false;
    bool getFromChaotic = false;
    bool getFromCurated = false;
};

// Configuration structure to hold settings
struct Config {
    bool askBeforeAUR = true;
    bool warnAboutAUR = true;
    bool askBeforeSwitchSources = false;
    std::map<std::string, UpdateRule> updateRules;
    std::map<std::string, Repository> repositories;
    // Misc options
    bool iLoveCandy = false;
    bool disableDownloadTimeout = false;
    bool color = true;
};

// Pacman-compatible progress callback (based on pacman source)
static int progressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t /*ultotal*/, curl_off_t /*ulnow*/) {
    if (dltotal <= 0) return 0;
    
    auto* data = static_cast<std::pair<std::string, Config>*>(clientp);
    const std::string& filename = data->first;
    const Config& config = data->second;
    
    static curl_off_t last_dlnow = 0;
    static auto last_update = std::chrono::steady_clock::now();
    static auto start_time = std::chrono::steady_clock::now();
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update).count();
    
    // Only update if significant change or time passed
    if (dlnow != dltotal && dlnow == last_dlnow && elapsed_ms < 200) {
        return 0;
    }
    
    last_dlnow = dlnow;
    last_update = now;
    
    double percentage = (double)dlnow / (double)dltotal * 100.0;
    double mb = dlnow / (1024.0 * 1024.0);
    
    // Calculate speed
    auto total_elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
    double speed = (total_elapsed > 0) ? (double)dlnow / total_elapsed : 0;
    int speed_kb = (int)(speed / 1024);
    
    // Calculate ETA
    int eta_sec = (speed > 0 && dlnow < dltotal) ? (int)((dltotal - dlnow) / speed) : 0;
    int eta_min = eta_sec / 60;
    eta_sec %= 60;
    
    struct winsize w;
    int term_width = (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) ? w.ws_col : 80;
    
    // Calculate info length for proper bar width
    char info_buf[256];
    snprintf(info_buf, sizeof(info_buf), "%s %.1f MiB %d KiB/s %02d:%02d ", 
             filename.c_str(), mb, speed_kb, eta_min, eta_sec);
    
    int info_len = strlen(info_buf);
    int bar_width = term_width - info_len - 8; // -8 for [ ] and " 100%"
    if (bar_width < 10) bar_width = 10;
    
    int filled = (int)(percentage * bar_width / 100.0);
    
    // Clear line and print progress with colors
    printf("\r\033[2K");
    
    if (config.color) {
        // Package name in cyan, values in white, units in cyan
        printf("\033[1;36m%s\033[0m \033[0;37m%.1f\033[0m \033[1;36mMiB\033[0m \033[0;37m%d\033[0m \033[1;36mKiB/s\033[0m \033[0;37m%02d:%02d\033[0m [",
               filename.c_str(), mb, speed_kb, eta_min, eta_sec);
    } else {
        printf("%s[", info_buf);
    }
    
    if (config.iLoveCandy) {
        // Pac-Man style progress bar
        for (int i = 0; i < bar_width; ++i) {
            if (i < filled - 1) {
                if (config.color) {
                    printf("\033[1;33m-\033[0m");
                } else {
                    printf("-");
                }
            } else if (i == filled - 1 && filled > 0) {
                if (config.color) {
                    printf("\033[1;33m%s\033[0m", ((int)percentage % 2 == 0) ? "C" : "c");
                } else {
                    printf("%s", ((int)percentage % 2 == 0) ? "C" : "c");
                }
            } else if (i % 3 == 0) {
                if (config.color) {
                    printf("\033[0;37mo\033[0m");
                } else {
                    printf("o");
                }
            } else {
                printf(" ");
            }
        }
    } else {
        // Standard progress bar
        for (int i = 0; i < bar_width; ++i) {
            if (i < filled) {
                if (config.color) {
                    printf("\033[1;32m#\033[0m");
                } else {
                    printf("#");
                }
            } else {
                printf("-");
            }
        }
    }
    
    if (config.color) {
        printf("] \033[0;37m%3.0f%%\033[0m", percentage);
    } else {
        printf("] %3.0f%%", percentage);
    }
    fflush(stdout);
    
    return 0;
}

// Download file with libcurl and pacman-style progress
static bool downloadWithProgress(const std::string& url, const std::string& output, const Config& config, const std::string& filename = "") {
    CURL* curl = curl_easy_init();
    if (!curl) return false;
    
    FILE* fp = fopen(output.c_str(), "wb");
    if (!fp) {
        curl_easy_cleanup(curl);
        return false;
    }
    
    std::string display_name = filename;
    if (display_name.empty()) {
        display_name = output.substr(output.find_last_of('/') + 1);
    }
    if (display_name.find(".pkg.tar") != std::string::npos) {
        display_name = display_name.substr(0, display_name.find(".pkg.tar"));
    }
    
    std::pair<std::string, Config> progress_data = {display_name, config};
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    
    if (!config.disableDownloadTimeout) {
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);
    }
    
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progressCallback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progress_data);
    
    CURLcode res = curl_easy_perform(curl);
    
    fclose(fp);
    curl_easy_cleanup(curl);
    
    if (res == CURLE_OK) {
        std::cout << "\n";
        return true;
    } else {
        return false;
    }
}

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
               "warn_about_aur_only = 1\n\n"
               "[UpdateRules]\n"
               "_CURATED_:\n"
               "getFromAUR=true\n"
               "getFromChaotic=true\n"
               "main=CURATED\n"
               "alternative=AUR\n"
               "fallback=CHAOTIC\n\n"
               "_AUR_:\n"
               "getFromCurated=true\n"
               "getFromChaotic=true\n"
               "main=AUR\n"
               "alternative=CHAOTIC\n"
               "fallback=CURATED\n\n"
               "_CHAOTIC_:\n"
               "getFromCurated=true\n"
               "getFromAUR=true\n"
               "main=CHAOTIC\n"
               "alternative=AUR\n"
               "fallback=CURATED\n\n"
               "askBeforeSwitchSources=false\n\n"
               "[repositories]\n"
               "chaotic-aur:\n"
               "Include=/home/$USER/.config/tolito/tolito.d/chaotic-mirrorlist\n"
               "SigLevel=Optional\n";
        return config;
    }

    std::ifstream in(conf);
    if (!in) {
        std::cerr << RED << "[!] Could not open configuration: " << conf << RESET << "\n";
        return config;
    }
    
    std::string line;
    std::string currentSection = "";
    std::string currentRule = "";
    std::string currentRepo = "";
    
    while (std::getline(in, line)) {
        if (auto c = line.find('#'); c != std::string::npos) line.erase(c);
        
        // Trim whitespace
        line.erase(line.begin(), std::find_if(line.begin(), line.end(), [](char c){ return !std::isspace(c); }));
        line.erase(std::find_if(line.rbegin(), line.rend(), [](char c){ return !std::isspace(c); }).base(), line.end());
        
        if (line.empty()) continue;
        
        // Check for section headers
        if (line.front() == '[' && line.back() == ']') {
            currentSection = line.substr(1, line.length() - 2);
            currentRule = "";
            currentRepo = "";
            continue;
        }
        
        // Check for rule/repo names (ending with :)
        if (line.back() == ':') {
            if (currentSection == "UpdateRules") {
                currentRule = line.substr(0, line.length() - 1);
            } else if (currentSection == "repositories") {
                currentRepo = line.substr(0, line.length() - 1);
            }
            continue;
        }
        
        // Parse key=value pairs
        if (auto eq = line.find('='); eq != std::string::npos) {
            std::string key = line.substr(0, eq);
            std::string val = line.substr(eq + 1);
            
            // Trim key and value
            key.erase(key.begin(), std::find_if(key.begin(), key.end(), [](char c){ return !std::isspace(c); }));
            key.erase(std::find_if(key.rbegin(), key.rend(), [](char c){ return !std::isspace(c); }).base(), key.end());
            val.erase(val.begin(), std::find_if(val.begin(), val.end(), [](char c){ return !std::isspace(c); }));
            val.erase(std::find_if(val.rbegin(), val.rend(), [](char c){ return !std::isspace(c); }).base(), val.end());
            
            // Convert key to lowercase for comparison
            std::string lowerKey = key;
            std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(), [](unsigned char c){ return std::tolower(c); });
            std::transform(val.begin(), val.end(), val.begin(), [](unsigned char c){ return std::tolower(c); });
            
            // Parse global settings
            if (currentSection.empty()) {
                if (lowerKey == "ask_before_fallback_into_aur") {
                    config.askBeforeAUR = (val == "1" || val == "true");
                } else if (lowerKey == "warn_about_aur_only") {
                    config.warnAboutAUR = (val == "1" || val == "true");
                } else if (lowerKey == "askbeforeswitchsources") {
                    config.askBeforeSwitchSources = (val == "1" || val == "true");
                }
            }
            // Parse Misc section
            else if (currentSection == "Misc") {
                if (lowerKey == "ilovecandy") {
                    config.iLoveCandy = (val == "1" || val == "true");
                } else if (lowerKey == "disabledownloadtimeout") {
                    config.disableDownloadTimeout = (val == "1" || val == "true");
                } else if (lowerKey == "color") {
                    config.color = (val == "1" || val == "true");
                }
            }
            // Parse UpdateRules
            else if (currentSection == "UpdateRules" && !currentRule.empty()) {
                if (lowerKey == "getfromaur") {
                    config.updateRules[currentRule].getFromAUR = (val == "true");
                } else if (lowerKey == "getfromchaotic") {
                    config.updateRules[currentRule].getFromChaotic = (val == "true");
                } else if (lowerKey == "getfromcurated") {
                    config.updateRules[currentRule].getFromCurated = (val == "true");
                } else if (lowerKey == "main") {
                    config.updateRules[currentRule].main = val;
                    std::transform(config.updateRules[currentRule].main.begin(), config.updateRules[currentRule].main.end(), config.updateRules[currentRule].main.begin(), ::toupper);
                } else if (lowerKey == "alternative") {
                    config.updateRules[currentRule].alternative = val;
                    std::transform(config.updateRules[currentRule].alternative.begin(), config.updateRules[currentRule].alternative.end(), config.updateRules[currentRule].alternative.begin(), ::toupper);
                } else if (lowerKey == "fallback") {
                    config.updateRules[currentRule].fallback = val;
                    std::transform(config.updateRules[currentRule].fallback.begin(), config.updateRules[currentRule].fallback.end(), config.updateRules[currentRule].fallback.begin(), ::toupper);
                }
            }
            // Parse repositories
            else if (currentSection == "repositories" && !currentRepo.empty()) {
                if (lowerKey == "servers") {
                    // Split comma-separated servers
                    std::stringstream ss(val);
                    std::string server;
                    while (std::getline(ss, server, ',')) {
                        // Trim server
                        server.erase(server.begin(), std::find_if(server.begin(), server.end(), [](char c){ return !std::isspace(c); }));
                        server.erase(std::find_if(server.rbegin(), server.rend(), [](char c){ return !std::isspace(c); }).base(), server.end());
                        if (!server.empty()) {
                            config.repositories[currentRepo].servers.push_back(server);
                        }
                    }
                    config.repositories[currentRepo].name = currentRepo;
                } else if (lowerKey == "siglevel") {
                    config.repositories[currentRepo].siglevel = val;
                } else if (lowerKey == "include") {
                    // Don't lowercase the include path value
                    std::string originalVal = line.substr(line.find('=') + 1);
                    originalVal.erase(originalVal.begin(), std::find_if(originalVal.begin(), originalVal.end(), [](char c){ return !std::isspace(c); }));
                    originalVal.erase(std::find_if(originalVal.rbegin(), originalVal.rend(), [](char c){ return !std::isspace(c); }).base(), originalVal.end());
                    config.repositories[currentRepo].includePath = originalVal;
                    config.repositories[currentRepo].name = currentRepo;
                }
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

// Handle choice between curated, AUR, and repository when multiple sources exist

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

// Package information structure
struct PackageInfo {
    std::string name;
    std::string version;
    std::string description;
    std::vector<std::string> depends;
    std::string filename;
};

// Parse package description file
static PackageInfo parsePackageDesc(const std::string& descFile) {
    PackageInfo pkg;
    std::ifstream file(descFile);
    std::string line;
    std::string currentSection;
    
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        
        if (line.front() == '%' && line.back() == '%') {
            currentSection = line.substr(1, line.length() - 2);
        } else if (currentSection == "NAME") {
            pkg.name = line;
        } else if (currentSection == "VERSION") {
            pkg.version = line;
        } else if (currentSection == "DESC") {
            pkg.description = line;
        } else if (currentSection == "DEPENDS") {
            pkg.depends.push_back(line);
        } else if (currentSection == "FILENAME") {
            pkg.filename = line;
        }
    }
    return pkg;
}

// Get system architecture
static std::string getSystemArch() {
    FILE* pipe = popen("uname -m", "r");
    if (!pipe) return "x86_64";
    
    char buffer[128];
    std::string result = "";
    if (fgets(buffer, sizeof(buffer), pipe)) {
        result = buffer;
        if (!result.empty() && result.back() == '\n') {
            result.pop_back();
        }
    }
    pclose(pipe);
    return result.empty() ? "x86_64" : result;
}

// Replace variables in repository URL
static std::string replaceRepoVars(const std::string& url, const std::string& repo, const std::string& arch) {
    std::string result = url;
    size_t pos = 0;
    while ((pos = result.find("$repo", pos)) != std::string::npos) {
        result.replace(pos, 5, repo);
        pos += repo.length();
    }
    pos = 0;
    while ((pos = result.find("$arch", pos)) != std::string::npos) {
        result.replace(pos, 5, arch);
        pos += arch.length();
    }
    return result;
}

// Test mirror speed and return response time in ms
static int testMirrorSpeed(const std::string& url, const std::string& repo, const std::string& arch) {
    std::string testUrl = replaceRepoVars(url, repo, arch) + "/" + repo + ".db";
    std::string testCmd = "curl -s -w '%{time_total}' -o /dev/null --connect-timeout 5 --max-time 10 '" + testUrl + "' 2>/dev/null";
    
    FILE* pipe = popen(testCmd.c_str(), "r");
    if (!pipe) return 9999;
    
    char buffer[32];
    std::string result;
    if (fgets(buffer, sizeof(buffer), pipe)) {
        result = buffer;
    }
    pclose(pipe);
    
    try {
        double seconds = std::stod(result);
        return static_cast<int>(seconds * 1000); // Convert to ms
    } catch (...) {
        return 9999; // Failed
    }
}

// Select best mirror from list (reorder, don't remove)
static std::vector<std::string> selectBestMirrors(const std::vector<std::string>& mirrors, const std::string& repo) {
    if (mirrors.empty()) return mirrors;
    
    std::string arch = getSystemArch();
    std::vector<std::pair<std::string, int>> mirrorTimes;
    
    std::cout << YELLOW << ":: Fetching best mirrors..." << RESET << std::flush;
    
    for (const auto& mirror : mirrors) {
        int time = testMirrorSpeed(mirror, repo, arch);
        mirrorTimes.push_back({mirror, time});
        std::cout << "." << std::flush;
    }
    
    // Sort by response time (working mirrors first, then failed ones)
    std::sort(mirrorTimes.begin(), mirrorTimes.end(), 
              [](const auto& a, const auto& b) { 
                  if (a.second < 9999 && b.second >= 9999) return true;
                  if (a.second >= 9999 && b.second < 9999) return false;
                  return a.second < b.second;
              });
    
    std::vector<std::string> sortedMirrors;
    for (const auto& [mirror, time] : mirrorTimes) {
        sortedMirrors.push_back(mirror);
    }
    
    std::cout << " done" << RESET << "\n";
    return sortedMirrors;
}

// Global cache for repository databases
static std::map<std::string, std::map<std::string, PackageInfo>> repoCache;

// Download and parse repository database
static std::map<std::string, PackageInfo> downloadRepoDatabase(const Repository& repo, bool silent = false) {
    // Check cache first
    if (repoCache.find(repo.name) != repoCache.end()) {
        return repoCache[repo.name];
    }
    
    std::map<std::string, PackageInfo> packages;
    std::string arch = getSystemArch();
    fs::path cacheDir = fs::path(std::getenv("HOME")) / ".cache" / "tolito" / "repos";
    fs::create_directories(cacheDir);
    
    // Get servers from Include path or use configured servers
    std::vector<std::string> servers = repo.servers;
    if (!repo.includePath.empty()) {
        // Expand $USER in path
        std::string expandedPath = repo.includePath;
        if (expandedPath.find("$USER") != std::string::npos) {
            std::string user = std::getenv("USER") ? std::getenv("USER") : "";
            size_t pos = 0;
            while ((pos = expandedPath.find("$USER", pos)) != std::string::npos) {
                expandedPath.replace(pos, 5, user);
                pos += user.length();
            }
        }
        
        if (fs::exists(expandedPath)) {
            auto mirrorServers = parseMirrorlist(expandedPath);
            if (!mirrorServers.empty()) {
                servers = selectBestMirrors(mirrorServers, repo.name);
            }
        } else {
            std::cerr << RED << "[!] Mirrorlist not found: " << expandedPath << RESET << "\n";
        }
    }
    
    for (const auto& serverUrl : servers) {
        std::string url = replaceRepoVars(serverUrl, repo.name, arch);
        std::string dbUrl = url + "/" + repo.name + ".db";
        std::string dbFile = (cacheDir / (repo.name + ".db")).string();
        
        // Download database file with timeout
        std::string downloadCmd = "curl --connect-timeout 10 --max-time 60 -s -L \"" + dbUrl + "\" -o \"" + dbFile + "\"";
        
        if (std::system(downloadCmd.c_str()) == 0 && fs::exists(dbFile) && fs::file_size(dbFile) > 0) {
            // Extract and parse database
            std::string extractDir = (cacheDir / repo.name).string();
            std::string extractCmd = "rm -rf \"" + extractDir + "\" && mkdir -p \"" + extractDir + "\" && tar -xf \"" + dbFile + "\" -C \"" + extractDir + "\" 2>/dev/null";
            
            if (std::system(extractCmd.c_str()) == 0) {
                // Parse extracted package directories
                try {
                    for (const auto& entry : fs::directory_iterator(extractDir)) {
                        if (entry.is_directory()) {
                            fs::path descFile = entry.path() / "desc";
                            if (fs::exists(descFile)) {
                                PackageInfo pkg = parsePackageDesc(descFile.string());
                                if (!pkg.name.empty()) {
                                    packages[pkg.name] = pkg;
                                }
                            }
                        }
                    }
                } catch (const std::exception& e) {
                    std::cerr << RED << "[!] Error parsing database: " << e.what() << RESET << "\n";
                    continue;
                }
                
                if (!packages.empty()) {
                    // Cache the result
                    repoCache[repo.name] = packages;
                    break; // Successfully parsed from this server
                }
            } else {
                if (!silent) {
                    std::cerr << RED << "[!] Failed to extract database from " << serverUrl << RESET << "\n";
                }
            }
        } else {
            if (!silent) {
                std::cerr << RED << "[!] Failed to download database from " << serverUrl << RESET << "\n";
            }
        }
    }
    return packages;
}



// Check if package exists in repository
static bool packageExistsInRepo(const std::string& pkgName, const Repository& repo) {
    auto packages = downloadRepoDatabase(repo, true); // Silent during existence check
    return packages.find(pkgName) != packages.end();
}

// Handle choice between curated and AUR when both sources exist
static int handleMultiSourceChoice(const std::string& spec, const fs::path& WORK, const fs::path& pkgdir) {
    std::string aurDir = (WORK / spec).string();
    std::string curatedDir = pkgdir.string();
    
    bool aurBuilt = isPackageBuiltInDir(aurDir);
    bool curatedBuilt = isPackageBuiltInDir(curatedDir);
    
    std::cout << YELLOW << "[?] '" << spec << "' available in multiple sources" << RESET << "\n";
    
    std::string aurOption = "[?] Try AUR?(AUR is unstable sometimes)";
    std::string curatedOption = "[?] Choose curated repos instead (Recommended)";
    
    if (aurBuilt && curatedBuilt) {
        std::cout << YELLOW << "[*] Both AUR and curated versions are already built locally" << RESET << "\n";
    } else if (aurBuilt) {
        aurOption += " (already built locally)";
    } else if (curatedBuilt) {
        curatedOption += " (already built locally)";
    }
    
    std::cout << YELLOW << aurOption << " : y" << RESET << "\n";
    std::cout << YELLOW << curatedOption << " : n" << RESET << "\n";
    std::cout << YELLOW << "\nYour choice: [Y/n] " << RESET << std::flush;
    
    std::string resp;
    std::getline(std::cin, resp);
    
    if (!resp.empty()) {
        char c = std::tolower(static_cast<unsigned char>(resp[0]));
        if (c == 'n') return 2; // Curated
    }
    return 1; // AUR (default)
}

// Download package from repository - returns: 0=failure, 1=success, 2=user_declined
static int downloadFromRepo(const std::string& pkgName, const Repository& repo, const fs::path& workDir, const Config& config) {
    auto packages = downloadRepoDatabase(repo);
    auto it = packages.find(pkgName);
    if (it == packages.end()) {
        return 0;
    }
    
    const PackageInfo& pkg = it->second;
    std::string arch = getSystemArch();
    std::string pkgFile = (workDir / pkg.filename).string();
    
    // Check if package already exists and is valid
    if (fs::exists(pkgFile) && fs::file_size(pkgFile) > 1000) {
        std::string testCmd = "file \"" + pkgFile + "\" | grep -q 'Zstandard\\|gzip\\|XZ'";
        if (std::system(testCmd.c_str()) == 0) {
            std::cout << GREEN << ":: Package cache hit, using existing file" << RESET << "\n";
            // Install using pacman
            std::string installCmd = "sudo pacman -U \"" + pkgFile + "\"";
            int result = std::system(installCmd.c_str());
            if (result == 0) {
                return 1; // Success
            } else if (WEXITSTATUS(result) == 1) {
                std::cout << YELLOW << "[*] Installation declined by user" << RESET << "\n";
                return 2; // User declined
            } else {
                std::cerr << RED << "[!] Installation failed" << RESET << "\n";
                return 0; // Failure
            }
        } else {
            // Remove corrupted file
            fs::remove(pkgFile);
        }
    }
    
    // Get servers from Include path or use configured servers
    std::vector<std::string> servers = repo.servers;
    if (!repo.includePath.empty()) {
        // Expand $USER in path
        std::string expandedPath = repo.includePath;
        if (expandedPath.find("$USER") != std::string::npos) {
            std::string user = std::getenv("USER") ? std::getenv("USER") : "";
            size_t pos = 0;
            while ((pos = expandedPath.find("$USER", pos)) != std::string::npos) {
                expandedPath.replace(pos, 5, user);
                pos += user.length();
            }
        }
        
        if (fs::exists(expandedPath)) {
            auto mirrorServers = parseMirrorlist(expandedPath);
            if (!mirrorServers.empty()) {
                servers = selectBestMirrors(mirrorServers, repo.name);
            }
        }
    }
    
    std::cout << GREEN << ":: Retrieving packages..." << RESET << "\n";
    
    for (const auto& serverUrl : servers) {
        std::string url = replaceRepoVars(serverUrl, repo.name, arch);
        std::string pkgUrl = url + "/" + pkg.filename;
        
        if (downloadWithProgress(pkgUrl, pkgFile, config, pkg.filename)) {
            // Verify file was downloaded and is valid
            if (fs::exists(pkgFile) && fs::file_size(pkgFile) > 1000) {
                // Test if it's a valid archive
                std::string testCmd = "file \"" + pkgFile + "\" | grep -q 'Zstandard\\|gzip\\|XZ'";
                if (std::system(testCmd.c_str()) == 0) {
                    // Install using pacman
                    std::string installCmd = "sudo pacman -U \"" + pkgFile + "\"";
                    
                    int result = std::system(installCmd.c_str());
                    if (result == 0) {
                        return 1; // Success
                    } else if (WEXITSTATUS(result) == 1) {
                        std::cout << YELLOW << "[*] Installation declined by user" << RESET << "\n";
                        return 2; // User declined
                    } else {
                        std::cerr << RED << "[!] Installation failed" << RESET << "\n";
                        return 0; // Failure
                    }
                } else {
                    std::cout << RED << "corrupted" << RESET << "\n";
                    fs::remove(pkgFile);
                }
            } else {
                std::cout << RED << "failed" << RESET << "\n";
            }
        } else {
            // Download failed, already handled by downloadWithProgress
        }
    }
    return 0;
}
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
                if (key == pkgName && (val == "AUR" || val == "Curated" || val == "chaotic-aur" || !val.empty())) {
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
    } else if (source == "chaotic-aur") {
        return "Chaotic-AUR repositories";
    } else if (!source.empty() && source != "Unknown") {
        return source + " repositories";
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
    Config config = readConfig();

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

    // 3. Monorepo Logic (Curated) - Check first
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
        
        // Check if package also exists in AUR for multi-source handling
        if (config.askBeforeAUR && packageExistsInAUR(spec)) {
            int choice = handleMultiSourceChoice(spec, WORK, pkgdir);
            
            if (choice == 1) {
                // User chose AUR
                std::string aurUrl = std::string(AUR_NS) + spec + ".git";
                int success = cloneAndBuild(aurUrl, (WORK / spec).string());
                fs::current_path(originalPath);
                return success;
            }
            // choice == 2 means curated, continue below
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

    // 5. If AUR fails, try configured repositories as last resort
    if (success == 0) {
        for (const auto& [repoName, repo] : config.repositories) {
            if (packageExistsInRepo(spec, repo)) {
                std::cout << GREEN << "[*] Found " << spec << " in " << repoName << " repository." << RESET << "\n";
                if (downloadFromRepo(spec, repo, WORK, config)) {
                    recordPackageSource(spec, repoName);
                    std::cout << GREEN << "[✓] Installed " << spec << " from " << repoName << " repository." << RESET << "\n";
                    fs::current_path(originalPath);
                    return 1; // Success
                }
            }
        }
    }

    fs::current_path(originalPath);
    return success;
}

// Repository-only installation (for -Sr flag)
int installPkgFromRepo(const std::string &spec) {
    static const fs::path WORK = getWorkDir();
    const fs::path originalPath = fs::current_path();
    Config config = readConfig();

    // Check if package is already installed
    std::string source = getPackageSource(spec);
    if (!source.empty()) {
        std::cout << GREEN << "[✓] Package '" << spec << "' is already installed from " << source << RESET << "\n";
        return 3; // Already installed
    }

    // Only check configured repositories
    for (const auto& [repoName, repo] : config.repositories) {
        if (packageExistsInRepo(spec, repo)) {
            std::cout << GREEN << "[*] Found " << spec << " in " << repoName << " repository." << RESET << "\n";
            int result = downloadFromRepo(spec, repo, WORK, config);
            fs::current_path(originalPath);
            
            if (result == 1) {
                recordPackageSource(spec, repoName);
                std::cout << GREEN << "[✓] Installed " << spec << " from " << repoName << " repository." << RESET << "\n";
                return 1; // Success
            } else if (result == 2) {
                return 2; // User declined
            }
            // If result == 0, continue to next repo or fail
        }
    }

    std::cerr << RED << "[!] Package '" << spec << "' not found in any configured repository" << RESET << "\n";
    fs::current_path(originalPath);
    return 3; // Not found (not a failure)
}
