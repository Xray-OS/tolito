#pragma once
#include <string>

/// importPgpKey retrieves a missing PGP key from the Ubuntu keyserver
/// and locally signs it so that makepkg trusts packages signed by that key.
///
/// @param keyId  The hex identifier of the PGP key (e.g. "157432CF78A65729").
/// @return       true on successful import & local sign; false on any error.

bool importPgpKey(const std::string& keyId);
