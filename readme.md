# 🐾 Tolito AUR Helper

## 🚧 Note

Tolito AUR Helper is still in **alpha**. Be patient—bugs and missing features are expected.

---

## 🚀 Overview

Tolito is an effective and useful AUR Helper that primarily pulls PKGBUILDS from Xray_OS official AUR repos, so yeah, Xray_OS has its own kind of AUR thing, but is not like the public AUR, it is private and the PKGBUILDS are curated to be installed on the fly without errors. 

Tolito basically uses the same commands that a typical AUR helper like Yay often uses, eg.: 'tolito -S palemoon-bin', when Tolito can't find any PKGBUILD that the user is looking for the curated XRAY-REPOS, it falls back into the AUR, but before that Tolito ask the user first if they want to get that package from the AUR or not.

Tolito is fast, lightweight AUR helper written in pure C++ (C++17). It was built for [Xray_OS](https://example.com) and streamlines package installation by:

- Pulling curated PKGBUILDs from Xray_OS’s **private** GitLab repositories  
- Using the **same syntax** as popular helpers like `yay` (e.g. `tolito -S palemoon-bin`)  
- Falling back to the **public AUR** only if the package isn’t found in Xray repos—and only after prompting you first  

This “private-first” approach ensures on-the-fly installs with minimal errors, while still giving you the flexibility of the AUR when needed.

---

## ✨ Unique Features

- 📥 Primary source: **Xray_OS official GitLab repos**  
- 🔄 Optional fallback: **public AUR** (with user confirmation)  
- 🚀 Written in **pure C++** for speed and a small footprint  
- 🔧 Pacman- and `yay`-style commands for instant familiarity  

---

## 🛠️ Commands Reference

| Action        | Command                   | Notes                                    |
|---------------|---------------------------|------------------------------------------|
| Install       | `tolito -S <pkg>`         | Installs a package                       |
| Remove        | `tolito -R <pkg>`         | Removes a package                        |
| Clean cache   | `tolito clean`            | Clears Tolito’s build/cache directory    |
| Query version | `tolito -Q <pkg>`         | Show installed package version           |
| Query info    | `tolito -Qi <pkg>`        | Show detailed package info               |
| Update        | `tolito -Syu`              | Not supported yet (WIP)                  |

---

## 💻 System Requirements

- **OS:** Xray_OS (recommended) or any Arch-based distro  
- **Compiler:** GCC or Clang with C++17 support  
- **Dependencies:** `git`, `curl`, `make`, `pkgconf`

---

## 📥 How to build the binary

```bash
git clone https://github.com/Xray-OS/Tolito.git
cd tolito
cmake ..
make

## 📥 How to run it?

"./Tolito" or "sudo ./tolito"

```

🌐 Contact For any questions or contributions, feel free to open an issue or pull request on the GitHub repository.
