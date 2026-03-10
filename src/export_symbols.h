#ifndef EXPORT_SYMBOLS_H
#define EXPORT_SYMBOLS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool list_exported_symbols_with_lief(const char* library_path);

#ifdef __cplusplus
}
#endif

#endif
