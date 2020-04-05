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
#include <fstream>
#include <iostream>
#include <list>
#include <vector>

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

// PCL
#include <pcl/point_types.h>
#include <pcl/io/pcd_io.h>
#include <pcl/filters/extract_indices.h>
#include <pcl/features/normal_3d.h>
#include <pcl/kdtree/kdtree.h>
#include <pcl/segmentation/extract_clusters.h>

#include <pcl/ModelCoefficients.h>
#include <pcl/point_types.h>
#include <pcl/io/pcd_io.h>
#include <pcl/filters/extract_indices.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/features/normal_3d.h>
#include <pcl/kdtree/kdtree.h>
#include <pcl/sample_consensus/method_types.h>
#include <pcl/sample_consensus/model_types.h>
#include <pcl/segmentation/sac_segmentation.h>
#include <pcl/segmentation/extract_clusters.h>
#include <pcl/segmentation/impl/extract_clusters.hpp>
#include <pcl/search/organized.h>
#include <pcl/search/impl/search.hpp>
#include <pcl/search/impl/organized.hpp>

#include <polyclipping/clipper.hpp>

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

typedef std::array<float_t, 2> SimpleVector;

static std::map<std::string, fs::path> entryPboMap = {};
static std::map<std::string, std::pair<ModelInfo, ODOLv4xLod>> p3dMap = {};

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

void writeGZJson(const std::string& fileName, fs::path path, nl::json& json) {
    std::stringstream strInStream;
    strInStream << std::setw(4) << json;

    bi::filtering_istream fis;
    fis.push(bi::gzip_compressor(bi::gzip_params(bi::gzip::best_compression)));
    fis.push(strInStream);

    std::ofstream out(path / fileName, std::ios::binary);
    bi::copy(fis, out);
    out.close();
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
    demStringStream << "cellsize " << ((float_t)worldSize / wrp.mapSizeX) << std::endl; // worldSize / mapsizex
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

        std::ofstream houseOut(basePathGeojsonLocations / boost::to_lower_copy((pair.first + std::string(".geojson.gz"))), std::ios::binary);
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
            mapFeature["properties"] = { { "color", { mapInfo4Ptr->color[2], mapInfo4Ptr->color[1], mapInfo4Ptr->color[0], mapInfo4Ptr->color[3] } } };

            house.push_back(mapFeature);
        }
    }
    writeGZJson("house.geojson.gz", basePathGeojson, house);
}

void writeObjects(grad_aff::Wrp& wrp, std::filesystem::path& basePathGeojson)
{
    nl::json house = nl::json::array();
    for (auto& mapInfo : wrp.mapInfo) {
        if (mapInfo->mapType == 4) {
            auto mapInfo4Ptr = std::static_pointer_cast<MapType4>(mapInfo);
            
            nl::json mapFeature;
            mapFeature["type"] = "Feature";

            auto posArray = nl::json::array();
            posArray.push_back(mapInfo4Ptr->bounds[0]);
            posArray.push_back(mapInfo4Ptr->bounds[1]);

            mapFeature["geometry"] = { { "type" , "Point" }, { "coordinates" , posArray } };
            mapFeature["properties"] = { { "text", std::to_string(mapInfo4Ptr->infoType)} };

            house.push_back(mapFeature);
        }
    }
    writeGZJson("debug.geojson.gz", basePathGeojson, house);
}

void writeRoads(grad_aff::Wrp& wrp, const std::string& worldName, std::filesystem::path& basePathGeojson, const std::map<std::string, std::vector<std::pair<Object, ODOLv4xLod&>>>& mapObjects) {

    std::vector<RoadPart> roadPartsList = {};
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
    std::map<uint32_t, std::pair<float_t, std::string>> roadWidthMap = {};
    for (auto& rootEntry : roadConfig.classEntries) {
        if (rootEntry->type == 0 && boost::iequals(rootEntry->name, "RoadTypesLibrary")) {
            for (auto& roadEntry : std::static_pointer_cast<RapClass>(rootEntry)->classEntries) {
                if (roadEntry->type == 0 && boost::istarts_with(roadEntry->name, "Road")) {
                    std::pair<float_t, std::string> roadPair = {};
                    for (auto roadValues : std::static_pointer_cast<RapClass>(roadEntry)->classEntries) {
                        if (roadValues->type == 1 && boost::iequals(roadValues->name, "width")) {
                            if (auto val = std::get_if<int32_t>(&std::static_pointer_cast<RapValue>(roadValues)->value)) {
                                roadPair.first = (float_t)*val;
                            }
                            else {
                                roadPair.first = std::get<float_t>(std::static_pointer_cast<RapValue>(roadValues)->value);
                            }
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
    std::vector<std::string> roadTypes = { "road", "mainroad", "track" };
    auto additionalRoads = std::map<std::string, std::vector<std::array<SimplePoint, 4>>>{};
    for (auto& roadType : roadTypes) {
        if (mapObjects.find(roadType) != mapObjects.end()) {
            for (auto& road : mapObjects.at(roadType)) {
                auto correctedRoadType = roadType;
                if (roadType == "mainroad") {
                    // Blame Zade when this blows up
                    correctedRoadType = "main_road";
                }

                // http://nghiaho.com/?page_id=846
                // Values from roation matrix, needed for the z rotation
                auto r31 = road.first.transformMatrix[2][0];
                auto r32 = road.first.transformMatrix[2][1];
                auto r33 = road.first.transformMatrix[2][2];

                // calculate rotation in RADIANS
                auto theta = std::atan2(-1 * r31, std::sqrt(std::pow(r32, 2) + std::pow(r33, 2)));

                // Corrected center pos
                auto centerCorX = road.first.transformMatrix[3][0] - road.second.bCeneter[0];
                auto centerCorY = road.first.transformMatrix[3][2] - road.second.bCeneter[2];

                // calc rotated pos of corner 
                // https://gamedev.stackexchange.com/a/86780

                /*
                    (x_1,y_1) --- (x_2,y_2)
                        |             |
                        |             |
                    (x_0,y_0) --- (x_3,y_3)
                */

                auto x0 = centerCorX + (road.second.bMin[0] * std::cos(theta)) - (road.second.bMin[2] * std::sin(theta));
                auto y0 = centerCorY + (road.second.bMin[0] * std::sin(theta)) + (road.second.bMin[2] * std::cos(theta));

                auto x1 = centerCorX + (road.second.bMin[0] * std::cos(theta)) - (road.second.bMax[2] * std::sin(theta));
                auto y1 = centerCorY + (road.second.bMin[0] * std::sin(theta)) + (road.second.bMax[2] * std::cos(theta));

                auto x2 = centerCorX + (road.second.bMax[0] * std::cos(theta)) - (road.second.bMax[2] * std::sin(theta));
                auto y2 = centerCorY + (road.second.bMax[0] * std::sin(theta)) + (road.second.bMax[2] * std::cos(theta));

                auto x3 = centerCorX + (road.second.bMax[0] * std::cos(theta)) - (road.second.bMin[2] * std::sin(theta));
                auto y3 = centerCorY + (road.second.bMax[0] * std::sin(theta)) + (road.second.bMin[2] * std::cos(theta));

                std::array<SimplePoint, 4> rectangle = { SimplePoint(x0,y0), SimplePoint(x1,y1), SimplePoint(x2,y2),SimplePoint(x3,y3) };

                auto addRoadsResult = additionalRoads.find(correctedRoadType);
                if (addRoadsResult == additionalRoads.end()) {
                    additionalRoads.insert({ correctedRoadType, {rectangle} });
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
        }
        writeGZJson((key + std::string(".geojson.gz")), basePathGeojsonRoads, value);
    }

    fs::remove_all(basePathGeojsonTemp);
}

void writeTrees(grad_aff::Wrp& wrp, fs::path& basePathGeojson) {

    auto treeLocations = nl::json();
    for (auto& mapInfo : wrp.mapInfo) {
        if (mapInfo->mapType == 1 && mapInfo->infoType == 0) {
            auto mapInfo1Ptr = std::static_pointer_cast<MapType1>(mapInfo);

            auto pointFeature = nl::json();
            pointFeature["type"] = "Feature";

            auto geometry = nl::json();
            geometry["type"] = "Point";

            auto posArray = nl::json::array();
            posArray.push_back((float_t)mapInfo1Ptr->x);
            posArray.push_back((float_t)mapInfo1Ptr->y);
            geometry["coordinates"] = posArray;

            pointFeature["geometry"] = geometry;
            pointFeature["properties"] = nl::json::object();

            treeLocations.push_back(pointFeature);
        }
    }
    writeGZJson("tree.geojson.gz", basePathGeojson, treeLocations);
}

void writeForests(grad_aff::Wrp& wrp, fs::path& basePathGeojson, const std::vector<std::pair<Object, ODOLv4xLod&>>& objectPairs) {
    size_t nPoints = 0;
    pcl::PointCloud<pcl::PointXYZ>::Ptr cloudPtr(new pcl::PointCloud<pcl::PointXYZ>());
    for (auto& objectPair : objectPairs) {
        pcl::PointXYZ point;
        point.x = objectPair.first.transformMatrix[3][0];
        point.y = objectPair.first.transformMatrix[3][2];
        point.z = 0;
        cloudPtr->push_back(point);
        nPoints++;
    }

    pcl::search::KdTree<pcl::PointXYZ>::Ptr tree(new pcl::search::KdTree<pcl::PointXYZ>);
    tree->setInputCloud(cloudPtr);

    std::vector<pcl::PointIndices> cluster_indices;
    pcl::EuclideanClusterExtraction<pcl::PointXYZ> ec;
    ec.setClusterTolerance(15); // 15m
    ec.setMinClusterSize(5);
    //ec.setMaxClusterSize(25000);
    ec.setSearchMethod(tree);
    ec.setInputCloud(cloudPtr);
    ec.extract(cluster_indices);

    pcl::PCDWriter writer;
    ClipperLib::Paths paths;
    paths.reserve(nPoints);

    for (std::vector<pcl::PointIndices>::const_iterator it = cluster_indices.begin(); it != cluster_indices.end(); ++it)
    {
        pcl::PointCloud<pcl::PointXYZ>::Ptr cloudCluster(new pcl::PointCloud<pcl::PointXYZ>);
        for (std::vector<int>::const_iterator pit = it->indices.begin(); pit != it->indices.end(); ++pit)
            cloudCluster->points.push_back(cloudPtr->points[*pit]); //*
        cloudCluster->width = cloudCluster->points.size();
        cloudCluster->height = 1;
        cloudCluster->is_dense = true;

        for (auto& point : *cloudCluster) {
            int dd = 10;
            ClipperLib::Path polygon;
            polygon.push_back(ClipperLib::IntPoint(point.x - dd, point.y));
            polygon.push_back(ClipperLib::IntPoint(point.x, point.y - dd));
            polygon.push_back(ClipperLib::IntPoint(point.x + dd, point.y));
            polygon.push_back(ClipperLib::IntPoint(point.x, point.y + dd));
            paths.push_back(polygon);
        }
    }

    ClipperLib::Paths solution;
    ClipperLib::Clipper c;
    c.AddPaths(paths, ClipperLib::ptSubject, true);
    c.Execute(ClipperLib::ctUnion, solution, ClipperLib::pftNonZero);

    std::vector<OGRLinearRing*> rings;
    rings.reserve(solution.size());

    for (auto& path : solution) {
        auto ring = new OGRLinearRing();
        for (auto& point : path) {
            ring->addPoint(point.X, point.Y);
        }
        ring->closeRings();
        rings.push_back(ring);
    }

    std::vector<OGRGeometry*> polygons;
    polygons.reserve(rings.size());
    for (auto& ring : rings) {
        OGRPolygon poly;
        poly.addRing(ring);
        polygons.push_back(poly.Simplify(3));
        OGRGeometryFactory::destroyGeometry(ring);
    }

    OGRMultiPolygon multiPolygon;
    for (auto& polygon : polygons) {
        multiPolygon.addGeometryDirectly(polygon);
    }
    auto s = multiPolygon.Simplify(10);
    s = s->UnionCascaded();

    nl::json forests;

    nl::json forest;
    forest["type"] = "Feature";

    auto ogrJson = s->exportToJson();
    forest["geometry"] = nl::json::parse(ogrJson);
    CPLFree(ogrJson);
    OGRGeometryFactory::destroyGeometry(s);

    forest["properties"] = { { "color", { 11, 156, 49, 255 } } };
    forests.push_back(forest);

    writeGZJson("forest.geojson.gz", basePathGeojson, forests);
}

float_t calculateDistance(types::auto_array<types::game_value> start, SimpleVector vector, SimpleVector point) {
    SimpleVector vec2 = { point[0] - (float_t)start[0], point[1] - (float_t)start[1] };
    auto vec2Magnitude = std::sqrt(std::pow(vec2[0], 2) + std::pow(vec2[1], 2));

    auto vecProduct = vector[0] * vec2[0] + vector[1] * vec2[1];
    auto vecMagnitude = std::sqrt(std::pow(vector[0], 2) + std::pow(vector[1], 2));
    auto angleInRad = std::acos(vecProduct / (vecMagnitude * vec2Magnitude));
    auto angleInDeg = angleInRad * 180 / M_PI;

    if (angleInDeg >= 90) {
        return 0;
    }

    return std::cos(angleInRad) * vec2Magnitude;
}

float_t calculateMaxDistance(types::auto_array<types::game_value> ilsPos, types::auto_array<types::game_value> ilsDirection, types::auto_array<types::game_value> taxiIn, types::auto_array<types::game_value> taxiOff) {
    SimpleVector dirVec = { (float_t)ilsDirection[0] * -1, (float_t)ilsDirection[2] * -1 };

    float_t maxDistance = 0.0f;

    for (size_t i = 0; i < taxiIn.size(); i += 2) {
        SimpleVector pos = { taxiIn[i], taxiIn[i + 1] };
        maxDistance = std::max(maxDistance, calculateDistance(ilsPos, dirVec, pos));
    }

    for (size_t i = 0; i < taxiOff.size(); i += 2) {
        SimpleVector pos = { taxiOff[i], taxiOff[i + 1] };
        maxDistance = std::max(maxDistance, calculateDistance(ilsPos, dirVec, pos));
    }

    return maxDistance;
}

nl::json buildRunwayPolygon(sqf::config_entry& runwayConfig) {
    auto ilsPos = sqf::get_array(runwayConfig >> "ilsPosition").to_array();
    auto ilsDirection = sqf::get_array(runwayConfig >> "ilsDirection").to_array();
    auto taxiIn = sqf::get_array(runwayConfig >> "ilsTaxiIn").to_array();
    auto taxiOff = sqf::get_array(runwayConfig >> "ilsTaxiOff").to_array();

    auto maxDist = calculateMaxDistance(ilsPos, ilsDirection, taxiIn, taxiOff) + 15;

    auto startPos = SimpleVector{ (float_t)ilsPos[0] + (float_t)ilsDirection[0] * 5, (float_t)ilsPos[1] + (float_t)ilsDirection[2] * 5 };
    auto endPos = SimpleVector{ (float_t)ilsPos[0] + (float_t)ilsDirection[0] * -1 * maxDist, (float_t)ilsPos[1] + (float_t)ilsDirection[2] * -1 * maxDist };

    auto dx = endPos[0] - startPos[0];
    auto dy = endPos[1] - startPos[1];

    auto lineLength = std::sqrt(std::pow(dx, 2) + std::pow(dy, 2));
    dx /= lineLength;
    dy /= lineLength;

    auto expandWidth = 20;
    auto px = expandWidth * -1 * dy;
    auto py = expandWidth * dx;

    nl::json runwaysFeature;
    runwaysFeature["type"] = "Feature";

    auto coordArr = nl::json::array();
    coordArr.push_back({ startPos[0] + px, startPos[1] + py });
    coordArr.push_back({ startPos[0] - px, startPos[1] - py });
    coordArr.push_back({ endPos[0] - px, endPos[1] - py });
    coordArr.push_back({ endPos[0] + px, endPos[1] + py });

    auto outerArr = nl::json::array();
    outerArr.push_back(coordArr);

    runwaysFeature["geometry"] = { { "type" , "Polygon" }, { "coordinates" , outerArr } };
    runwaysFeature["properties"] = nl::json::object();

    return runwaysFeature;
}

// width 40

void writeRunways(fs::path& basePathGeojson, const std::string& worldName) {
    client::invoker_lock threadLock;
    nl::json runways;

    // main runway
    auto runwayConfig = sqf::config_entry(sqf::config_file()) >> "CfgWorlds" >> worldName;
    if (sqf::get_array(runwayConfig >> "ilsPosition").to_array().size() > 0) {
        runways.push_back(buildRunwayPolygon(runwayConfig));
    }

    // secondary runways
    // configfile >> "CfgWorlds" >> worldName >> "SecondaryAirports"
    auto secondaryAirports = sqf::config_classes("true", runwayConfig >> "SecondaryAirports");
    if (secondaryAirports.size() > 0) {
        for (auto& airport : secondaryAirports) {
            runways.push_back(buildRunwayPolygon(sqf::config_entry(airport)));
        }
    }

    writeGZJson("runway.geojson.gz", basePathGeojson, runways);
}

void writeGenericMapTypes(fs::path& basePathGeojson, const std::vector<std::pair<Object, ODOLv4xLod&>>& objectPairs, const std::string& name) {
    if (objectPairs.size() == 0)
        return;

    auto mapTypeLocation = nl::json();
    for (auto& pair : objectPairs) {
        auto pointFeature = nl::json();
        pointFeature["type"] = "Feature";

        auto geometry = nl::json();
        geometry["type"] = "Point";

        auto posArray = nl::json::array();
        posArray.push_back(pair.first.transformMatrix[3][0]);
        posArray.push_back(pair.first.transformMatrix[3][2]);
        geometry["coordinates"] = posArray;

        pointFeature["geometry"] = geometry;
        pointFeature["properties"] = nl::json::object();

        mapTypeLocation.push_back(pointFeature);
    }
    writeGZJson((name + ".geojson.gz"), basePathGeojson, mapTypeLocation);
}

void writePowerlines(grad_aff::Wrp& wrp, fs::path& basePathGeojson) {
    nl::json powerline = nl::json::array();

    for (auto& mapInfo : wrp.mapInfo) {
        if (mapInfo->mapType == 5) {
            auto mapInfo5Ptr = std::static_pointer_cast<MapType5>(mapInfo);
            nl::json mapFeature;
            mapFeature["type"] = "Feature";

            auto coordArr = nl::json::array();
            coordArr.push_back({ mapInfo5Ptr->floats[0], mapInfo5Ptr->floats[1] });
            coordArr.push_back({ mapInfo5Ptr->floats[2], mapInfo5Ptr->floats[3] });

            mapFeature["geometry"] = { { "type" , "LineString" }, { "coordinates" , coordArr } };
            mapFeature["properties"] = nl::json::object();

            powerline.push_back(mapFeature);
        }
    }
    if (!powerline.empty())
        writeGZJson("powerline.geojson.gz", basePathGeojson, powerline);
}

void writeRailways(fs::path& basePathGeojson, const std::vector<std::pair<Object, ODOLv4xLod&>>& objectPairs) {
    if (objectPairs.size() == 0)
        return;

    nl::json railways = nl::json::array();

    for (auto& objectPair : objectPairs) {
        XYZTriplet map1Triplet;
        XYZTriplet map2Triplet;
        std::optional<XYZTriplet> map3Triplet;
        for (auto& namedSelection : objectPair.second.namedSelections) {
            if (namedSelection.selectedName == "map1") {
                map1Triplet = objectPair.second.lodPoints[namedSelection.vertexTableIndexes[0]];
            }
            if (namedSelection.selectedName == "map2") {
                map2Triplet = objectPair.second.lodPoints[namedSelection.vertexTableIndexes[0]];
            }
            if (namedSelection.selectedName == "map3") {
                map3Triplet = objectPair.second.lodPoints[namedSelection.vertexTableIndexes[0]];
            }
        }

        auto xPos = objectPair.first.transformMatrix[3][0];
        auto yPos = objectPair.first.transformMatrix[3][2];

        auto r11 = objectPair.first.transformMatrix[0][0];
        auto r13 = objectPair.first.transformMatrix[0][2];

        auto r34 = 0;
        auto r31 = objectPair.first.transformMatrix[2][0];
        auto r32 = objectPair.first.transformMatrix[2][1];
        auto r33 = objectPair.first.transformMatrix[2][2];

        // calculate rotation in RADIANS
        auto theta = std::atan2(-1 * r31, std::sqrt(std::pow(r32, 2) + std::pow(r33, 2)));
        
        if (r11 == 1.0f || r11 == -1.0f) {
            theta = std::atan2(r13, r34);
        }
        else {
            theta = std::atan2(-1 * r31, r11);
        }

        auto degree = theta * 180 / M_PI;

        auto startPosX = xPos + (map1Triplet[0] * std::cos(theta)) - (map1Triplet[2] * std::sin(theta));
        auto startPosY = yPos + (map1Triplet[0] * std::sin(theta)) + (map1Triplet[2] * std::cos(theta));

        auto endPosX = xPos + (map2Triplet[0] * std::cos(theta)) - (map2Triplet[2] * std::sin(theta));
        auto endPosY = yPos + (map2Triplet[0] * std::sin(theta)) + (map2Triplet[2] * std::cos(theta));
        
        nl::json railwayFeature;
        railwayFeature["type"] = "Feature";

        auto coordArr = nl::json::array();
        coordArr.push_back({ startPosX, startPosY });
        coordArr.push_back({ endPosX, endPosY });

        railwayFeature["geometry"] = { { "type" , "LineString" }, { "coordinates" , coordArr } };
        railwayFeature["properties"] = nl::json::object();

        railways.push_back(railwayFeature);

        if (map3Triplet) {
            nl::json railwaySwitchFeature;
            railwaySwitchFeature["type"] = "Feature";

            auto coordArr = nl::json::array();
            auto switchStartPosX = xPos + ((*map3Triplet)[0] * std::cos(theta)) - ((*map3Triplet)[2] * std::sin(theta));
            auto switchStartPosY = yPos + ((*map3Triplet)[0] * std::sin(theta)) + ((*map3Triplet)[2] * std::cos(theta));

            coordArr.push_back({ switchStartPosX, switchStartPosY });
            coordArr.push_back({ endPosX, endPosY });

            railwaySwitchFeature["geometry"] = { { "type" , "LineString" }, { "coordinates" , coordArr } };
            railwaySwitchFeature["properties"] = nl::json::object();
            railways.push_back(railwaySwitchFeature);
        }

    }
    writeGZJson("railway.geojson.gz", basePathGeojson, railways);
}

void writeGeojsons(grad_aff::Wrp& wrp, std::filesystem::path& basePathGeojson, const std::string& worldName)
{
    std::vector<ODOLv4xLod> modelInfos;
    modelInfos.reserve(wrp.models.size());

    std::vector<std::pair<std::string, ODOLv4xLod>> modelMapTypes;
    modelMapTypes.resize(wrp.models.size());

    std::map<fs::path, std::shared_ptr<grad_aff::Pbo>> pboMap;
    for (int i = 0; i < wrp.models.size(); i++) {
        auto modelPath = wrp.models[i];
        if (boost::starts_with(modelPath, "\\")) {
            modelPath = modelPath.substr(1);
        }

        auto pboPath = findPboPath(modelPath);
        std::shared_ptr<grad_aff::Pbo> pbo = {};
        auto res = pboMap.find(pboPath);
        if (res == pboMap.end()) {
            auto pboPtr = std::make_shared<grad_aff::Pbo>(pboPath.string());
            pboMap.insert({ pboPath, pboPtr });
            pbo = pboPtr;
        }
        else {
            pbo = res->second;
        }

        auto p3dData = pbo->getEntryData(modelPath);

        auto odol = grad_aff::Odol(p3dData);
        odol.readOdol(false);

        odol.peekLodTypes();

        auto geoIndex = -1;
        for (int j = 0; j < odol.lods.size(); j++) {
            if (odol.lods[j].lodType == LodType::GEOMETRY) {
                geoIndex = j;
            }
        }

        if (geoIndex != -1) {
            auto retLod = odol.readLod(geoIndex);

            auto findClass = retLod.tokens.find("map");
            if (findClass != retLod.tokens.end()) {
                if (findClass->second == "railway") {
                    geoIndex = -1;
                    for (int j = 0; j < odol.lods.size(); j++) {
                        if (odol.lods[j].lodType == LodType::SPECIAL_LOD) {
                            geoIndex = j;
                        }
                    }
                    auto memLod = odol.readLod(geoIndex);
                    modelMapTypes[i] = { findClass->second, memLod };
                }
                else {
                    modelMapTypes[i] = { findClass->second, retLod };
                }
            }
        }
    }

    std::map<std::string, std::vector<std::pair<Object, ODOLv4xLod&>>> objectMap = {};
    /*
    std::vector<Point_> forestPositions = {};
    std::vector<Point_> forestSquarePositions = {};
    std::vector<Point_> forestTrianglePositions = {};
    std::vector<Point_> forestBroderPositions = {};

    std::vector<Point_> forestFeulStatPositions = {};
    std::vector<Point_> forestBroderPositions = {};
    */
    for (auto& object : wrp.objects) {
        auto mapType = modelMapTypes[object.modelIndex].first;
        auto res = objectMap.find(mapType);
        if (res != objectMap.end()) {
            res->second.push_back({object, modelMapTypes[object.modelIndex].second});
        }
        else {
            objectMap.insert({ mapType, {{object, modelMapTypes[object.modelIndex].second}} });
        }
        /*
        if (!forestModels[index].empty()) {
            Point_ p;
            p.x = object.transformMatrix[3][0];
            p.y = object.transformMatrix[3][2];
            p.z = 0;
            p.clusterID = UNCLASSIFIED;
            forestPositions.push_back(p);
        }
        */
    }

    std::vector<std::string> genericMapTypes = { "bunker", "chapel", "church", "cross", "fuelstation", "lighthouse", "rock", "shipwreck", "transmitter", "tree", "watertower",
                                                "fortress", "fountain", "view-tower", "quay", "hospital", "busstop", "stack", "ruin", "tourism", "powersolar", "powerwave", "powerwind" };

    for (auto& genericMapType : genericMapTypes) {
        if (objectMap.find(genericMapType) != objectMap.end()) {
            if (genericMapType == "tree") {
                writeForests(wrp, basePathGeojson, objectMap["tree"]);
            }
            else {
                writeGenericMapTypes(basePathGeojson, objectMap[genericMapType], genericMapType);
            }
        }
    }
        
    if(objectMap.find("railway") != objectMap.end())
        writeRailways(basePathGeojson, objectMap["railway"]);

    writeHouses(wrp, basePathGeojson);
    writeObjects(wrp, basePathGeojson);
    writeLocations(worldName, basePathGeojson);
    writeRoads(wrp, worldName, basePathGeojson, objectMap);
    writeTrees(wrp, basePathGeojson);
    writePowerlines(wrp, basePathGeojson);

    writeRunways(basePathGeojson, worldName);

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
