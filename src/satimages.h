#pragma once

#include "util.h"

#include <vector>
#include <filesystem>
#include <execution>
#include <tuple>

#include <rust-lib/lib.h>

namespace fs = std::filesystem;

struct TileTransform {
    std::vector<float_t> aside = {};
    std::vector<float_t> pos = {};

};

struct CmpTileTransform
{
    bool operator()(const TileTransform& lhs, const TileTransform& rhs) const
    {
        return (lhs.aside < rhs.aside) || (rhs.pos < lhs.pos);
    }
};

void writeSatImages(arma_file_formats::cxx::OprwCxx& wrp, const int32_t& worldSize, std::filesystem::path& basePathSat, const std::string& worldName);
TileTransform getTileTransform(rust::Box<arma_file_formats::cxx::CfgCxx>& rap);
