#ifndef TOLITO_REMOVE_HPP
#define TOLITO_REMOVE_HPP

#include <string>

// Uninstall a package via pacman -Rns.
// Pkg: name of the package to remove.
// Prints status messages on success or failure.
void removePkg(const std::string& pkg);

#endif // TOLITO_REMOVE_HPP
