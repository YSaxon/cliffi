#include "export_symbols.h"

#include <stdio.h>

#if defined(CLIFFI_HAS_LIEF)
#include <LIEF/LIEF.hpp>
#include <memory>

bool list_exported_symbols_with_lief(const char* library_path) {
    std::unique_ptr<LIEF::Binary> binary = LIEF::Parser::parse(library_path);
    if (!binary) {
        fprintf(stderr, "Error: Failed to parse '%s' using LIEF\n", library_path);
        return false;
    }

    size_t exported_count = 0;
    for (const LIEF::Symbol& symbol : binary->symbols()) {
        if (!symbol.is_exported()) {
            continue;
        }
        printf("%s\n", symbol.name().c_str());
        exported_count++;
    }

    printf("Total exported symbols: %zu\n", exported_count);
    return true;
}

#else

bool list_exported_symbols_with_lief(const char* library_path) {
    (void)library_path;
    fprintf(stderr,
            "Error: This build does not include LIEF support. Rebuild with a LIEF dependency to use 'exports'.\n");
    return false;
}

#endif
