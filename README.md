# Tolito AUR Helper

## Overview

**Note: Tolito AUR Helper is still in alpha state, be patient**

Tolito is an effective and useful AUR Helper that primarily pulls PKGBUILDS from Xray_OS official AUR repos, so yeah, Xray_OS has its own kind of AUR thing, but is not like the public AUR, it is private and the PKGBUILDS are curated to be installed on the fly without errors. Tolito basically uses the same commands that a typical AUR helper like Yay often uses, eg.: 'tolito -S palemoon-bin', when Tolito can't find any PKGBUILD that the user is looking for the curated XRAY-REPOS, it falls back into looking for it in the AUR, but before that Tolito ask the user first if they want to fallback and get that package from the AUR or not.

## Unique Features
- Downloads PKGBUILDS primarily from Xray-Official GitLab repos
- Fallbacks into the AUR if PKGBUILD is not found on the core xray-repos
- Written in pure C++

## Commands to Use:
- **install:** "tolito -S (name of pkg)"
- **Remove:** "tolito -R (name of pkg)"
- **Clean cache:** "tolito clean"
- **See version of a pkg:** "tolito -Q" / "tolito -Qi"
- **Update pkg/pkgs:** not supported yet.. working on it

## Fun facts about Tolito
- 🎨 Tolito is the name of my Kitty Cat
- 🛠️ Tolito is written in C++ which is my favorite language
- 🖥️ Tolito has the same Syntax from the conventional pacman manager, the only difference is when cleaning the cache 'tolito clean'

- **Project Page**: [SourceForge](https://sourceforge.net/projects/tolito/)
- **Status**: Xray_OS is back and being updated
- **Next Release**: Will include optional Xray_OS branding via Tolitica
