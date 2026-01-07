# 🐾 Tolito Package Manager

## 🚧 Status

Tolito is in **active development**. Core features are functional but expect ongoing improvements and refinements.

---

## 🚀 Overview

Tolito is a fast, lightweight package manager for Arch Linux written in pure C++ (C++17). It provides intelligent multi-source package management with priority-based updates, combining curated repositories, AUR, and Chaotic-AUR into a unified system.

### Key Philosophy

- **Curated-First**: Prioritizes Viper's curated PKGBUILDs (viper-pkgbuilds) for stability
- **AUR Integration**: Seamless fallback to AUR when packages aren't in curated repos
- **Repository Support**: Full pacman-compatible repository system (Chaotic-AUR, etc.)
- **Configuration-Driven**: Everything controlled via `tolito.conf` with no hardcoded values

---

## ✨ Features

### Multi-Source Package Management
- 📦 **Curated Repositories**: viper-pkgbuilds (GitHub)
- 🔧 **AUR Integration**: Full Arch User Repository support
- 🗄️ **Repository System**: Pacman-compatible repos (Chaotic-AUR)
- 🔗 **Direct URLs**: Install from any git repository URL

### Intelligent Update System
- 🔄 **Priority-Based Updates**: main → alternative → fallback logic
- 📊 **Cross-Source Comparison**: Compares versions across all sources
- 🎯 **Smart Source Switching**: Automatic or user-confirmed source changes
- ⚡ **Version Comparison**: Uses `vercmp` for accurate version checking

### Advanced Features
- 🔐 **PGP Key Handling**: Automatic key fetching and signing
- 💾 **Build Caching**: Skips rebuilding already-built packages
- 📝 **Source Tracking**: JSON-based tracking of package origins
- 🎨 **Progress Bars**: Pacman-style download progress with ILoveCandy support
- 🌈 **Color Support**: Configurable ANSI color output
- 🪞 **Mirror Selection**: Automatic speed testing and mirror ranking

---

## 🛠️ Commands

| Command | Description |
|---------|-------------|
| `tolito -S <pkg>` | Install package(s) from any source |
| `tolito -Sr <pkg>` | Install from repositories only |
| `tolito -Syu` | Update all installed packages |
| `tolito -Su <pkg>` | Update specific package |
| `tolito -R <pkg>` | Remove package(s) |
| `tolito -Q <pkg>` | Show package name and version |
| `tolito -Qi <pkg>` | Show detailed package information |
| `tolito clean` | Clear build cache |

---

## 📋 Installation Priority

When installing with `-S`, Tolito follows this sequence:

1. **Curated Repos** (viper-pkgbuilds) - Checked first
2. **AUR** - If not in curated (with user confirmation)
3. **Repositories** (Chaotic-AUR) - Last resort fallback

With `-Sr`, only repositories are checked.

---

## ⚙️ Configuration

Configuration file: `~/.config/tolito/tolito.conf`

### Example Configuration

```ini
# Global settings
ask_before_fallback_into_aur = 1
warn_about_aur_only = 1
askBeforeSwitchSources = false

[Misc]
ILoveCandy = true
Color = true
DisableDownloadTimeout = false

[UpdateRules]
_CURATED_:
getFromAUR=true
getFromChaotic=true
main=CURATED
alternative=AUR
fallback=CHAOTIC

_AUR_:
getFromCurated=true
getFromChaotic=true
main=AUR
alternative=CHAOTIC
fallback=CURATED

[repositories]
chaotic-aur:
Include=/home/$USER/.config/tolito/tolito.d/chaotic-mirrorlist
SigLevel=Optional
```

### Configuration Options

**Global Settings:**
- `ask_before_fallback_into_aur`: Prompt before using AUR
- `warn_about_aur_only`: Show warnings for AUR packages
- `askBeforeSwitchSources`: Confirm before switching sources

**Misc Options:**
- `ILoveCandy`: Enable Pac-Man style progress bar
- `Color`: Enable colored output
- `DisableDownloadTimeout`: Remove 120s download timeout

**UpdateRules:**
- `main`: Primary update source
- `alternative`: Secondary source
- `fallback`: Last resort source
- `getFrom*`: Enable checking specific sources

**Repositories:**
- `Include`: Path to mirrorlist file
- `SigLevel`: Signature verification level

---

## 💻 System Requirements

**OS:** Arch Linux or Arch-based distributions

**Build Dependencies:**
- GCC/Clang with C++17 support
- make
- libcurl development files

**Runtime Dependencies:**
- git
- makepkg (pacman)
- curl
- tar
- pacman

---

## 📥 Building from Source

```bash
git clone https://github.com/Xray-OS/tolito.git
cd tolito
make

# Run tolito
./tolito -S <package>
```

---

## 📂 File Structure

```
~/.config/tolito/
├── tolito.conf              # Main configuration
├── tolito.d/
│   └── chaotic-mirrorlist   # Repository mirrors
└── package_sources.json     # Source tracking

~/.cache/tolito/
└── repos/                   # Repository database cache

~/tolito/                    # Working directory
├── viper-pkgbuilds/         # Curated repository
└── <package-dirs>/          # AUR package builds
```

---

## 🎨 Progress Bar Features

- **Pacman-compatible**: Matches pacman's progress bar style
- **ILoveCandy Mode**: Pac-Man animation eating dots
- **Color Support**: Cyan package names, white values, colored progress
- **Smart Throttling**: Updates every 200ms or 1% progress change
- **Terminal Aware**: Adapts to terminal width

---

## 🔄 Update System

Tolito's update system uses priority-based rules:

1. Checks installed package source
2. Applies UpdateRules for that source type
3. Checks main → alternative → fallback sources
4. Compares versions using `vercmp`
5. Offers update if newer version found

---

## 🌐 Contact

For questions, issues, or contributions:
- GitHub: [Xray-OS/tolito](https://github.com/Xray-OS/tolito)
- Open an issue or pull request

---

**Built with ❤️ for Viper(Xray_OS) and the Arch Linux community**
