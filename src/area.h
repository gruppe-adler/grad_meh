#pragma once

#include "util.h"

#include <filesystem>
#include <vector>
#include <string>

#include <grad_aff/p3d/odol.h>
#include <grad_aff/wrp/wrp.h>

namespace fs = std::filesystem;

void writeArea(grad_aff::Wrp& wrp, fs::path& basePathGeojson, const std::vector<std::pair<Object, ODOLv4xLod&>>& objectPairs,
    uint32_t epsilon, uint32_t minClusterSize, uint32_t buffer, uint32_t simplify, const std::string& name);
