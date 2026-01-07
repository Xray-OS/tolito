#ifndef TOLITO_INSTALL_H
#define TOLITO_INSTALL_H

#include <string>

// Clones, builds and installs a PKGBUILD identified by 'spec'
int installPkg(const std::string& spec);

// Install package from repository only (for -Sr flag)
int installPkgFromRepo(const std::string& spec);

// Remove package from source tracking
void removePackageSource(const std::string& pkgName);

#endif
