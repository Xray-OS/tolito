#ifndef TOLITO_UPDATE_H
#define TOLITO_UPDATE_H

#include <string>
#include <vector>

// Update a single package or all packages
int updatePkg(const std::string& spec = "");

// Check for updates across all sources
std::vector<std::string> checkUpdates();

#endif