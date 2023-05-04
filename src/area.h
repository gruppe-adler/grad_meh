#pragma once

#include "util.h"

#include <filesystem>
#include <vector>
#include <string>

#include <rust-lib/lib.h>

namespace fs = std::filesystem;

void writeArea(rvff::cxx::OprwCxx& wrp, fs::path& basePathGeojson, const std::vector<std::pair<rvff::cxx::ObjectCxx, rvff::cxx::LodCxx&>>& objectPairs,
    uint32_t epsilon, uint32_t minClusterSize, uint32_t buffer, uint32_t simplify, const std::string& name);
