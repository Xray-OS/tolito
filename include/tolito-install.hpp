#ifndef TOLITO_INSTALL_HPP
#define TOLITO_INSTALL_HPP

#include <string>

// clones, builds, and installs a package from the AUR.
// pkg: name of the AUR package (e.g. "yay").
// On failure, prints errors; on success, prints a confirmation.
void installPkg(const std::string& pkg);

#endif // TOLITO_INSTALL_HPP
