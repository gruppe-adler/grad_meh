#include "util.h"

static std::map<std::string, fs::path> entryPboMap = {};
static bool gradMehMapIsPopulating = false;

/*
    Returns full paths to every *.pbo from mod Paths + A3 Root
*/
std::vector<fs::path> getFullPboPaths() {
    std::vector<std::wstring> pboList;

#ifdef _WIN32
    for (auto& pboW : generate_pbo_list()) {
        pboList.push_back(pboW.substr(4));
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
    gradMehMapIsPopulating = true;
    for (auto& p : getFullPboPaths()) {
        try {
            auto pboReader = rvff::cxx::create_pbo_reader_path(p.string());
            auto pbo = pboReader->get_pbo();
            for (auto& entry : pbo.entries) {
                for (auto& prop : pbo.properties) {
                    if (static_cast<std::string>(prop.key) == "prefix") {
                        entryPboMap.insert({ static_cast<std::string>(prop.value) , p });
                        break;
                    }
                }
            }
        }
        catch (const rust::Error& ex) {
            PLOG_WARNING << fmt::format("Couldn't open PBO at: {} (Exception: {})", p.string(), ex.what());
        }
    }
    gradMehMapIsPopulating = false;
    PLOG_INFO << "Finished PBO Mapping";
}

/*
    Finds pbo Path
*/
fs::path findPboPath(std::string path) {
    PLOG_INFO << fmt::format("Searching pbos for: {}", path);
    size_t matchLength = 0;
    fs::path retPath;

    if (boost::starts_with(path, "\\")) {
        path = path.substr(1);
    }

    for (auto const& [key, val] : entryPboMap)
    {
        if (boost::istarts_with(path, key) && key.length() > matchLength) {
            matchLength = key.length();
            retPath = val;
        }
    }

    if (retPath.empty()) {
        PLOG_WARNING << fmt::format("No pbo found for: {}", path);
    }
    else {
        PLOG_INFO << fmt::format("Found pbo at: {}", retPath.string());
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
    fis.push(bi::gzip_compressor(bi::gzip_params(bi::gzip::best_speed)));
    fis.push(strInStream);

    std::ofstream out(path / fileName, std::ios::binary);
    bi::copy(fis, out);
    out.close();
}

bool isMapPopulating() {
    return gradMehMapIsPopulating;
}

// thread safe
void prettyDiagLog(std::string message) {
    client::invoker_lock thread_lock;
    sqf::diag_log(sqf::text("[GRAD] (meh): " + message));
}

bool checkMagic(rust::Vec<uint8_t>& data, std::string magic) {
    if (data.size() < magic.size()) {
        return false;
    }

    for (size_t i = 0; i < magic.size(); i++) {
        if (data[i] != magic[i]) {
            return false;
        }
    }
    return true;
}

fs::path getDllPath() {
#ifdef _WIN32
    char filePath[MAX_PATH + 1];
    GetModuleFileName(HINST_THISCOMPONENT, filePath, MAX_PATH + 1);
#endif

    auto dllPath = fs::path(filePath);
    dllPath = dllPath.remove_filename();

#ifdef _WIN32
    // Remove win garbage
    dllPath = dllPath.string().substr(4);
#endif

    return dllPath;
}

std::map<std::string, int32_t> overlapMap = {};
bool mapInit = false;

int32_t getConfigOverlap(std::string worldName) {
    if (!mapInit) {
        try {
            std::ifstream oc(getDllPath() / "rb_overlap_config.json");
            nl::json data = nl::json::parse(oc);
            PLOG_INFO << "Reading rb_overlap_config.json";
            for (auto& el : data.items()) {
                PLOG_INFO << fmt::format("Key {} - Value {}", el.key(), el.value());
                overlapMap.insert({ el.key(), el.value() });
            }
            mapInit = true;
        }
        catch (std::exception& ex) {
            PLOG_ERROR << fmt::format("Couldn't read rb_overlap_config.json! Exception: {}", ex.what());
        }
    }

    auto overlap = overlapMap.find(worldName);
    if (overlap != overlapMap.end()) {
        return overlap->second;
    } else {
        return -1;
    }
}

