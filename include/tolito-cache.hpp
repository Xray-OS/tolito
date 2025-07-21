#ifndef TOLITO_CACHE_HPP
#define TOLITO_CACHE_HPP

// Clears pacman's package cache (all uninstalled packages).
// Prompts and runs "pacman -Scc" under the hood.
void clearCache();

#endif // TOLITO_CACHE_HPP
