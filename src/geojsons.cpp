#include "geojsons.h"

#include "area.h"

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

void normalizePolygon(nl::json& arr)
{
    if (arr.is_array()) {

        // make sure the first point is also the last point
        std::vector<float_t> first = arr.at(0);
        std::vector<float_t> last = arr.at(arr.size() - 1);
        if (first != last) {
            arr.push_back(first);
        }

        // make sure the winding order is counterclockwise
        // https://stackoverflow.com/a/1165943
        auto sum = 0.0f;
        for (auto i = 1; i < arr.size(); i++) {
            std::vector<float_t> p1 = arr.at(i - 1);
            std::vector<float_t> p2 = arr.at(i);
            sum += (p2[0] - p1[0]) * (p2[1] + p1[1]);
        }
        if (sum > 0.0f) {
            std::reverse(arr.begin(), arr.end());
        }
    }
}

void writeHouses(rvff::cxx::OprwCxx& wrp, std::filesystem::path& basePathGeojson, std::vector<rvff::cxx::LodCxx> modelInfos)
{
    nl::json house = nl::json::array();

    for (auto& mapInfo : wrp.map_infos_4) {
        //if (mapInfo->mapType == 4) {
            //auto mapInfo4Ptr = std::static_pointer_cast<MapType4>(mapInfo);
        nl::json mapFeature;
        mapFeature["type"] = "Feature";

        auto coordArr = nl::json::array();
        coordArr.push_back(std::vector<float_t> { mapInfo.bounds.a.x, mapInfo.bounds.a.y });
        coordArr.push_back(std::vector<float_t> { mapInfo.bounds.b.x, mapInfo.bounds.b.y });
        coordArr.push_back(std::vector<float_t> { mapInfo.bounds.d.x, mapInfo.bounds.d.y });
        coordArr.push_back(std::vector<float_t> { mapInfo.bounds.c.x, mapInfo.bounds.a.y });
        coordArr.push_back(std::vector<float_t> { mapInfo.bounds.a.x, mapInfo.bounds.a.y });

        normalizePolygon(coordArr);

        auto outerArr = nl::json::array();
        outerArr.push_back(coordArr);

        mapFeature["geometry"] = { { "type" , "Polygon" }, { "coordinates" , outerArr } };

        /**
         * Arma takes the color from the config and modifies it to make sure none of the rgb-values is bigger
         * than 128. This ensures that the houses have a big enough contrast against the backgorund.
         *
         * So we'll check if any of r/g/b is bigger than 128 and if that's the case we'll calculate the
         * factor we need to apply to reduce it to 128. After that we'll just apply that factor to all
         * values, round the result and then we're done.
         *
         * Oh yeah and the opacity / alpha value is just discarded completely. All houses have a opacity of 100%
         *
         * Don't ask me why they don't just multiply/overlay the config colors with a base color... Just BI things I guess
         * - DZ
         */

         // r, g, b
        auto color = std::vector<uint8_t>{ mapInfo.color[2], mapInfo.color[1], mapInfo.color[0] };

        auto maxColorValue = std::max_element(std::begin(color), std::end(color));

        // make sure every color-value is below or equal to 128
        if (*maxColorValue > 128) {
            float_t factor = 128.0f / *maxColorValue;

            std::transform(color.begin(), color.end(), color.begin(), [factor](uint8_t value) { return (uint8_t)std::round(factor * value);  });
        }

        float_t houseHeight = 0.0;
        std::array<float, 3> housePos = { 0, 0, 0 };
        for (auto &object : wrp.objects)
        {
            if (object.object_id == mapInfo.object_id)
            {

                /*if (wrp.objects.find(mapInfo4Ptr->objectId) != wrp.objectIdMap.end()) {
                    auto& object = wrp.objectIdMap.at(mapInfo4Ptr->objectId);*/

                housePos[0] = object.transform_matrx._3.x;// [3] [0] ;
                housePos[1] = object.transform_matrx._3.z;// [3] [2] ;
                housePos[2] = object.transform_matrx._3.y;// [3] [1] ;

                if (object.model_index < wrp.models.size()) {
                    auto& objectModel = modelInfos[object.model_index];

                    houseHeight = objectModel.b_max.y/*[1]*/ - objectModel.b_min.y/*[1]*/;
                }
                break;
            }
        }

        mapFeature["properties"] = { { "color", color }, { "height", houseHeight }, { "position", housePos } };
        house.push_back(mapFeature);
        //}
    }
    writeGZJson("house.geojson.gz", basePathGeojson, house);
}

void writeObjects(rvff::cxx::OprwCxx& wrp, std::filesystem::path& basePathGeojson)
{
    //nl::json house = nl::json::array();
    //for (auto& mapInfo : wrp.map_infos_4) {
    //    //if (mapInfo->mapType == 4) {
    //        //auto mapInfo4Ptr = std::static_pointer_cast<MapType4>(mapInfo);


    //        nl::json mapFeature;
    //        mapFeature["type"] = "Feature";

    //        auto posArray = nl::json::array();
    //        posArray.push_back(mapInfo.bounds[0]);
    //        posArray.push_back(mapInfo->bounds[1]);

    //        mapFeature["geometry"] = { { "type" , "Point" }, { "coordinates" , posArray } };
    //        mapFeature["properties"] = { { "text", std::to_string(mapInfo->infoType)} };

    //        house.push_back(mapFeature);
    //    //}
    //}
    //writeGZJson("debug.geojson.gz", basePathGeojson, house);
}

void writeRoads(
    rvff::cxx::OprwCxx& wrp,
    const std::string& worldName,
    std::filesystem::path& basePathGeojson,
    const std::map<std::string, std::vector<std::pair<rvff::cxx::ObjectCxx, rvff::cxx::LodCxx&>>>& mapObjects) {

    auto basePathGeojsonTemp = basePathGeojson / "temp";

    try {
        client::invoker_lock threadLock;
        auto roadsPath = sqf::get_text(sqf::config_entry(sqf::config_file()) >> "CfgWorlds" >> worldName >> "newRoadsShape");
        threadLock.unlock();

        // some maps have no roads (e.g. desert)
        if (roadsPath.empty()) {
            return;
        }

        if (!fs::exists(basePathGeojsonTemp)) {
            fs::create_directories(basePathGeojsonTemp);
        }

        auto basePathGeojsonRoads = basePathGeojson / "roads";
        if (!fs::exists(basePathGeojsonRoads)) {
            fs::create_directories(basePathGeojsonRoads);
        }

        auto roads_pbo = rvff::cxx::create_pbo_reader_path(findPboPath(roadsPath).string());

        auto roadsPathDir = ((fs::path)roadsPath).remove_filename().string();
        if (boost::starts_with(roadsPathDir, "\\")) {
            roadsPathDir = roadsPathDir.substr(1);
        }

        auto prefix = roads_pbo->get_prefix();

        for (auto& entry : roads_pbo->get_pbo().entries) {
            if (boost::istarts_with((((fs::path)static_cast<std::string>(prefix)) / static_cast<std::string>(entry.filename)).string(), roadsPathDir)) {
                roads_pbo->extract_single_file(static_cast<std::string>(entry.filename), basePathGeojsonTemp.string(), false);
                //roads_pbo->extract_single_file(key, basePathGeojsonTemp);
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

        char* options[] = { PSTR("-f"), PSTR("GeoJSON"), nullptr };

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

        auto road_config = rvff::cxx::create_cfg_path((basePathGeojsonTemp / "roadslib.cfg").string());

        std::map<uint32_t, std::pair<float_t, std::string>> roadWidthMap = {};

        auto entries = road_config->get_entry_as_entries(std::vector < std::string> { "RoadTypesLibrary" });
        for (auto& entry : entries)
        {
            try {
                auto _class = entry.get_entry_as_class();
                auto class_name = static_cast<std::string>(_class->get_class_name());
                auto map = static_cast<std::string>(_class->get_entry_as_string(std::vector < std::string> { "map" }));
                auto width = _class->get_entry_as_number(std::vector < std::string> { "width" });

                auto map_fixed = boost::replace_all_copy(map, " ", "_");
                auto class_name_fixed = std::stoi(class_name.substr(4));

                std::pair<float_t, std::string> roadPair = {};
                roadWidthMap.insert({ class_name_fixed, std::make_pair(width, map_fixed) });

                spdlog::info(static_cast<std::string>(map));
                spdlog::info(width);
            }
            catch (const rust::Error& ex) {
                spdlog::error(ex.what());
            }
        }

        // calc additional roads
        std::vector<std::string> roadTypes = { "road", "main road", "track" };
        auto additionalRoads = std::map<std::string, std::vector<std::array<SimplePoint, 4>>>{};
        for (auto& roadType : roadTypes) {
            if (mapObjects.find(roadType) != mapObjects.end()) {
                for (auto& road : mapObjects.at(roadType)) {
                    auto correctedRoadType = roadType;
                    if (roadType == "mainroad" || roadType == "main road") {
                        // Blame Zade when this blows up
                        correctedRoadType = "main_road";
                    }

                    auto r11 = road.first.transform_matrx._0.x;// [0][0];
                    auto r13 = road.first.transform_matrx._0.z;// [0][2];

                    auto r34 = 0;
                    auto r31 = road.first.transform_matrx._2.x;// [2][0];
                    auto r32 = road.first.transform_matrx._2.y;// [2][1];
                    auto r33 = road.first.transform_matrx._2.z;// [2][2];

                    // calculate rotation in RADIANS
                    auto theta = std::atan2(-1 * r31, std::sqrt(std::pow(r32, 2) + std::pow(r33, 2)));

                    if (r11 == 1.0f || r11 == -1.0f) {
                        theta = std::atan2(r13, r34);
                    }
                    else {
                        theta = std::atan2(-1 * r31, r11);
                    }

                    // Corrected center pos
                    auto centerCorX = road.first.transform_matrx._3.x/*[3][0]*/ - road.second.b_center.x;// [0] ;
                    auto centerCorY = road.first.transform_matrx._3.z/*[3][2]*/ - road.second.b_center.z;// [2] ;

                    // calc rotated pos of corner 
                    // https://gamedev.stackexchange.com/a/86780

                    /*
                        (x_1,y_1) --- (x_2,y_2)
                            |             |
                            |             |
                        (x_0,y_0) --- (x_3,y_3)
                    */

                    rvff::cxx::XYZTripletCxx lb = {};
                    rvff::cxx::XYZTripletCxx le = {};
                    rvff::cxx::XYZTripletCxx pb = {};
                    rvff::cxx::XYZTripletCxx pe = {};
                    std::optional<rvff::cxx::XYZTripletCxx> map3Triplet = {};
                    for (auto& namedSelection : road.second.named_selection) {
                        spdlog::info(fmt::format("NamedName: {}", static_cast<std::string>(namedSelection.name)));
                        if (static_cast<std::string>(namedSelection.name) == "lb") {
                            lb = road.second.vertices[namedSelection.selected_vertices.edges[0]];
                        }
                        if (static_cast<std::string>(namedSelection.name) == "le") {
                            le = road.second.vertices[namedSelection.selected_vertices.edges[0]];
                        }
                        if (static_cast<std::string>(namedSelection.name) == "pb") {
                            pb = road.second.vertices[namedSelection.selected_vertices.edges[0]];
                        }
                        if (static_cast<std::string>(namedSelection.name) == "pe") {
                            pe = road.second.vertices[namedSelection.selected_vertices.edges[0]];
                        }
                    }

                    auto x0 = centerCorX + (lb.x/*[0]*/ * std::cos(theta)) - (lb.z/*[2]*/ * std::sin(theta));
                    auto y0 = centerCorY + (lb.x/*[0]*/ * std::sin(theta)) + (lb.z/*[2]*/ * std::cos(theta));

                    auto x1 = centerCorX + (le.x/*[0]*/ * std::cos(theta)) - (le.z/*[2]*/ * std::sin(theta));
                    auto y1 = centerCorY + (le.x/*[0]*/ * std::sin(theta)) + (le.z/*[2]*/ * std::cos(theta));

                    auto x2 = centerCorX + (pe.x/*[0]*/ * std::cos(theta)) - (pe.z/*[2]*/ * std::sin(theta));
                    auto y2 = centerCorY + (pe.x/*[0]*/ * std::sin(theta)) + (pe.z/*[2]*/ * std::cos(theta));

                    auto x3 = centerCorX + (pb.x/*[0]*/ * std::cos(theta)) - (pb.z/*[2]*/ * std::sin(theta));
                    auto y3 = centerCorY + (pb.x/*[0]*/ * std::sin(theta)) + (pb.z/*[2]*/ * std::cos(theta));

                    /*
                    auto x0 = centerCorX + (road.second.bMin[0] * std::cos(theta)) - (road.second.bMin[2] * std::sin(theta));
                    auto y0 = centerCorY + (road.second.bMin[0] * std::sin(theta)) + (road.second.bMin[2] * std::cos(theta));

                    auto x1 = centerCorX + (road.second.bMin[0] * std::cos(theta)) - (road.second.bMax[2] * std::sin(theta));
                    auto y1 = centerCorY + (road.second.bMin[0] * std::sin(theta)) + (road.second.bMax[2] * std::cos(theta));

                    auto x2 = centerCorX + (road.second.bMax[0] * std::cos(theta)) - (road.second.bMax[2] * std::sin(theta));
                    auto y2 = centerCorY + (road.second.bMax[0] * std::sin(theta)) + (road.second.bMax[2] * std::cos(theta));

                    auto x3 = centerCorX + (road.second.bMax[0] * std::cos(theta)) - (road.second.bMin[2] * std::sin(theta));
                    auto y3 = centerCorY + (road.second.bMax[0] * std::sin(theta)) + (road.second.bMin[2] * std::cos(theta));
                    */

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
            feature["properties"]["width"] = roadWidthMap[jId].first;

            //auto ogrGeometry = OGRGeometryFactory::createFromGeoJson(feature["geometry"].dump().c_str());
            //ogrGeometry = ogrGeometry->Buffer(roadWidthMap[jId].first * GRAD_MEH_ROAD_WITH_FACTOR);

            //auto ret = ogrGeometry->exportToJson();
            //feature["geometry"] = nl::json::parse(ret);
            //CPLFree(ret);
            //OGRGeometryFactory::destroyGeometry(ogrGeometry);

            auto kvp = roadMap.find(roadWidthMap[jId].second);
            if (kvp == roadMap.end()) {
                kvp = roadMap.insert({ roadWidthMap[jId].second, nl::json() }).first;
            }
            kvp->second.push_back(feature);
        }

        // separate roads into main_road, track, etc.
        for (auto& [key, value] : roadMap) {
            writeGZJson((key + std::string(".geojson.gz")), basePathGeojsonRoads, value);
        }

        for (auto& [key, value] : additionalRoads) {
            nl::json bridge;

            for (auto& rectangles : value) {
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

                normalizePolygon(geometry["coordinates"][0]);

                addtionalRoadJson["geometry"] = geometry;
                addtionalRoadJson["properties"] = nl::json::object();
                addtionalRoadJson["type"] = "Feature";

                bridge.push_back(addtionalRoadJson);
            }

            writeGZJson((key + std::string("-bridge.geojson.gz")), basePathGeojsonRoads, bridge);
        }

        /*
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
        */
    }
    catch (const rust::Error& ex) {
        spdlog::error(fmt::format("Exception in writeRoads"));
        log_error(ex);
        throw ex;
    }

    // Remove temp dir
    fs::remove_all(basePathGeojsonTemp);
}

void writeSpecialIcons(rvff::cxx::OprwCxx& wrp, fs::path& basePathGeojson, uint32_t id, const std::string& name) {

    auto treeLocations = nl::json();
    for (auto& mapInfo : wrp.map_infos_1) {
        if (/*mapInfo->mapType == 1 && */mapInfo.type_id == id) {
            //auto mapInfo1Ptr = std::static_pointer_cast<MapType1>(mapInfo);

            auto pointFeature = nl::json();
            pointFeature["type"] = "Feature";

            auto geometry = nl::json();
            geometry["type"] = "Point";

            auto posArray = nl::json::array();
            posArray.push_back((float_t)mapInfo.x);
            posArray.push_back((float_t)mapInfo.y);
            geometry["coordinates"] = posArray;

            pointFeature["geometry"] = geometry;
            pointFeature["properties"] = nl::json::object();

            treeLocations.push_back(pointFeature);
        }
    }
    if (!treeLocations.is_null() && treeLocations.is_array() && !treeLocations.empty()) {
        writeGZJson(name + ".geojson.gz", basePathGeojson, treeLocations);
    }
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

float_t calculateMaxDistance(types::auto_array<types::game_value> ilsPos, types::auto_array<types::game_value> ilsDirection, 
    types::auto_array<types::game_value> taxiIn, types::auto_array<types::game_value> taxiOff) {

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

nl::json buildRunwayPolygon(sqf::config_entry &runwayConfig) {
    auto ilsPos = sqf::get_array(runwayConfig >> "ilsPosition").to_array();
    auto ilsDirection = sqf::get_array(runwayConfig >> "ilsDirection").to_array();
    auto taxiIn = sqf::get_array(runwayConfig >> "ilsTaxiIn").to_array();
    auto taxiOff = sqf::get_array(runwayConfig >> "ilsTaxiOff").to_array();

    if (taxiIn.empty() && taxiOff.empty()) {
        return nl::json();
    }

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
    coordArr.push_back({ startPos[0] + px, startPos[1] + py });

    normalizePolygon(coordArr);

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
    if (sqf::get_array(runwayConfig >> "ilsPosition").to_array().size() > 0 &&
        (float_t)sqf::get_array(runwayConfig >> "ilsPosition").to_array()[0] != 0) {
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

    if (std::all_of(runways.begin(), runways.end(), [](nl::json j) { return j.empty(); })) {
        return;
    }

    writeGZJson("runway.geojson.gz", basePathGeojson, runways);
}

void writeGenericMapTypes(fs::path& basePathGeojson, const std::vector<std::pair<rvff::cxx::ObjectCxx, rvff::cxx::LodCxx&>>& objectPairs, const std::string& name) {
    if (objectPairs.size() == 0)
        return;

    auto mapTypeLocation = nl::json();
    for (auto& pair : objectPairs) {
        auto pointFeature = nl::json();
        pointFeature["type"] = "Feature";

        auto geometry = nl::json();
        geometry["type"] = "Point";

        auto posArray = nl::json::array();
        posArray.push_back(pair.first.transform_matrx._3.x/*[3][0]*/);
        posArray.push_back(pair.first.transform_matrx._3.z/*[3][2]*/);
        geometry["coordinates"] = posArray;

        pointFeature["geometry"] = geometry;
        pointFeature["properties"] = nl::json::object();

        mapTypeLocation.push_back(pointFeature);
    }
    writeGZJson((name + ".geojson.gz"), basePathGeojson, mapTypeLocation);
}

void writePowerlines(rvff::cxx::OprwCxx& wrp, fs::path& basePathGeojson) {
    nl::json powerline = nl::json::array();

    for (auto& mapInfo : wrp.map_infos_5) {
        //if (mapInfo->mapType == 5) {
            //auto mapInfo5Ptr = std::static_pointer_cast<MapType5>(mapInfo);
        nl::json mapFeature;
        mapFeature["type"] = "Feature";

        auto coordArr = nl::json::array();
        coordArr.push_back({ mapInfo.floats[0], mapInfo.floats[1] });
        coordArr.push_back({ mapInfo.floats[2], mapInfo.floats[3] });

        mapFeature["geometry"] = { { "type" , "LineString" }, { "coordinates" , coordArr } };
        mapFeature["properties"] = nl::json::object();

        powerline.push_back(mapFeature);
        //}
    }
    if (!powerline.empty())
        writeGZJson("powerline.geojson.gz", basePathGeojson, powerline);
}

void writeRailways(fs::path& basePathGeojson, const std::vector<std::pair<rvff::cxx::ObjectCxx, rvff::cxx::LodCxx&>>& objectPairs) {
    if (objectPairs.size() == 0)
        return;

    nl::json railways = nl::json::array();

    for (auto& objectPair : objectPairs) {
        rvff::cxx::XYZTripletCxx map1Triplet;
        rvff::cxx::XYZTripletCxx map2Triplet;
        std::optional<rvff::cxx::XYZTripletCxx> map3Triplet;
        for (auto& namedSelection : objectPair.second.named_selection) {
            if (namedSelection.name == "map1") {
                map1Triplet = objectPair.second.normals[namedSelection.vertex_indices[0]];
            }
            if (namedSelection.name == "map2") {
                map2Triplet = objectPair.second.normals[namedSelection.vertex_indices[0]];
            }
            if (namedSelection.name == "map3") {
                map3Triplet = objectPair.second.normals[namedSelection.vertex_indices[0]];
            }
        }

        auto xPos = objectPair.first.transform_matrx._3.x;// [3] [0] ;
        auto yPos = objectPair.first.transform_matrx._3.z;// [3] [2] ;

        auto r11 = objectPair.first.transform_matrx._0.x;// [0] [0] ;
        auto r13 = objectPair.first.transform_matrx._0.z;// [0] [2] ;

        auto r34 = 0;
        auto r31 = objectPair.first.transform_matrx._2.x;// [2] [0] ;
        auto r32 = objectPair.first.transform_matrx._2.y;// [2] [1] ;
        auto r33 = objectPair.first.transform_matrx._2.z;// [2] [2] ;

        // calculate rotation in RADIANS
        auto theta = std::atan2(-1 * r31, std::sqrt(std::pow(r32, 2) + std::pow(r33, 2)));

        if (r11 == 1.0f || r11 == -1.0f) {
            theta = std::atan2(r13, r34);
        }
        else {
            theta = std::atan2(-1 * r31, r11);
        }

        auto startPosX = xPos + (map1Triplet.x/*[0]*/ * std::cos(theta)) - (map1Triplet.z/*[2]*/ * std::sin(theta));
        auto startPosY = yPos + (map1Triplet.x/*[0]*/ * std::sin(theta)) + (map1Triplet.z/*[2]*/ * std::cos(theta));

        auto endPosX = xPos + (map2Triplet.x/*[0]*/ * std::cos(theta)) - (map2Triplet.z/*[2]*/ * std::sin(theta));
        auto endPosY = yPos + (map2Triplet.x/*[0]*/ * std::sin(theta)) + (map2Triplet.z/*[2]*/ * std::cos(theta));

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
            auto switchStartPosX = xPos + ((*map3Triplet).x/*[0]*/ * std::cos(theta)) - ((*map3Triplet).z/*[2]*/ * std::sin(theta));
            auto switchStartPosY = yPos + ((*map3Triplet).x/*[0]*/ * std::sin(theta)) + ((*map3Triplet).z/*[2]*/ * std::cos(theta));

            coordArr.push_back({ switchStartPosX, switchStartPosY });
            coordArr.push_back({ endPosX, endPosY });

            railwaySwitchFeature["geometry"] = { { "type" , "LineString" }, { "coordinates" , coordArr } };
            railwaySwitchFeature["properties"] = nl::json::object();
            railways.push_back(railwaySwitchFeature);
        }

    }
    writeGZJson("railway.geojson.gz", basePathGeojson, railways);
}

void writeGeojsons(rvff::cxx::OprwCxx& wrp, std::filesystem::path& basePathGeojson, const std::string& worldName)
{
    using namespace rvff::cxx;

    std::vector<LodCxx> modelInfos;
    modelInfos.resize(wrp.models.size());

    std::vector<std::pair<std::string, LodCxx>> modelMapTypes;
    modelMapTypes.resize(wrp.models.size());


    std::map<fs::path, rust::Box<rvff::cxx::PboReaderCxx>> pboMap;
    for (int i = 0; i < wrp.models.size(); i++) {
        auto modelPath = static_cast<std::string>(wrp.models[i]);
        if (boost::starts_with(modelPath, "\\")) {
            modelPath = modelPath.substr(1);
        }

        auto pboPath = findPboPath(modelPath);

        if (pboPath.empty()) {
            spdlog::warn(fmt::format("Couldn't find path for model: {}", modelPath));
            modelMapTypes[i] = {};
            modelInfos[i] = {};
            break;
        }

        //pboMap.contains()
        /*auto res = pboMap.find(pboPath);
        if (res == pboMap.end()) {*/
        if (pboMap.count(pboPath) < 1) {
            pboMap.emplace(pboPath, std::move(rvff::cxx::create_pbo_reader_path(pboPath.string())));
        }

        int rvffIndex = -1;
        int affIndex = -1;

        auto p3dData = pboMap.find(pboPath)->second->get_entry_data(modelPath);

        if (p3dData.empty())
        {
            spdlog::warn(fmt::format("Couldn't get data for model: {} (PBO: {})", modelPath, pboPath.string()));
            modelMapTypes[i] = {};
            modelInfos[i] = {};
            break;
        }
        try {
            spdlog::info(fmt::format("Reading P3D: {} (PBO: {})", modelPath, pboPath.string()));
            auto odol_reader = rvff::cxx::create_odol_lazy_reader_vec(p3dData);
            auto odol2 = odol_reader->get_odol();

            rvff::cxx::LodCxx lod = {};

            bool foundGeoLod = false;
            bool foundSpecialLod = false;
            for (auto& res : odol2.resolutions)
            {
                if (res.res == rvff::cxx::ResolutionEnumCxx::Geometry) {
                    foundGeoLod = true;
                }
                else if (res.res == rvff::cxx::ResolutionEnumCxx::GraphicalLod) {
                    foundSpecialLod = true;
                }
                if (foundGeoLod && foundSpecialLod) {
                    break;
                }
            }

            if (foundGeoLod)
            {
                lod = odol_reader->read_lod(rvff::cxx::ResolutionEnumCxx::Geometry);
                rvffIndex = 1;

                for (auto& prop : lod.named_properties) {
                    if (static_cast<std::string>(prop.property) == "map")
                    {
                        auto val = static_cast<std::string>(prop.value);
                        if ((val == "railway" || val == "road" || val == "track" || val == "main road") && foundSpecialLod) {
                            lod = odol_reader->read_lod(rvff::cxx::ResolutionEnumCxx::Memory);
                            rvffIndex = 2;
                        }

                        modelMapTypes[i] = { static_cast<std::string>(val), lod };
                        modelInfos[i] = lod;
                        break;
                    }
                }


            }
        }
        catch (const rust::Error& ex) {
            spdlog::error(fmt::format("Exception while reading P3D: {} (PBO: {})", modelPath, pboPath.string()));
            log_error(ex);
            throw ex;
        }

        //auto odol = grad_aff::Odol(p3dData);
        //odol.readOdol(false);

        //odol.peekLodTypes();

        //auto geoIndex = -1;
        //for (int j = 0; j < odol.lods.size(); j++) {
        //    if (odol.lods[j].lodType == LodType::GEOMETRY) {
        //        geoIndex = j;
        //    }
        //}

        //if (geoIndex != -1) {
        //    auto retLod = odol.readLod(geoIndex);
        //    affIndex = 1;

        //    auto findClass = retLod.tokens.find("map");
        //    if (findClass != retLod.tokens.end()) {
        //        if (findClass->second == "railway" || findClass->second == "road" || findClass->second == "track" || findClass->second == "main road") {
        //            geoIndex = -1;
        //            for (int j = 0; j < odol.lods.size(); j++) {
        //                if (odol.lods[j].lodType == LodType::SPECIAL_LOD) {
        //                    geoIndex = j;
        //                    affIndex = 1;
        //                    break;
        //                }
        //                if (geoIndex == -1 && odol.lods[j].lodType == LodType::GEOMETRY) { // Fallback
        //                    geoIndex = j; 
        //                    affIndex = 2;
        //                }
        //            }
        //            auto memLod = odol.readLod(geoIndex);
        //            modelMapTypes[i] = { findClass->second, memLod };
        //            modelInfos[i] = memLod;
        //        }
        //        else {
        //            modelMapTypes[i] = { findClass->second, retLod };
        //            modelInfos[i] = retLod;
        //        }
        //    }
        //}

        /*if (rvffIndex != affIndex)
        {
            spdlog::error(fmt::format("Wrong Index (rvff: {}, aff: {}) while reading P3D: {} (PBO: {})", rvffIndex, affIndex,  modelPath, pboPath.string()));
        }*/

    }

    // mapType, [object, lod]
    std::map<std::string, std::vector<std::pair<rvff::cxx::ObjectCxx, LodCxx&>>> objectMap = {};
    /*
    std::vector<Point_> forestPositions = {};
    std::vector<Point_> forestSquarePositions = {};
    std::vector<Point_> forestTrianglePositions = {};
    std::vector<Point_> forestBroderPositions = {};

    std::vector<Point_> forestFeulStatPositions = {};
    std::vector<Point_> forestBroderPositions = {};
    */
    for (auto& object : wrp.objects) {
        auto mapType = modelMapTypes[object.model_index].first;
        auto res = objectMap.find(mapType);
        if (res != objectMap.end()) {
            res->second.push_back({ object, modelMapTypes[object.model_index].second });
        }
        else {
            objectMap.insert({ mapType, {{object, modelMapTypes[object.model_index].second}} });
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

    std::vector<std::string> genericMapTypes = { "bunker", "chapel", "church", "cross", "fuelstation", "lighthouse", "rock", "shipwreck", "transmitter", "tree", "rock", "watertower",
                                                "fortress", "fountain", "view-tower", "quay", "hospital", "busstop", "stack", "ruin", "tourism", "powersolar", "powerwave", "powerwind" };

    for (auto& genericMapType : genericMapTypes) {
        if (objectMap.find(genericMapType) != objectMap.end()) {
            if (genericMapType == "tree") {
                writeArea(wrp, basePathGeojson, objectMap["tree"], 15, 5, 10, 7, "forest");
            }
            else if (genericMapType == "rock") {
                writeArea(wrp, basePathGeojson, objectMap["rock"], 10, 4, 10, 7, "rocks");
            }
            else {
                writeGenericMapTypes(basePathGeojson, objectMap[genericMapType], genericMapType);
            }
        }
    }

    if (objectMap.find("railway") != objectMap.end())
        writeRailways(basePathGeojson, objectMap["railway"]);

    writeHouses(wrp, basePathGeojson, modelInfos);
    //writeObjects(wrp, basePathGeojson);
    writeLocations(worldName, basePathGeojson);
    writeRoads(wrp, worldName, basePathGeojson, objectMap);
    writeSpecialIcons(wrp, basePathGeojson, 0, "tree");
    writeSpecialIcons(wrp, basePathGeojson, 2, "bush");
    writeSpecialIcons(wrp, basePathGeojson, 11, "rock");
    writePowerlines(wrp, basePathGeojson);

    writeRunways(basePathGeojson, worldName);

}
