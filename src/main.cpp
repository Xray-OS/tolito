#include <iostream>
#include <cstdlib>
#include <string>

#include "tolito-install.hpp"
#include "tolito-remove.hpp"
#include "tolito-query.hpp"
#include "tolito-cache.hpp"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: tolito <option> [pkg-name]\n";
        return 1;
    }

    std::string option = argv[1];

    if (option == "-S" && argc == 3) installPkg(argv[2]);
    else if (option == "-R" && argc == 3) removePkg(argv[2]);
    else if (option == "-Qi" && argc == 3) showInfo(argv[2]);
    else if (option == "-Q" && argc == 3) queryPkg(argv[2]);
    else if (option == "clean") clearCache();
    else {
        std::cerr << "Invalid option or missing package name\n";
        return 2;
    }

    return 0;
}
