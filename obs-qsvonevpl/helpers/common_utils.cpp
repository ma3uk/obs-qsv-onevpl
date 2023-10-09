#include "common_utils.hpp"
#include "../obs-qsv-onevpl-encoder.hpp"
#include <cstdint>
#include <string>

// =================================================================
// Utility functions, not directly tied to Intel Media SDK functionality
//

struct adapter_info adapters[MAX_ADAPTERS] = {};
size_t adapter_count = 0;
