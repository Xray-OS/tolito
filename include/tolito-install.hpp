#ifndef TOLITO_INSTALL_HPP
#define TOLITO_INSTALL_HPP

#include <string>

/// installPkg clones, builds, and installs a package identified by 'spec'.
///
/// 'spec' can be:
///  • a Git URL           – clones and installs directly from that repo
///  • a monorepo package  – sparse-checks out & installs from the curated GitLab repo
///  • an AUR package name – clones & builds from the AUR, auto-importing any missing PGP keys
///
/// On success, prints “[✓] Installed …”; on failure, prints detailed errors.

void installPkg(const std::string& spec);

#endif // TOLITO_INSTALL_HPP
