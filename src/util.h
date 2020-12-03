#pragma once

#include <intercept.hpp>

#include "findPbos.h"

// String
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string.hpp>

// Gzip
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>

#include <vector>
#include <filesystem>
#include <nlohmann/json.hpp>

#include <grad_aff/pbo/pbo.h>

#ifdef _WIN32
    #define NOMINMAX
    #include <Windows.h>
    #include <codecvt>
#endif

#define GRAD_MEH_FORMAT_VERSION 0.1
#define GRAD_MEH_MAX_COLOR_DIF 441.6729559300637f
#define GRAD_MEH_OVERLAP_THRESHOLD 0.95f
#define GRAD_MEH_ROAD_WITH_FACTOR (M_PI / 10)

using namespace intercept;

namespace fs = std::filesystem;
namespace nl = nlohmann;

namespace ba = boost::algorithm;
namespace bi = boost::iostreams;

typedef std::array<float_t, 2> SimpleVector;

std::vector<fs::path> getFullPboPaths();
void populateMap();
fs::path findPboPath(std::string path);
void reportStatus(std::string worldName, std::string method, std::string report);
void writeGZJson(const std::string& fileName, fs::path path, nl::json& json);

bool isMapPopulating();

void prettyDiagLog(std::string message);
