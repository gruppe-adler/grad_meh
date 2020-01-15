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

#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>

#include <nlohmann/json.hpp>

#include <grad_aff/wrp/wrp.h>
#include <grad_aff/paa/paa.h>
#include <grad_aff/pbo/pbo.h>
#include <grad_aff/rap/rap.h>

#ifdef _WIN32
#include <Windows.h>
#else
#error Only Windows is supported
#endif // _WIN32

#define GRAD_MEH_VERSION 0.1

using namespace intercept;
using namespace OpenImageIO_v2_1;

namespace fs = std::filesystem;
namespace nl = nlohmann;
namespace ba = boost::algorithm;
namespace bi = boost::iostreams;

using SQFPar = game_value_parameter;

std::map<std::string, fs::path> entryPboMap = {};

int intercept::api_version() { //This is required for the plugin to work.
    return INTERCEPT_SDK_API_VERSION;
}

/*
    Returns all mod Paths including the A3 Root Path
*/
std::vector<fs::path> getAddonsPaths() {
    // is there a better way?
    client::invoker_lock threadLock;
    auto cmdLineArray = sqf::call(sqf::compile("gaclaGetCmdLineArray")).to_array();
    threadLock.unlock();
    std::vector<fs::path> retPaths;

    // Get a3 Path
    fs::path a3Path;  
    TCHAR filename[MAX_PATH];
    GetModuleFileName(NULL, filename, MAX_PATH);
    a3Path = ((fs::path)filename).remove_filename();

    // Collect Paths from "-mod"
    for (auto &argPair : cmdLineArray) {
        std::string arg(argPair[0].get_as<game_data_string>()->to_string());
        boost::replace_all(arg, "\"", "");

        if (boost::iequals(arg, "mod")) {
            std::string par(argPair[1].get_as<game_data_string>()->to_string());
            boost::replace_all(par, "\"", "");
            if (par.find(";") != std::string::npos) {
                std::vector<std::string> splitted;
                boost::split(splitted, par, boost::is_any_of(";"));
                splitted.erase(std::remove_if(splitted.begin(),
                    splitted.end(),
                    [](std::string x) { return x.empty(); }),
                    splitted.end());
                retPaths.insert(retPaths.end(), splitted.begin(), splitted.end());
            }
            else {
                retPaths.push_back(par);
            }

        }
    }

    for (auto& pboPath : retPaths) {
        if (boost::starts_with(pboPath.string(), "@")) {
            pboPath = a3Path / pboPath;
        }
        else if (boost::iequals(pboPath.string(), "GM")) {
            pboPath = a3Path / pboPath;
        }
    }
    retPaths.push_back(a3Path);

    return retPaths;
}

/*
    Returns full paths to every *.pbo from mod Paths + A3 Root
*/
std::vector<fs::path> getFullPboPaths() {
    std::vector<fs::path> pboPaths = {};
    auto ext(".pbo");
    for (auto& modPaths : getAddonsPaths()) {
        for (auto& p : fs::recursive_directory_iterator(modPaths))
        {
            if (p.path().extension() == ext) {
                std::cout << p << '\n';
                pboPaths.push_back(p);
            }
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
        auto pboName = p;
        pbo.setPboName(pboName.filename().replace_extension("").string());
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
}

/*
    Finds pbo Path
*/
fs::path findPboPath(std::string path) {
    size_t matchLength = 0;
    fs::path retPath;

    for (auto const& [key, val] : entryPboMap)
    {
        if (boost::starts_with(ba::to_lower_copy(path), ba::to_lower_copy(key)) &&
            key.length() > matchLength) {
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

void writeMeta(const std::string& worldName, std::filesystem::path& basePath)
{
    client::invoker_lock threadLock;
    auto mapConfig = sqf::config_entry(sqf::config_file()) >> "CfgWorlds" >> worldName;
    nl::json meta;
    meta["version"] = GRAD_MEH_VERSION;
    meta["worldName"] = sqf::world_name();
    meta["worldSize"] = sqf::world_size();
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

    auto locationArray = nl::json::array();

    for (auto& location : sqf::config_classes("true", (mapConfig >> "Names"))) {
        auto locationEntry = sqf::config_entry(location);

        nl::json entryGrid;
        entryGrid["type"] = sqf::get_text(locationEntry >> "type");

        auto posArray = nl::json::array();
        auto posArr = sqf::get_array(locationEntry >> "position");
        for (auto& pos : posArr.to_array()) {
            posArray.push_back((float_t)pos);
        }
        entryGrid["position"] = posArray;

        entryGrid["name"] = sqf::get_text(locationEntry >> "name");
        entryGrid["radiusA"] = (int32_t)sqf::get_number(locationEntry >> "radiusA");
        entryGrid["radiusB"] = (int32_t)sqf::get_number(locationEntry >> "radiusB");
        entryGrid["angle"] = (int32_t)sqf::get_number(locationEntry >> "angle");

        locationArray.push_back(entryGrid);
    }

    meta["locations"] = locationArray;
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

    int64_t c = 0;
    for (int64_t i = wrp.elevation.size() - 1; i > 0; i--) {
        if ((c % wrp.mapSizeX) == 0) {
            demStringStream << std::endl;
        }
        demStringStream << wrp.elevation[i] << " ";
        c++;
    }
    demStringStream << std::endl;

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

    auto pboPath = findPboPath(picturePath);
    grad_aff::Pbo prewviewPbo(pboPath.string());

    auto paa = grad_aff::Paa(prewviewPbo.getEntryData(picturePath));
    paa.readPaa();
    paa.writeImage((basePath / "preview.png").string());
    threadLock.lock();
}

void writeHouseGeojson(grad_aff::Wrp& wrp, std::filesystem::path& basePathGeojson)
{
    nl::json house = nl::json::array();

    for (auto& mapInfo : wrp.mapInfo) {
        if (mapInfo->mapType == 4) {
            auto mapInfo4Ptr = std::static_pointer_cast<MapType4>(mapInfo);
            nl::json mapFeature;
            mapFeature["type"] = "Feature";

            auto coordArr = nl::json::array();
            for (int32_t i = 0; i < mapInfo4Ptr->bounds.size(); i += 2) {
                coordArr.push_back(std::vector<float_t> { mapInfo4Ptr->bounds[i], mapInfo4Ptr->bounds[i + 1] });
            }

            auto outerArr = nl::json::array();
            outerArr.push_back(coordArr);

            mapFeature["geometry"] = { { "type" , "Polygon" },{ "coordinates" , outerArr } };
            mapFeature["properties"] = { "color", mapInfo4Ptr->color };

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

void writeSatImages(grad_aff::Wrp& wrp, const int32_t& worldSize, std::filesystem::path& basePathSat)
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

        std::mutex rapMutex;

        for(auto& rvmatPath : rvmats) {
            if (!boost::istarts_with(((fs::path)rvmatPath).filename().string(), lastValidRvMat)) {
                auto rap = grad_aff::Rap::Rap(rvmatPbo.getEntryData(rvmatPath));
                rap.readRap();
                
                for (auto& entry : rap.classEntries) {
                    if (entry->name == "Stage0") {
                        auto rapClassPtr = std::static_pointer_cast<RapClass>(entry);

                        for (auto& subEntry : rapClassPtr->classEntries) {
                            if (subEntry->name == "texture") {
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

        int32_t layerCellSize = wrp.layerCellSize;
        ImageBuf dst(ImageSpec(worldSize, worldSize, 4, TypeDesc::UINT8));

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

            ImageBufAlgo::paste(dst, (x * paa.mipMaps[0].height - x * layerCellSize), (y * paa.mipMaps[0].height - y * layerCellSize), 0, 0, src);
        }

        size_t tileSize = worldSize / 4;

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

void extractMap(const std::string& worldName, const std::string& worldPath, const int32_t& worldSize) {

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

    // Populate Pbo Map
    if (entryPboMap.size() == 0)
        populateMap();

    std::string curWorldPath = "";

    // Find Wrp Path
    auto wrpPath = findPboPath(worldPath);
    grad_aff::Pbo wrpPbo(wrpPath.string());

    auto wrp = grad_aff::Wrp(wrpPbo.getEntryData(worldPath));
    wrp.wrpName = worldName + ".wrp";
    try {
        wrp.readWrp();
        //reportStatus(worldName, "read_wrp", "done");
    }
    catch (std::exception & ex) { // most likely caused by unknown mapinfo type
        client::invoker_lock threadLock;
        sqf::diag_log(ex.what());
        sqf::hint(ex.what());
        return;
    }

    writeSatImages(wrp, worldSize, basePathSat);
    writeHouseGeojson(wrp, basePathGeojson);
    writePreviewImage(worldName, basePath);
    writeMeta(worldName, basePath);
    writeDem(basePath, wrp, worldSize);
}

game_value exportMapCommand(game_state& gs, SQFPar rightArg) {

    if (rightArg.type_enum() != game_data_type::STRING) {
        gs.set_script_error(types::game_state::game_evaluator::evaluator_error_type::assertion_failed, "Expected a string!"sv);
        return false;
    }

    std::string worldName = r_string(rightArg).c_str();

    auto configWorld = sqf::config_entry(sqf::config_file()) >> "CfgWorlds" >> worldName;
    if (sqf::config_name(configWorld) != worldName) {
        gs.set_script_error(types::game_state::game_evaluator::evaluator_error_type::assertion_failed, "Couldn't find the specified world!"sv);
        return false;
    }

    // Filters out maps like CAWorld
    auto worldSize = (int32_t)sqf::get_number(configWorld >> "mapSize");
    if (worldSize == 0) {
        gs.set_script_error(types::game_state::game_evaluator::evaluator_error_type::assertion_failed, "Invalid world!"sv);
        return false;
    }

    // removes leading /
    std::string worldPath = sqf::get_text(configWorld >> "worldName").substr(1);

    std::thread readWrpThread(extractMap, worldName, worldPath, worldSize);
    readWrpThread.detach();
    return true;
}

void intercept::pre_start() {
    static auto grad_meh_export_map =
        client::host::register_sqf_command("gradMehExportMap", "Exports the given map", exportMapCommand, game_data_type::BOOL, game_data_type::STRING);
}

void intercept::pre_init() {
    intercept::sqf::system_chat("The grad_meh plugin is running!");
}