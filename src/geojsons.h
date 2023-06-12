#pragma once

#include <intercept.hpp>

#include "util.h"


#include <string>
#include <filesystem>
#include <vector>
#include <math.h>

#include <rust/cxx.h>
#include <rust-lib/lib.h>

#include <gdal.h>
#include <gdal_utils.h>
#include <gdal_priv.h>

#include <ogr_geometry.h>
#include <nlohmann/json.hpp>

#include "SimplePoint.h"

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#include <KnownFolders.h>
#include <Shlobj.h>
#include <codecvt>

// https://devblogs.microsoft.com/oldnewthing/20041025-00/?p=37483
EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#define HINST_THISCOMPONENT ((HINSTANCE)&__ImageBase)
#else
#error Only Win is supported
#endif // _WIN32

using namespace intercept;

namespace fs = std::filesystem;
namespace nl = nlohmann;

void writeLocations(const std::string& worldName, std::filesystem::path& basePathGeojson);
void writeHouses(rvff::cxx::OprwCxx& wrp, std::filesystem::path& basePathGeojson);
void writeObjects(rvff::cxx::OprwCxx& wrp, std::filesystem::path& basePathGeojson);
void writeRoads(rvff::cxx::OprwCxx& wrp, const std::string& worldName, std::filesystem::path& basePathGeojson,
    const std::map<std::string, std::vector<std::pair<rvff::cxx::ObjectCxx, rvff::cxx::LodCxx&>>>& mapObjects);
void writeSpecialIcons(rvff::cxx::OprwCxx& wrp, fs::path& basePathGeojson, uint32_t id, const std::string& name);
float_t calculateDistance(types::auto_array<types::game_value> start, SimpleVector vector, SimpleVector point);
float_t calculateMaxDistance(types::auto_array<types::game_value> ilsPos, types::auto_array<types::game_value> ilsDirection,
    types::auto_array<types::game_value> taxiIn, types::auto_array<types::game_value> taxiOff);
nl::json buildRunwayPolygon(sqf::config_entry& runwayConfig);
void writeRunways(fs::path& basePathGeojson, const std::string& worldName);
void writeGenericMapTypes(fs::path& basePathGeojson, const std::vector<std::pair<rvff::cxx::ObjectCxx, rvff::cxx::LodCxx&>>& objectPairs, const std::string& name);
void writePowerlines(rvff::cxx::OprwCxx& wrp, fs::path& basePathGeojson);
void writeRailways(fs::path& basePathGeojson, const std::vector<std::pair<rvff::cxx::ObjectCxx, rvff::cxx::LodCxx&>>& objectPairs);
void writeGeojsons(rvff::cxx::OprwCxx& wrp, std::filesystem::path& basePathGeojson, const std::string& worldName);
void writeRiver(rvff::cxx::OprwCxx& wrp, std::filesystem::path& basePathGeojson);
