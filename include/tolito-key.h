#ifndef TOLITO_KEY_H
#define TOLITO_KEY_H

#include <string>

// ImportPkgKey retrieves a missing PGP key from the ubuntu keyserver
bool importPgpKey(const std::string& keyId);

// Fetch and trust a PGP key
bool fetchAndTrustgKey(const std::string& keyId);

#endif
