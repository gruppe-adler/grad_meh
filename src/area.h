#pragma once

#include "util.h"

#include <filesystem>
#include <vector>
#include <string>

#include <rust-lib/lib.h>

namespace fs = std::filesystem;

void writeArea(arma_file_formats::cxx::OprwCxx& wrp, fs::path& basePathGeojson, const std::vector<std::pair<arma_file_formats::cxx::ObjectCxx, arma_file_formats::cxx::LodCxx&>>& objectPairs,
    uint32_t epsilon, uint32_t minClusterSize, uint32_t buffer, uint32_t simplify, const std::string& name);
