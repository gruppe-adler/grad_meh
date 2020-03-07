#include <intercept.hpp>

// String
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string.hpp>

// Gzip
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>

// Range TODO: replace with C++20 Range
#include <boost/range/counting_range.hpp>

#include <sstream>
#include <algorithm>
#include <execution>
#include <array>

#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>

#include <nlohmann/json.hpp>

#include <gdal.h>
#include <gdal_utils.h>
#include <gdal_priv.h>

#include <ogr_geometry.h>

#include <grad_aff/wrp/wrp.h>
#include <grad_aff/paa/paa.h>
#include <grad_aff/pbo/pbo.h>
#include <grad_aff/rap/rap.h>
#include <grad_aff/p3d/odol.h>

#include "findPbos.h"
#include "SimplePoint.h"

#ifdef _WIN32
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

#include "../addons/main/status_codes.hpp"

#define GRAD_MEH_VERSION 0.1
#define GRAD_MEH_MAX_COLOR_DIF 441.6729559300637f
#define GRAD_MEH_OVERLAP_THRESHOLD 0.95f
#define GRAD_MEH_ROAD_WITH_FACTOR (M_PI / 10)

using namespace intercept;
using namespace OpenImageIO_v2_1;

namespace fs = std::filesystem;
namespace nl = nlohmann;
namespace ba = boost::algorithm;
namespace bi = boost::iostreams;
using iet = types::game_state::game_evaluator::evaluator_error_type;

using SQFPar = game_value_parameter;

static std::map<std::string, fs::path> entryPboMap = {};
static std::map<std::string, std::pair<ModelInfo, ODOLv4xLod>> p3dMap = {};
static std::vector<RoadPart> roadPartsList = {};

static bool gradMehIsRunning = false;
static bool gradMehMapIsPopulating = false;

int intercept::api_version() { //This is required for the plugin to work.
    return INTERCEPT_SDK_API_VERSION;
}

/*
    Returns full paths to every *.pbo from mod Paths + A3 Root
*/
std::vector<fs::path> getFullPboPaths() {
    std::vector<std::string> pboList;

#ifdef _WIN32
    for (auto& pboW : generate_pbo_list()) {
        pboList.push_back(std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t>().to_bytes(pboW).substr(4));
    }
#else
    pboList = generate_pbo_list();
#endif

    std::vector<fs::path> pboPaths;
    pboPaths.reserve(pboList.size());

    for (auto& pboPath : pboList) {
        auto path = (fs::path)pboPath;
        std::string ext(".pbo");
        if (path.has_extension() && boost::iequals(path.extension().string(), ext)) {
            pboPaths.push_back(path);
        }
    }

    return pboPaths;
}

/*
    Builds a map <pboPrefix, fullPboPath>
*/
void populateMap() {
    for (auto& p : getFullPboPaths()) {
        auto pbo = grad_aff::Pbo::Pbo(p.string());
        try {
            pbo.readPbo(false);
        }
        catch (std::exception &ex) {
            client::invoker_lock threadLock;
            std::stringstream errorStream;
            errorStream << "[grad_meh] Couldn't open PBO: " << p.string() << " error msg: " << ex.what();
            sqf::diag_log(errorStream.str());
            threadLock.unlock();
        }
        for (auto& entry : pbo.entries) {
            entryPboMap.insert({ pbo.productEntries["prefix"] , p });
        }
    }
    gradMehMapIsPopulating = false;
}

/*
    Finds pbo Path
*/
fs::path findPboPath(std::string path) {
    size_t matchLength = 0;
    fs::path retPath;

    if (boost::starts_with(path, "\\")) {
        path = path.substr(1);
    }

    for (auto const& [key, val] : entryPboMap)
    {
        if (boost::istarts_with(path, key) && key.length() > matchLength) {
            retPath = val;
        }
    }
    return retPath;
}

types::game_value updateCode;

void reportStatus(std::string worldName, std::string method, std::string report) {
    client::invoker_lock threadLock;
    if (updateCode.data == NULL) {
        updateCode = sqf::get_variable(sqf::ui_namespace(), "grad_meh_fnc_updateProgress");
    }
    sqf::call(updateCode, auto_array<game_value> {worldName, method, report});
}

void writeMeta(const std::string& worldName, const int32_t& worldSize, std::filesystem::path& basePath)
{
    client::invoker_lock threadLock;
    auto mapConfig = sqf::config_entry(sqf::config_file()) >> "CfgWorlds" >> worldName;
    nl::json meta;
    meta["version"] = GRAD_MEH_VERSION;
    meta["worldName"] = worldName;
    meta["worldSize"] = worldSize;
    meta["author"] = sqf::get_text(mapConfig >> "author");
    meta["displayName"] = sqf::get_text(mapConfig >> "description");
    meta["elevationOffset"] = sqf::get_number(mapConfig >> "elevationOffset");
    meta["latitude"] = sqf::get_number(mapConfig >> "latitude");
    meta["longitude"] = sqf::get_number(mapConfig >> "longitude");
    meta["gridOffsetX"] = sqf::get_number(mapConfig >> "Grid" >> "offsetX");
    meta["gridOffsetY"] = sqf::get_number(mapConfig >> "Grid" >> "offsetY");

    auto gridArray = nl::json::array();

    for (auto& grid : sqf::config_classes("true", (mapConfig >> "Grid"))) {
        auto entry = sqf::config_entry(grid);

        nl::json entryGrid;
        entryGrid["zoomMax"] = sqf::get_number(entry >> "zoomMax");
        entryGrid["format"] = sqf::get_text(entry >> "format");
        entryGrid["formatX"] = sqf::get_text(entry >> "formatX");
        entryGrid["formatY"] = sqf::get_text(entry >> "formatY");
        entryGrid["stepX"] = (int32_t)sqf::get_number(entry >> "stepX");
        entryGrid["stepY"] = (int32_t)sqf::get_number(entry >> "stepY");

        gridArray.push_back(entryGrid);
    }

    meta["grids"] = gridArray;
    threadLock.unlock();

    std::ofstream out(basePath / "meta.json");
    out << std::setw(4) << meta << std::endl;
    out.close();
}

void writeDem(std::filesystem::path& basePath, grad_aff::Wrp& wrp, const int32_t& worldSize)
{
    std::stringstream demStringStream;
    demStringStream << "ncols " << wrp.mapSizeX << std::endl;
    demStringStream << "nrows " << wrp.mapSizeY << std::endl;
    demStringStream << "xllcorner " << 0.0 << std::endl;
    demStringStream << "yllcorner " << 0.0 << std::endl;
    demStringStream << "cellsize " << (worldSize / wrp.mapSizeX) << std::endl; // worldSize / mapsizex
    demStringStream << "NODATA_value " << -9999;
    demStringStream << std::endl;

    for (int64_t y = wrp.mapSizeY - 1; y >= 0; y--) {
        for (size_t x = 0; x < wrp.mapSizeX; x++) {
            demStringStream << wrp.elevation[x + wrp.mapSizeX * y] << " ";
        }
        demStringStream << std::endl;
    }

    bi::filtering_istream fis;
    fis.push(bi::gzip_compressor(bi::gzip_params(bi::gzip::best_compression)));
    fis.push(demStringStream);

    std::ofstream demOut(basePath / "dem.asc.gz", std::ios::binary);
    bi::copy(fis, demOut);
    demOut.close();
}

void writePreviewImage(const std::string& worldName, std::filesystem::path& basePath)
{
    client::invoker_lock threadLock;
    auto mapConfig = sqf::config_entry(sqf::config_file()) >> "CfgWorlds" >> worldName;
    std::string picturePath = sqf::get_text(mapConfig >> "pictureMap");
    threadLock.unlock();

    if (boost::starts_with(picturePath, "\\")) {
        picturePath = picturePath.substr(1);
    }

    auto pboPath = findPboPath(picturePath);
    grad_aff::Pbo prewviewPbo(pboPath.string());

    auto paa = grad_aff::Paa(prewviewPbo.getEntryData(picturePath));
    paa.readPaa();
    paa.writeImage((basePath / "preview.png").string());
}

void writeLocations(const std::string& worldName, std::filesystem::path& basePathGeojson)
{
    client::invoker_lock threadLock;
    auto mapConfig = sqf::config_entry(sqf::config_file()) >> "CfgWorlds" >> worldName;

    auto basePathGeojsonLocations = basePathGeojson / "locations";
    if (!fs::exists(basePathGeojsonLocations)) {
        fs::create_directories(basePathGeojsonLocations);
    }

    std::map<std::string, std::shared_ptr<nl::json>> locationMap = {};
    auto locationArray = nl::json::array();
    for (auto& location : sqf::config_classes("true", (mapConfig >> "Names"))) {
        auto locationEntry = sqf::config_entry(location);
        auto type = sqf::get_text(locationEntry >> "type");

        auto kvp = locationMap.find(type);
        if (kvp == locationMap.end()) {
            kvp = locationMap.insert({ type, std::make_shared<nl::json>() }).first;
        }

        auto pointFeature = nl::json();
        pointFeature["type"] = "Feature";

        auto geometry = nl::json();
        geometry["type"] = "Point";

        auto posArray = nl::json::array();
        auto posArr = sqf::get_array(locationEntry >> "position");
        for (auto& pos : posArr.to_array()) {
            posArray.push_back((float_t)pos);
        }
        geometry["coordinates"] = posArray;

        pointFeature["geometry"] = geometry;

        auto properties = nl::json();
        properties["name"] = sqf::get_text(locationEntry >> "name");
        properties["radiusA"] = sqf::get_number(locationEntry >> "radiusA");
        properties["radiusB"] = sqf::get_number(locationEntry >> "radiusB");
        properties["angle"] = sqf::get_number(locationEntry >> "angle");

        pointFeature["properties"] = properties;

        kvp->second->push_back(pointFeature);
    }

    threadLock.unlock();
    for (auto& pair : locationMap) {
        std::stringstream houseInStream;
        houseInStream << std::setw(4) << *pair.second;

        bi::filtering_istream fis;
        fis.push(bi::gzip_compressor(bi::gzip_params(bi::gzip::best_compression)));
        fis.push(houseInStream);

        std::ofstream houseOut(basePathGeojsonLocations / (pair.first + std::string(".geojson.gz")), std::ios::binary);
        bi::copy(fis, houseOut);
        houseOut.close();
    }
}

void writeHouses(grad_aff::Wrp& wrp, std::filesystem::path& basePathGeojson)
{
    nl::json house = nl::json::array();

    for (auto& mapInfo : wrp.mapInfo) {
        if (mapInfo->mapType == 4) {
            auto mapInfo4Ptr = std::static_pointer_cast<MapType4>(mapInfo);
            nl::json mapFeature;
            mapFeature["type"] = "Feature";

            auto coordArr = nl::json::array();
            coordArr.push_back(std::vector<float_t> { mapInfo4Ptr->bounds[0], mapInfo4Ptr->bounds[1] });
            coordArr.push_back(std::vector<float_t> { mapInfo4Ptr->bounds[2], mapInfo4Ptr->bounds[3] });
            coordArr.push_back(std::vector<float_t> { mapInfo4Ptr->bounds[6], mapInfo4Ptr->bounds[7] });
            coordArr.push_back(std::vector<float_t> { mapInfo4Ptr->bounds[4], mapInfo4Ptr->bounds[5] });

            auto outerArr = nl::json::array();
            outerArr.push_back(coordArr);

            mapFeature["geometry"] = { { "type" , "Polygon" }, { "coordinates" , outerArr } };
            mapFeature["properties"] = { { "color", mapInfo4Ptr->color } };

            house.push_back(mapFeature);
        }
    }

    std::stringstream houseInStream;
    houseInStream << std::setw(4) << house;

    bi::filtering_istream fis;
    fis.push(bi::gzip_compressor(bi::gzip_params(bi::gzip::best_compression)));
    fis.push(houseInStream);

    std::ofstream houseOut(basePathGeojson / "house.geojson.gz", std::ios::binary);
    bi::copy(fis, houseOut);
    houseOut.close();
}

void writeRoads(const std::string& worldName, std::filesystem::path& basePathGeojson) {
    client::invoker_lock threadLock;
    auto roadsPath = sqf::get_text(sqf::config_entry(sqf::config_file()) >> "CfgWorlds" >> worldName >> "newRoadsShape");
    threadLock.unlock();

    auto basePathGeojsonTemp = basePathGeojson / "temp";
    if (!fs::exists(basePathGeojsonTemp)) {
        fs::create_directories(basePathGeojsonTemp);
    }

    auto basePathGeojsonRoads = basePathGeojson / "roads";
    if (!fs::exists(basePathGeojsonRoads)) {
        fs::create_directories(basePathGeojsonRoads);
    }

    auto roadsPbo = grad_aff::Pbo::Pbo(findPboPath(roadsPath).string());
    roadsPbo.readPbo(false);

    auto roadsPathDir = ((fs::path)roadsPath).remove_filename().string();
    if (boost::starts_with(roadsPathDir, "\\")) {
        roadsPathDir = roadsPathDir.substr(1);
    }

    for (auto& [key, val] : roadsPbo.entries) {
        if (boost::istarts_with((((fs::path)roadsPbo.productEntries["prefix"]) / key).string(), roadsPathDir)) {
            roadsPbo.extractSingleFile(key, basePathGeojsonTemp, false);
        }
    }

#ifdef _WIN32
    char filePath[MAX_PATH + 1];
    GetModuleFileName(HINST_THISCOMPONENT, filePath, MAX_PATH + 1);
#endif

    auto gdalPath = fs::path(filePath);
    gdalPath = gdalPath.remove_filename();

#ifdef _WIN32
    // remove win garbage
    gdalPath = gdalPath.string().substr(4);

    // get appdata path
    fs::path gdalLogPath;
    PWSTR localAppdataPath = NULL;
    if (SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &localAppdataPath) != S_OK) {
        gdalLogPath = "grad_meh_gdal.log";
    }
    else {
        gdalLogPath = (fs::path)localAppdataPath / "Arma 3" / "grad_meh_gdal.log";
    };
    CoTaskMemFree(localAppdataPath);

#endif

    CPLSetConfigOption("GDAL_DATA", gdalPath.string().c_str());
    CPLSetConfigOption("CPL_LOG", gdalLogPath.string().c_str());
    CPLSetConfigOption("CPL_LOG_ERRORS", "ON");

    const char* pszFormat = "GeoJSON";
    GDALDriver* poDriver;
    char** papszMetadata;
    GDALAllRegister();
    poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
    if (poDriver == NULL) {
        threadLock.lock();
        sqf::diag_log("Couldn't get the GeoJSON Driver");
        threadLock.unlock();
        return;
    }
    papszMetadata = poDriver->GetMetadata();

    GDALDatasetH poDatasetH = GDALOpenEx((basePathGeojsonTemp / "roads.shp").string().c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);

    GDALDataset* poDataset = (GDALDataset*)poDatasetH;
    GDALDataset* poDstDS;
    char** papszOptions = NULL;

    fs::remove(basePathGeojsonTemp / "roads.geojson");
    poDstDS = poDriver->Create((basePathGeojsonTemp / "roads.geojson").string().c_str(),
        poDataset->GetRasterXSize(), poDataset->GetRasterYSize(), poDataset->GetBands().size(), GDT_Byte, papszOptions);

    char* options[] = { "-f", "GeoJSON", nullptr };

    auto gdalOptions = GDALVectorTranslateOptionsNew(options, NULL);

    int error = 0;
    auto dataSet = GDALVectorTranslate((basePathGeojsonTemp / "roads.geojson").string().c_str(), poDstDS, 1, &poDatasetH, gdalOptions, &error);

    if (error != 0) {
        threadLock.lock();
        sqf::diag_log("GDALVectorTranslate failed!");
        threadLock.unlock();
        return;
    }

    GDALClose(poDatasetH);
    GDALClose(poDstDS);
    GDALVectorTranslateOptionsFree(gdalOptions);

    grad_aff::Rap roadConfig;
    roadConfig.parseConfig(basePathGeojsonTemp / "roadslib.cfg");

    // Parse RoadsLib.cfg and construct with:roadType Map
    std::map<uint32_t, std::pair<int32_t, std::string>> roadWidthMap = {};
    for (auto& rootEntry : roadConfig.classEntries) {
        if (rootEntry->type == 0 && boost::iequals(rootEntry->name, "RoadTypesLibrary")) {
            for (auto& roadEntry : std::static_pointer_cast<RapClass>(rootEntry)->classEntries) {
                if (roadEntry->type == 0 && boost::istarts_with(roadEntry->name, "Road")) {
                    std::pair<int32_t, std::string> roadPair = {};
                    for (auto roadValues : std::static_pointer_cast<RapClass>(roadEntry)->classEntries) {
                        if (roadValues->type == 1 && boost::iequals(roadValues->name, "width")) {
                            roadPair.first = std::get<int32_t>(std::static_pointer_cast<RapValue>(roadValues)->value);
                            continue;
                        }
                        if (roadValues->type == 1 && boost::iequals(roadValues->name, "map")) {
                            roadPair.second = boost::replace_all_copy(std::get<std::string>(std::static_pointer_cast<RapValue>(roadValues)->value), " ", "_");
                            continue;
                        }
                    }
                    roadWidthMap.insert({ std::stoi(roadEntry->name.substr(4)), roadPair });
                }
            }
        }
    }

    // calc additional roads
    auto additionalRoads = std::map<std::string, std::vector<std::array<SimplePoint, 4>>>{};
    for (auto& roadPart : roadPartsList) {
        auto p3dPath = roadPart.p3dModel;
        if (boost::starts_with(p3dPath, "\\")) {
            p3dPath = p3dPath.substr(1);
        }
        auto res = p3dMap.find(p3dPath);
        if (res != p3dMap.end()) {
            auto lod = res->second.second;
            auto mapToken = lod.tokens.find("map");
            if (mapToken != lod.tokens.end() && mapToken->second != "hide") {
                // http://nghiaho.com/?page_id=846
                // Values from roation matrix, needed for the z rotation
                auto r31 = roadPart.transformMatrix[2][0];
                auto r32 = roadPart.transformMatrix[2][1];
                auto r33 = roadPart.transformMatrix[2][2];

                // calculate rotation in degrees
                auto theta = std::atan2(r31 * (-1), std::sqrt(std::pow(r32,2) + std::pow(r33,2)));
                theta = theta * (180.0f / M_PI);

                // Corrected center pos
                auto centerCorX = roadPart.transformMatrix[3][0] - lod.bCeneter[0];
                auto centerCorY = roadPart.transformMatrix[3][2] - lod.bCeneter[2];
                /*
                auto rXmin = centerCorX + lod.bMin[0];
                auto rYmin = centerCorY + lod.bMin[2];

                auto rXmax = centerCorX + lod.bMax[0];
                auto rYmax = centerCorY + lod.bMax[2];
                */
                // calc rotated pos of corner 
                // https://gamedev.stackexchange.com/a/86780

                /*
                    (x_1,y_1) --- (x_2,y_2)
                        |             |
                        |             |
                    (x_0,y_0) --- (x_3,y_3)
                */


                auto x0 = centerCorX + (lod.bMin[0] * std::cos(theta)) - (lod.bMin[2] * std::sin(theta));
                auto y0 = centerCorY + (lod.bMax[0] * std::sin(theta)) + (lod.bMax[2] * std::cos(theta));

                auto x1 = centerCorX + (lod.bMax[0] * std::cos(theta)) - (lod.bMin[2] * std::sin(theta));
                auto y1 = centerCorY + (lod.bMax[0] * std::sin(theta)) + (lod.bMin[2] * std::cos(theta));

                auto x2 = centerCorX + (lod.bMax[0] * std::cos(theta)) - (lod.bMax[2] * std::sin(theta));
                auto y2 = centerCorY + (lod.bMin[0] * std::sin(theta)) + (lod.bMin[2] * std::cos(theta));

                auto x3 = centerCorX + (lod.bMin[0] * std::cos(theta)) - (lod.bMax[2] * std::sin(theta));
                auto y3 = centerCorY + (lod.bMin[0] * std::sin(theta)) + (lod.bMax[2] * std::cos(theta));

                /*
                auto rXmin = centerCorX + (lod.bMin[0] * std::cos(thetaZ)) - (lod.bMin[2] * std::sin(thetaZ));
                auto rYmin = centerCorY + (lod.bMin[0] * std::sin(thetaZ)) - (lod.bMin[2] * std::cos(thetaZ));

                auto rXmax = centerCorX + (lod.bMax[0] * std::cos(thetaZ)) - (lod.bMax[2] * std::sin(thetaZ));
                auto rYmax = centerCorY + (lod.bMax[0] * std::sin(thetaZ)) - (lod.bMax[2] * std::cos(thetaZ));
                */
                std::array<SimplePoint, 4> rectangle = { SimplePoint(x0,y0), SimplePoint(x1,y1), SimplePoint(x2,y2),SimplePoint(x3,y3) };

                auto addRoadsResult = additionalRoads.find(mapToken->second);
                if (addRoadsResult == additionalRoads.end()) {
                    additionalRoads.insert({ mapToken->second, {rectangle} });
                }
                else {
                    addRoadsResult->second.push_back(rectangle);
                }
            }
        }
    }
    
    std::ifstream inGeoJson(basePathGeojsonTemp / "roads.geojson");
    nl::json j;
    inGeoJson >> j;
    inGeoJson.close();
    
    std::map<std::string, nl::json> roadMap = {};
    for (auto& feature : j["features"]) {
        auto coordsUpdate = nl::json::array();
        for (auto& coords : feature["geometry"]["coordinates"]) {
            coords[0] = (float_t)coords[0] - 200000.0f;
            coordsUpdate.push_back(coords);
        }
        feature["geometry"]["coordinates"] = coordsUpdate;

        int32_t jId = feature["properties"]["ID"];
        feature["properties"].clear();

        auto ogrGeometry = OGRGeometryFactory::createFromGeoJson(feature["geometry"].dump().c_str());
        ogrGeometry = ogrGeometry->Buffer(roadWidthMap[jId].first * GRAD_MEH_ROAD_WITH_FACTOR);

        auto ret = ogrGeometry->exportToJson();
        feature["geometry"] = nl::json::parse(ret);
        CPLFree(ret);
        OGRGeometryFactory::destroyGeometry(ogrGeometry);

        auto kvp = roadMap.find(roadWidthMap[jId].second);
        if (kvp == roadMap.end()) {
            kvp = roadMap.insert({ roadWidthMap[jId].second, nl::json() }).first;
        }
        kvp->second.push_back(feature);
    }
    


    for (auto& [key, value] : roadMap) {

        // Append additional roads
        auto kvp = additionalRoads.find(key);
        if (kvp != additionalRoads.end()) {
            //for (auto& addtionalRoad : kvp->second) {
                for (auto& rectangles : kvp->second) {
                    nl::json addtionalRoadJson;

                    nl::json geometry;
                    geometry["type"] = "Polygon";
                    geometry["coordinates"].push_back({ nl::json::array() });
                    for (auto& point : rectangles) {
                        nl::json points = nl::json::array();
                        points.push_back(point.x);
                        points.push_back(point.y);
                        geometry["coordinates"][0].push_back(points);
                    }

                    addtionalRoadJson["geometry"] = geometry;
                    addtionalRoadJson["properties"] = nl::json::object();
                    addtionalRoadJson["type"] = "Feature";

                    value.push_back(addtionalRoadJson);
                }
           // }
        }

        std::stringstream roadInStream;
        roadInStream << std::setw(4) << value;

        bi::filtering_istream fis;
        fis.push(bi::gzip_compressor(bi::gzip_params(bi::gzip::best_compression)));
        fis.push(roadInStream);

        std::ofstream roadOut(basePathGeojsonRoads / (key + std::string(".geojson.gz")), std::ios::binary);
        bi::copy(fis, roadOut);
        roadOut.close();
    }

    fs::remove_all(basePathGeojsonTemp);
}

void writeGeojsons(grad_aff::Wrp& wrp, std::filesystem::path& basePathGeojson, const std::string& worldName)
{
    for (auto& roadNet : wrp.roadNets) {
        for (auto& roadPart : roadNet.roadParts) {
            roadPartsList.push_back(roadPart);
        }
    }

    for (auto& roadPart : roadPartsList) {

        auto p3dPath = roadPart.p3dModel;
        if (boost::starts_with(p3dPath, "\\")) {
            p3dPath = p3dPath.substr(1);
        }
        if (p3dMap.find(p3dPath) == p3dMap.end()) {
            auto pboPath = findPboPath(p3dPath);
            grad_aff::Pbo p3dPbo(pboPath.string());

            auto odol = grad_aff::Odol(p3dPbo.getEntryData(p3dPath));
            odol.peekLodTypes();

            auto geoIndex = -1;
            for (int i = 0; i < odol.lods.size(); i++) {
                if (odol.lods[i].lodType == LodType::GEOMETRY) {
                    geoIndex = i;
                }
            }
            if (geoIndex) {
                auto retLod = odol.readLod(geoIndex);
                p3dMap.insert({ p3dPath, {odol.modelInfo, retLod } });
            }
            else {
                p3dMap.insert({ p3dPath, {} });
            }
        }
    }

    writeHouses(wrp, basePathGeojson);
    writeLocations(worldName, basePathGeojson);
    writeRoads(worldName, basePathGeojson);
}

float_t mean(std::vector<float_t>& equalities)
{
    float_t mean = 0;
    std::for_each(equalities.begin(), equalities.end(), [&mean](float_t& val) {
        mean += val;
        });

    mean /= equalities.size();
    return mean;
}

float_t compareColors(uint8_t r1, uint8_t g1, uint8_t b1, uint8_t r2, uint8_t g2, uint8_t b2) {
    return (1 - (std::sqrt(std::pow(r2 - r1, 2) + std::pow(g2 - g1, 2) + std::pow(b2 - b1, 2)) / GRAD_MEH_MAX_COLOR_DIF));
}

float_t compareRows(std::vector<uint8_t>::iterator mipmap1It, std::vector<uint8_t>::iterator mipmap2It, uint32_t width) {
    std::vector<float_t> equalities = {};
    for (size_t i = 0; i < width; i++) {
        equalities.push_back(compareColors(*mipmap1It, *(mipmap1It + 1), *(mipmap1It + 2), *mipmap2It, *(mipmap2It + 1), *(mipmap2It + 2)));
        mipmap1It += 4;
        mipmap2It += 4;
    }

    return mean(equalities);
}

uint32_t calcOverLap(MipMap& mipmap1, MipMap& mipmap2) {
    std::vector<std::pair<float_t, size_t>> bestMatchingOverlaps = {};
    auto filter = mipmap1.height > 512 ? 6 : 3;

    for (uint32_t overlap = 1; overlap < (mipmap1.height / filter); overlap++) {
        std::vector<float_t> equalities = {};

        for (size_t i = 0; i < overlap; i++) {
            auto beginRowUpper = mipmap1.data.begin() + (mipmap2.height - 1 - (overlap - i)) * (size_t)mipmap1.width * 4;
            auto beginRowLower = mipmap2.data.begin() + i * (size_t)mipmap2.width * 4;

            equalities.push_back(compareRows(beginRowUpper,
                beginRowLower,
                (uint32_t)mipmap1.width));
        }

        auto meanVal = mean(equalities);
        if (meanVal > GRAD_MEH_OVERLAP_THRESHOLD) {
            bestMatchingOverlaps.push_back({ meanVal, overlap });
        }
    }

    uint32_t calculatedOverlap = 0;
    float_t bestOverlap = 0;

    for (auto& kvp : bestMatchingOverlaps) {
        if (kvp.first > bestOverlap) {
            bestOverlap = kvp.first;
            calculatedOverlap = kvp.second;
        }
    }

    return calculatedOverlap + 1;
}

void writeSatImages(grad_aff::Wrp& wrp, const int32_t& worldSize, std::filesystem::path& basePathSat, const std::string& worldName)
{
    std::vector<std::string> rvmats = {};
    for (auto& rv : wrp.rvmats) {
        if (rv != "") {
            rvmats.push_back(rv);
        }
    }

    std::sort(rvmats.begin(), rvmats.end());

    if (rvmats.size() > 1) {
        std::vector<std::string> lcoPaths = {};
        lcoPaths.reserve(rvmats.size());

        auto rvmatPbo = grad_aff::Pbo::Pbo(findPboPath(rvmats[0]).string());
        rvmatPbo.readPbo(false);

        // Has to be not empty
        std::string lastValidRvMat = "1337";
        std::string fillerTile = "";
        std::string prefix = "s_";

        for(auto& rvmatPath : rvmats) {
            if (!boost::istarts_with(((fs::path)rvmatPath).filename().string(), lastValidRvMat)) {
                auto rap = grad_aff::Rap::Rap(rvmatPbo.getEntryData(rvmatPath));
                rap.readRap();
                
                for (auto& entry : rap.classEntries) {
                    if (boost::iequals(entry->name, "Stage0")) {
                        auto rapClassPtr = std::static_pointer_cast<RapClass>(entry);

                        for (auto& subEntry : rapClassPtr->classEntries) {
                            if (boost::iequals(subEntry->name, "texture")) {
                                auto textureStr = std::get<std::string>(std::static_pointer_cast<RapValue>(subEntry)->value);
                                auto rvmatFilename = ((fs::path)textureStr).filename().string();
                                if (!boost::istarts_with(rvmatFilename, prefix)) {
                                    if (fillerTile == "") {
                                        fillerTile = textureStr;
                                    }
                                }
                                else {
                                    lcoPaths.push_back(textureStr);
                                    lastValidRvMat = rvmatFilename.substr(0, 9);
                                }
                                break;
                            }
                        }
                    }
                }
            }
        }

        // Remove Duplicates
        std::sort(lcoPaths.begin(), lcoPaths.end());
        auto last = std::unique(lcoPaths.begin(), lcoPaths.end());
        lcoPaths.erase(last, lcoPaths.end());

        // "strechted border" overlap is og overlap/2

        // find highest x/y
        std::vector<std::string> splitResult = {};
        boost::split(splitResult, ((fs::path)lcoPaths[lcoPaths.size()-1]).filename().string(), boost::is_any_of("_"));
        uint32_t maxX = std::stoi(splitResult[1]);
        uint32_t maxY = std::stoi(splitResult[2]);

        // find two tiles "in the middle"
        uint32_t x = maxX / 2;
        uint32_t y = maxY / 2;

        prefix = "00";

        if (y > 9 && y < 100) {
            prefix = "0";
        }
        else if (y >= 100) {
            prefix = "";
        }

        std::stringstream upperLcoPathStream; 
        upperLcoPathStream << x << "_" << prefix << y << "_lco.paa";
        auto upperPaaPath = upperLcoPathStream.str();

        std::stringstream lowerLcoPathStream;
        lowerLcoPathStream << x << "_" << prefix << (y + 1) << "_lco.paa";
        auto lowerPaaPath = lowerLcoPathStream.str();

        MipMap upperMipmap;
        MipMap lowerMipmap;

        for (auto& lcoPath : lcoPaths) {
            if (boost::iends_with(lcoPath, upperPaaPath)) {
                auto upperPbo = grad_aff::Pbo::Pbo(findPboPath(lcoPath).string());
                auto upperData = upperPbo.getEntryData(lcoPath);
                auto upperPaa = grad_aff::Paa::Paa(upperData);
                upperPaa.readPaa();
                upperMipmap = upperPaa.mipMaps[0];
            }
            if (boost::iends_with(lcoPath, lowerPaaPath)) {
                auto lowerPbo = grad_aff::Pbo::Pbo(findPboPath(lcoPath).string());
                auto lowerData = lowerPbo.getEntryData(lcoPath);
                auto lowerPaa = grad_aff::Paa::Paa(lowerData);
                lowerPaa.readPaa();
                lowerMipmap = lowerPaa.mipMaps[0];
            }
        }
        
        auto overlap = calcOverLap(upperMipmap, lowerMipmap);

        ImageBuf dst(ImageSpec(upperMipmap.width + maxX * (upperMipmap.width - overlap), upperMipmap.height + maxY * (upperMipmap.height - overlap), 4, TypeDesc::UINT8));

        if (fillerTile != "") {
            auto fillerPbo = grad_aff::Pbo::Pbo(findPboPath(fillerTile).string());
            auto fillerData = fillerPbo.getEntryData(fillerTile);
            auto fillerPaa = grad_aff::Paa::Paa(fillerData);
            fillerPaa.readPaa();

            ImageBuf src(ImageSpec(fillerPaa.mipMaps[0].height, fillerPaa.mipMaps[0].height, 4, TypeDesc::UINT8), fillerPaa.mipMaps[0].data.data());

            size_t noOfTiles = (worldSize / fillerPaa.mipMaps[0].height);

            auto cr = boost::counting_range(size_t(0), noOfTiles);
            std::for_each(std::execution::par_unseq, cr.begin(), cr.end(), [noOfTiles, &dst, fillerPaa, src] (size_t i) {
                for (size_t j = 0; j < noOfTiles; j++) {
                    ImageBufAlgo::paste(dst, (i * fillerPaa.mipMaps[0].height), (j * fillerPaa.mipMaps[0].height), 0, 0, src);
                }
            });
        }

        auto pbo = grad_aff::Pbo::Pbo(findPboPath(lcoPaths[0]).string());

        for (auto& lcoPath : lcoPaths) {
            auto data = pbo.getEntryData(lcoPath);

            if (data.empty()) {
                pbo = grad_aff::Pbo::Pbo(findPboPath(lcoPath).string());
                data = pbo.getEntryData(lcoPath);
            }

            auto paa = grad_aff::Paa::Paa(data);
            paa.readPaa();

            std::vector<std::string> splitResult = {};
            boost::split(splitResult, ((fs::path)lcoPath).filename().string(), boost::is_any_of("_"));

            ImageBuf src(ImageSpec(paa.mipMaps[0].height, paa.mipMaps[0].height, 4, TypeDesc::UINT8), paa.mipMaps[0].data.data());

            uint32_t x = std::stoi(splitResult[1]);
            uint32_t y = std::stoi(splitResult[2]);

            ImageBufAlgo::paste(dst, (x * paa.mipMaps[0].height - x * overlap), (y * paa.mipMaps[0].height - y * overlap), 0, 0, src);
        }

        // https://github.com/pennyworth12345/A3_MMSI/wiki/Mapframe-Information#stretching-at-edge-tiles
        maxX += 1;

        auto hasEqualStrechting = (worldSize == (upperMipmap.width * maxX)) || (worldSize == ((upperMipmap.width - overlap) * maxX));
        auto strechedBorderSizeStart = overlap / 2;
        auto strechedBorderSizeEnd = strechedBorderSizeStart;

        if (!hasEqualStrechting) {
            auto a = upperMipmap.width * maxX;
            auto b = (upperMipmap.width - (overlap / 2)) + ((upperMipmap.width - overlap) * (maxX - 1));
            strechedBorderSizeEnd = upperMipmap.width - (a - b);
        }

        dst = ImageBufAlgo::cut(dst, ROI(strechedBorderSizeStart, dst.spec().width - strechedBorderSizeEnd, strechedBorderSizeStart, dst.spec().height - strechedBorderSizeEnd));
        size_t tileSize = dst.spec().width / 4;

        auto cr = boost::counting_range(0, 4);
        std::for_each(std::execution::par_unseq, cr.begin(), cr.end(), [basePathSat, dst, tileSize] (int i) {
            auto curWritePath = basePathSat / std::to_string(i);
            if (!fs::exists(curWritePath)) {
                fs::create_directories(curWritePath);
            }

            for (int32_t j = 0; j < 4; j++) {
                ImageBuf out = ImageBufAlgo::cut(dst, ROI(i * tileSize, (i + 1) * tileSize, j * tileSize, (j + 1) * tileSize));
                out.write((curWritePath / std::to_string(j).append(".png")).string());
            }
        });
    }
}

void extractMap(const std::string& worldName, const std::string& worldPath, const int32_t& worldSize, std::array<bool, 5>& steps) {

    auto basePath = fs::path("grad_meh") / worldName;
    auto basePathGeojson = fs::path("grad_meh") / worldName / "geojson";
    auto basePathSat = fs::path("grad_meh") / worldName / "sat";

    if (!fs::exists(basePath)) {
        fs::create_directories(basePath);
    }
    if (!fs::exists(basePathGeojson)) {
        fs::create_directories(basePathGeojson);
    }
    if (!fs::exists(basePathSat)) {
        fs::create_directories(basePathSat);
    }

    std::string curWorldPath = "";

    // Find Wrp Path
    auto wrpPath = findPboPath(worldPath);
    grad_aff::Pbo wrpPbo(wrpPath.string());

    auto wrp = grad_aff::Wrp(wrpPbo.getEntryData(worldPath));
    wrp.wrpName = worldName + ".wrp";
    try {
        if (steps[0] || steps[1] || steps[4]) {
            reportStatus(worldName, "read_wrp", "running");
            wrp.readWrp();
            reportStatus(worldName, "read_wrp", "done");
        }
        else {
            reportStatus(worldName, "read_wrp", "canceled");
        }
    }
    catch (std::exception & ex) { // most likely caused by unknown mapinfo type
        client::invoker_lock threadLock;
        sqf::diag_log(ex.what());
        sqf::hint(ex.what());
        return;
    }

    if (steps[0]) {
        reportStatus(worldName, "write_sat", "running");
        writeSatImages(wrp, worldSize, basePathSat, worldName);
        reportStatus(worldName, "write_sat", "done");
    }
    if (steps[1]) {
        reportStatus(worldName, "write_houses", "running");
        writeGeojsons(wrp, basePathGeojson, worldName);
        reportStatus(worldName, "write_houses", "done");
    }
    
    if (steps[2]) {
        reportStatus(worldName, "write_preview", "running");
        writePreviewImage(worldName, basePath);
        reportStatus(worldName, "write_preview", "done");
    }
    
    if (steps[3]) {
        reportStatus(worldName, "write_meta", "running");
        writeMeta(worldName, worldSize, basePath);
        reportStatus(worldName, "write_meta", "done");
    }
    
    if (steps[4]) {
        reportStatus(worldName, "write_dem", "running");
        writeDem(basePath, wrp, worldSize);
        reportStatus(worldName, "write_dem", "done");
    }

    gradMehIsRunning = false;
    return;
}

game_value exportMapCommand(game_state& gs, SQFPar rightArg) {

    if (gradMehIsRunning)
        return GRAD_MEH_STATUS_ERR_ALREADY_RUNNING;

    if (gradMehMapIsPopulating)
        return GRAD_MEH_STATUS_ERR_PBO_POPULATING;

    std::string worldName;

    // [sat image, houses, preview img, meta.json, dem.asc]
    std::array<bool, 5> steps = { true, true, true, true, true };

    if (rightArg.type_enum() == game_data_type::STRING) {
        worldName = r_string(rightArg).c_str();
    }
    else if (rightArg.type_enum() == game_data_type::ARRAY) {
        auto parArray = rightArg.to_array();
        
        if (parArray.size() <= 0 || parArray.size() >= 7) {
            gs.set_script_error(iet::assertion_failed, "Wrong amount of arguments!"sv);
            return GRAD_MEH_STATUS_ERR_ARGS;
        }

        if (parArray[0].type_enum() == game_data_type::STRING) {

            worldName = r_string(parArray[0]);

            for (int i = 1; i < parArray.size(); i++) {
                if (parArray[i].type_enum() == game_data_type::BOOL) {
                    auto b = (bool)parArray[i];
                    steps[i - 1] = b;
                }
                else {
                    gs.set_script_error(iet::assertion_failed, types::r_string("Expected bool at index ").append(std::to_string(i)).append("!"));
                    return GRAD_MEH_STATUS_ERR_ARGS;
                }
            }
        }
        else {
            gs.set_script_error(iet::assertion_failed, "First element in the parameter array has to be a string!"sv);
            return GRAD_MEH_STATUS_ERR_ARGS;
        }

    }
    else {
        gs.set_script_error(iet::assertion_failed, "Expected a string or an array!"sv);
        return GRAD_MEH_STATUS_ERR_ARGS;
    }

    auto configWorld = sqf::config_entry(sqf::config_file()) >> "CfgWorlds" >> worldName;
    if (!boost::iequals(sqf::config_name(configWorld),  worldName)) {
        gs.set_script_error(iet::assertion_failed, "Couldn't find the specified world!"sv);
        return GRAD_MEH_STATUS_ERR_NOT_FOUND;
    }

    // Filters out maps like CAWorld
    auto worldSize = (int32_t)sqf::get_number(configWorld >> "mapSize");
    if (worldSize == 0) {
        gs.set_script_error(iet::assertion_failed, "Invalid world!"sv);
        return GRAD_MEH_STATUS_ERR_NO_WORLD_SIZE;
    }

    // check for leading /
    std::string worldPath = sqf::get_text(configWorld >> "worldName");
    if (boost::starts_with(worldPath, "\\")) {
        worldPath = worldPath.substr(1);
    }

    // try to find pbo
    auto wrpPboPath = findPboPath(worldPath);
    if (wrpPboPath == "") {
        return GRAD_MEH_STATUS_ERR_PBO_NOT_FOUND;
    }
    else {
        try {
            grad_aff::Pbo wrpPbo(wrpPboPath.string());
            if (!wrpPbo.hasEntry(worldPath))
                return GRAD_MEH_STATUS_ERR_PBO_NOT_FOUND;
        } catch (std::exception& ex) {
            sqf::diag_log(std::string("[grad_meh] Exception when opening PBO: ").append(ex.what()));
            return GRAD_MEH_STATUS_ERR_PBO_NOT_FOUND;
        }
    }

    if (!gradMehIsRunning) {
        gradMehIsRunning = true;
        std::thread readWrpThread(extractMap, worldName, worldPath, worldSize, steps);
        readWrpThread.detach();
        return GRAD_MEH_STATUS_OK;
    }
    else {
        sqf::diag_log("[grad_meh] gradMeh is already running! Aborting!");
        return GRAD_MEH_STATUS_ERR_ALREADY_RUNNING;
    }
}

void intercept::pre_start() {
    static auto grad_meh_export_map_string =
        client::host::register_sqf_command("gradMehExportMap", "Exports the given map", exportMapCommand, game_data_type::SCALAR, game_data_type::STRING);
    static auto grad_meh_export_map_array =
        client::host::register_sqf_command("gradMehExportMap", "Exports the given map", exportMapCommand, game_data_type::SCALAR, game_data_type::ARRAY);

    gradMehMapIsPopulating = true;
    std::thread mapPopulateThread(populateMap);
    mapPopulateThread.detach();
}

void intercept::pre_init() {
    intercept::sqf::system_chat("The grad_meh plugin is running!");
}
