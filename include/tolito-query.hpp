#ifndef TOLITO_QUERY_HPP
#define TOLITO_QUERY_HPP

#include <string>

// Checks if a package is installed (pacman -Q)
// pkg: name of the package to query.
// Prints version or "package not found" message.
void queryPkg(const std::string& pkg);

// Shows detailed info about an installed package (pacman -Qi).
// pkg: name of the package.
// Prints full info or an error message.
void showInfo(const std::string& pkg);

#endif // TOLITO_QUERY_HPP