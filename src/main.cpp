#include <intercept.hpp>

#include "util.h"
#include "geojsons.h"
#include "satimages.h"

#include "version.h"

#include <OpenImageIO/imageio.h>

// String
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string.hpp>

// Gzip
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>

// #include <rust-lib/foo/>
#include <rust-lib/lib.h>

#include <sstream>
#include <algorithm>
#include <array>
#include <array>
#include <fstream>
#include <iostream>
#include <list>
#include <vector>
#include <filesystem>

#include <nlohmann/json.hpp>

#include <ogr_geometry.h>

#include <fmt/format.h>
#include <fmt/chrono.h>
#include <fmt/format.h>

// spdlog
#include "spdlog/spdlog.h"
#include "spdlog/async.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/stopwatch.h"

#include "findPbos.h"
#include "SimplePoint.h"

#include "../addons/main/status_codes.hpp"

using namespace intercept;
using namespace OIIO;

namespace fs = std::filesystem;
namespace nl = nlohmann;
namespace ba = boost::algorithm;
namespace bi = boost::iostreams;
using iet = types::game_state::game_evaluator::evaluator_error_type;

using SQFPar = game_value_parameter;

static bool gradMehIsRunning = false;

int intercept::api_version()
{ // This is required for the plugin to work.
    return INTERCEPT_SDK_API_VERSION;
}

void writeMeta(const std::string &worldName, const int32_t &worldSize, fs::path &basePath)
{
    client::invoker_lock threadLock;
    auto mapConfig = sqf::config_entry(sqf::config_file()) >> "CfgWorlds" >> worldName;
    nl::json meta;
    meta["version"] = GRAD_MEH_GIT_LAST_VERSION;
    meta["worldName"] = ba::to_lower_copy(worldName);
    meta["worldSize"] = worldSize;
    meta["author"] = sqf::get_text(mapConfig >> "author");
    meta["displayName"] = sqf::get_text(mapConfig >> "description");
    meta["elevationOffset"] = sqf::get_number(mapConfig >> "elevationOffset");
    meta["latitude"] = sqf::get_number(mapConfig >> "latitude");
    meta["longitude"] = sqf::get_number(mapConfig >> "longitude");
    meta["gridOffsetX"] = sqf::get_number(mapConfig >> "Grid" >> "offsetX");
    meta["gridOffsetY"] = sqf::get_number(mapConfig >> "Grid" >> "offsetY");

    auto colorArray = sqf::get_array(mapConfig >> "OutsideTerrain" >> "colorOutside").to_array();
    if (!colorArray.empty())
    {
        meta["colorOutside"] = std::vector<float_t>(colorArray.begin(), colorArray.end());
    }

    auto gridArray = nl::json::array();

    for (auto &grid : sqf::config_classes("true", (mapConfig >> "Grid")))
    {
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

void writeDem(fs::path &basePath, rvff::cxx::OprwCxx &wrp, const int32_t &worldSize)
{
    auto cellsize = (float_t)worldSize / wrp.map_size_x;

    std::stringstream demStringStream;
    demStringStream << "ncols " << wrp.map_size_x << std::endl;
    demStringStream << "nrows " << wrp.map_size_y << std::endl;
    demStringStream << "xllcorner " << 0.0 << std::endl;
    demStringStream << "yllcorner " << -cellsize << std::endl;
    demStringStream << "cellsize " << cellsize << std::endl; // worldSize / mapsizex
    demStringStream << "NODATA_value " << -9999;
    demStringStream << std::endl;

    for (int64_t y = wrp.map_size_y - 1; y >= 0; y--)
    {
        for (size_t x = 0; x < wrp.map_size_x; x++)
        {
            demStringStream << wrp.elevation[x + wrp.map_size_x * y] << " ";
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

void writePreviewImage(const std::string &worldName, std::filesystem::path &basePath)
{
    client::invoker_lock threadLock;
    auto mapConfig = sqf::config_entry(sqf::config_file()) >> "CfgWorlds" >> worldName;
    std::string picturePath = sqf::get_text(mapConfig >> "pictureMap");
    threadLock.unlock();

    if (picturePath.empty()) {
        return;
    }

    if (boost::starts_with(picturePath, "\\")) {
        picturePath = picturePath.substr(1);
    }

    auto pboPath = findPboPath(picturePath);
    if (pboPath.empty()) {
        return;
    }

    //grad_aff::Pbo prewviewPbo(pboPath.string());
    auto previewPbo = rvff::cxx::create_pbo_reader_path(pboPath.string());
    auto previewData = previewPbo->get_entry_data(picturePath);
    auto previewMipmap = rvff::cxx::get_mipmap_from_paa_vec(previewData, 0);

    auto previewFileName = (basePath / "preview.png").string();

    std::unique_ptr<ImageOutput> out = ImageOutput::create(previewFileName);
    if (!out)
        return;
    ImageSpec spec(previewMipmap.width, previewMipmap.height, 4, TypeDesc::UINT8);
    out->open(previewFileName, spec);
    out->write_image(TypeDesc::UINT8, previewMipmap.data.data());
    out->close();

    //paa.writeImage((basePath / "preview.png").string());
}

void extractMap(const std::string &worldName, const std::string &worldPath, std::array<bool, 5> &steps)
{

    auto lowerWorldName = boost::algorithm::to_lower_copy(worldName);

    auto basePath = fs::path("grad_meh") / lowerWorldName;
    auto basePathGeojson = fs::path("grad_meh") / lowerWorldName / "geojson";
    auto basePathSat = fs::path("grad_meh") / lowerWorldName / "sat";

    std::stringstream startMsg;
    startMsg << "Starting export of " << worldName << " [";
    startMsg << std::boolalpha;

    for (auto i = 0; i < steps.size(); i++)
    {
        startMsg << steps[i];
        if (i < (steps.size() - 1))
        {
            startMsg << ", ";
        }
    }
    startMsg << "]";

    prettyDiagLog(startMsg.str());

    if (!fs::exists(basePath))
    {
        fs::create_directories(basePath);
    }
    if (!fs::exists(basePathGeojson))
    {
        fs::create_directories(basePathGeojson);
    }
    if (!fs::exists(basePathSat))
    {
        fs::create_directories(basePathSat);
    }

    std::string curWorldPath = "";
    try {
        // Find Wrp Path
        auto wrpPath = findPboPath(worldPath);
        auto wrpPboReader = rvff::cxx::create_pbo_reader_path(wrpPath.string());

        auto wrp_data = wrpPboReader->get_entry_data(worldPath);

        auto wrp = rvff::cxx::OprwCxx{};
        // wrp.wrpName = worldName + ".wrp";
        try
        {
            if (steps[0] || steps[1] || steps[3] || steps[4])
            {
                reportStatus(worldName, "read_wrp", "running");
                wrp = rvff::cxx::create_wrp_from_vec(wrp_data);
                reportStatus(worldName, "read_wrp", "done");
            }
            else
            {
                reportStatus(worldName, "read_wrp", "canceled");
            }
        }
        catch (std::exception& ex)
        { // most likely caused by unknown mapinfo type
            client::invoker_lock threadLock;
            prettyDiagLog(std::string("exception while reading the wrp: ").append(ex.what()));
            sqf::hint(ex.what());
            return;
        }

        auto worldSize = (uint32_t)wrp.layer_cell_size * wrp.layer_size_x;

        if (steps[0])
        {
            reportStatus(worldName, "write_sat", "running");
            prettyDiagLog("Exporting sat images");
            writeSatImages(wrp, worldSize, basePathSat, worldName);
            reportStatus(worldName, "write_sat", "done");
        }
        if (steps[1])
        {
            reportStatus(worldName, "write_houses", "running");
            prettyDiagLog("Exporting geojson");
            writeGeojsons(wrp, basePathGeojson, worldName);
            reportStatus(worldName, "write_houses", "done");
        }

        if (steps[2])
        {
            reportStatus(worldName, "write_preview", "running");
            prettyDiagLog("Exporting preview image");
            writePreviewImage(worldName, basePath);
            reportStatus(worldName, "write_preview", "done");
        }

        if (steps[3])
        {
            reportStatus(worldName, "write_meta", "running");
            prettyDiagLog("Exporting meta json");
            writeMeta(worldName, worldSize, basePath);
            reportStatus(worldName, "write_meta", "done");
        }

        if (steps[4])
        {
            reportStatus(worldName, "write_dem", "running");
            prettyDiagLog("Exporting dem file");
            writeDem(basePath, wrp, worldSize);
            reportStatus(worldName, "write_dem", "done");
        }

    }
    catch (const rust::Error& ex) {
        spdlog::error(fmt::format("Exception in extract map command"));
        log_error(ex);
        throw ex;
    }

    gradMehIsRunning = false;
    return;
}

game_value exportMapCommand(game_state &gs, SQFPar rightArg)
{

    if (gradMehIsRunning)
        return GRAD_MEH_STATUS_ERR_ALREADY_RUNNING;

    if (isMapPopulating())
        return GRAD_MEH_STATUS_ERR_PBO_POPULATING;

    std::string worldName;

    // [sat image, houses, preview img, meta.json, dem.asc]
    std::array<bool, 5> steps = { true, true, true, true, true };

    if (rightArg.type_enum() == game_data_type::STRING)
    {
        worldName = r_string(rightArg).c_str();
    }
    else if (rightArg.type_enum() == game_data_type::ARRAY)
    {
        auto parArray = rightArg.to_array();

        if (parArray.size() <= 0 || parArray.size() >= 7)
        {
            gs.set_script_error(iet::assertion_failed, "Wrong amount of arguments!"sv);
            return GRAD_MEH_STATUS_ERR_ARGS;
        }

        if (parArray[0].type_enum() == game_data_type::STRING)
        {

            worldName = r_string(parArray[0]);

            for (int i = 1; i < parArray.size(); i++)
            {
                if (parArray[i].type_enum() == game_data_type::BOOL)
                {
                    auto b = (bool)parArray[i];
                    steps[i - 1] = b;
                }
                else
                {
                    gs.set_script_error(iet::assertion_failed, types::r_string("Expected bool at index ").append(std::to_string(i)).append("!"));
                    return GRAD_MEH_STATUS_ERR_ARGS;
                }
            }
        }
        else
        {
            gs.set_script_error(iet::assertion_failed, "First element in the parameter array has to be a string!"sv);
            return GRAD_MEH_STATUS_ERR_ARGS;
        }
    }
    else
    {
        gs.set_script_error(iet::assertion_failed, "Expected a string or an array!"sv);
        return GRAD_MEH_STATUS_ERR_ARGS;
    }

    auto configWorld = sqf::config_entry(sqf::config_file()) >> "CfgWorlds" >> worldName;
    if (!boost::iequals(sqf::config_name(configWorld), worldName))
    {
        gs.set_script_error(iet::assertion_failed, "Couldn't find the specified world!"sv);
        return GRAD_MEH_STATUS_ERR_NOT_FOUND;
    }

    // check for leading /
    std::string worldPath = sqf::get_text(configWorld >> "worldName");
    if (boost::starts_with(worldPath, "\\"))
    {
        worldPath = worldPath.substr(1);
    }

    // try to find pbo
    auto wrpPboPath = findPboPath(worldPath);
    if (wrpPboPath == "")
    {
        return GRAD_MEH_STATUS_ERR_PBO_NOT_FOUND;
    }
    else
    {
        try
        {
            auto wrpPboReader = rvff::cxx::create_pbo_reader_path(wrpPboPath.string());
            if (!wrpPboReader->has_entry(worldPath))
                return GRAD_MEH_STATUS_ERR_PBO_NOT_FOUND;
        }
        catch (std::exception &ex)
        {
            prettyDiagLog(std::string("Exception when opening PBO: ").append(ex.what()));
            return GRAD_MEH_STATUS_ERR_PBO_NOT_FOUND;
        }
    }

    if (!gradMehIsRunning)
    {
        gradMehIsRunning = true;
        std::thread readWrpThread(extractMap, worldName, worldPath, steps);
        readWrpThread.detach();
        return GRAD_MEH_STATUS_OK;
    }
    else
    {
        prettyDiagLog("gradMeh is already running! Aborting!");
        return GRAD_MEH_STATUS_ERR_ALREADY_RUNNING;
    }
}

void intercept::pre_start()
{
    static auto grad_meh_export_map_string =
        client::host::register_sqf_command("gradMehExportMap", "Exports the given map", exportMapCommand, game_data_type::SCALAR, game_data_type::STRING);
    static auto grad_meh_export_map_array =
        client::host::register_sqf_command("gradMehExportMap", "Exports the given map", exportMapCommand, game_data_type::SCALAR, game_data_type::ARRAY);

    std::thread mapPopulateThread(populateMap);
    mapPopulateThread.detach();
}

void intercept::pre_init()
{
    std::stringstream preInitMsg;

    preInitMsg << "The grad_meh plugin is running! (";

    if (GRAD_MEH_GIT_TAG.empty())
    {
        preInitMsg << GRAD_MEH_GIT_REV;
    }
    else
    {
        preInitMsg << GRAD_MEH_GIT_TAG;
    }

    preInitMsg << "@" << GRAD_MEH_GIT_BRANCH << ")";
    intercept::sqf::system_chat(preInitMsg.str());
    prettyDiagLog(preInitMsg.str());

#if WIN32
    std::filesystem::path a3_log_path;
    PWSTR path_tmp;

    auto get_folder_path_ret = SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path_tmp);

    if (get_folder_path_ret == S_OK) {
        a3_log_path = path_tmp;
    }
    CoTaskMemFree(path_tmp);

    a3_log_path = a3_log_path / "Arma 3";

#endif

    auto async_file = spdlog::basic_logger_mt<spdlog::async_factory>(
        "meh-logger",
        fmt::format("{}/grad_meh_{:%Y-%m-%d_%H-%M-%S}.log", a3_log_path.string(), std::chrono::system_clock::now()));
    spdlog::flush_every(std::chrono::seconds(10));
    spdlog::flush_on(spdlog::level::err);
    spdlog::set_default_logger(async_file);
    spdlog::info(preInitMsg.str());
}
