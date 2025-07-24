# Tolito

Tolito is a modular, user-centric AUR helper mainly for Xray_OS and also for general Arch-based systems. It prioritizes curated GitLab PKGBUILDs and seamlessly falls back to the AUR when a package isn’t found—combining trust with comprehensive coverage.

---

## Features

- **Curated Xray_OS GitLab repositories First**  
  Tolito searches trusted Xray_OS GitLab repositories for PKGBUILDs before anything else.

- **Automatic AUR Fallback**  
  If a package isn’t found in Xray_OS curated repositories, Tolito gracefully retrieves it from the AUR.

- **Interactive Prompts**  
  Confirm each key step in the installation flow, ensuring clarity and control.

- **PKGBUILD Compliance**  
  Fully compatible with Arch packaging standards and best practices.

- **Lightweight & Modular**  
  Minimal dependencies and a pluggable design let you extend functionality as you see fit.

---

## Installation

1. Clone the Tolito repository:
   ```bash
   git clone https://gitlab.com/your-username/tolito.git
   cd tolito
   sudo cp -r tolito /usr/bin

   EXAMPLE USECASE:
   ## installing packages:
   tolito -S mullvad-browser-bin

   ## seeing versions of packages:
   tolito -Q mullvad-browser-bin
   tp;otp -Qi mullvad-browser-bin  (this is for a more detailed version of course)

   ## removing packages and clean cache:
   tolito -R mullvad-browser-bin
   tolito clean

   - the cache is clean from /home/user/tolito

   ## Installation

Other interesting features 
   ```
   W.I.P



   
