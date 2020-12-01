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

void writeHouses(grad_aff::Wrp& wrp, std::filesystem::path& basePathGeojson, std::vector<ODOLv4xLod> modelInfos)
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
            coordArr.push_back(std::vector<float_t> { mapInfo4Ptr->bounds[0], mapInfo4Ptr->bounds[1] });

            // make sure the winding order is counterclockwise
            // https://stackoverflow.com/a/1165943
            auto sum = 0.0f;
            for (auto i = 1; i < coordArr.size(); i++) {
                std::vector<float_t> p1 = coordArr.at(i - 1);
                std::vector<float_t> p2 = coordArr.at(i);
                sum += (p2[0] - p1[0]) * (p2[1] + p1[1]);
            }
            if (sum > 0.0f) {
                std::reverse(std::begin(coordArr), std::end(coordArr));
            }

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
            auto color = std::vector<uint8_t>{ mapInfo4Ptr->color[2], mapInfo4Ptr->color[1], mapInfo4Ptr->color[0] };

            auto maxColorValue = std::max_element(std::begin(color), std::end(color));

            // make sure every color-value is below or equal to 128
            if (*maxColorValue > 128) {
                float_t factor = 128.0f / *maxColorValue;

                std::transform(color.begin(), color.end(), color.begin(), [factor](uint8_t value) { return (uint8_t)std::round(factor * value);  });
            }

            float_t houseHeight = 0.0;
            XYZTriplet housePos = { 0, 0, 0 };
            if (wrp.objectIdMap.find(mapInfo4Ptr->objectId) != wrp.objectIdMap.end()) {
                auto &object = wrp.objectIdMap.at(mapInfo4Ptr->objectId);

                housePos[0] = object.transformMatrix[3][0];
                housePos[1] = object.transformMatrix[3][2];
                housePos[2] = object.transformMatrix[3][1];

                if (object.modelIndex < wrp.models.size()) {
                    auto &objectModel = modelInfos[object.modelIndex];

                    houseHeight = objectModel.bMax[1] - objectModel.bMin[1];
                }
            }

            mapFeature["properties"] = { { "color", color }, { "height", houseHeight }, { "position", housePos } };
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

    client::invoker_lock threadLock;
    auto roadsPath = sqf::get_text(sqf::config_entry(sqf::config_file()) >> "CfgWorlds" >> worldName >> "newRoadsShape");
    threadLock.unlock();

    // some maps have no roads (e.g. desert)
    if (roadsPath.empty()) {
        return;
    }

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

                auto r11 = road.first.transformMatrix[0][0];
                auto r13 = road.first.transformMatrix[0][2];

                auto r34 = 0;
                auto r31 = road.first.transformMatrix[2][0];
                auto r32 = road.first.transformMatrix[2][1];
                auto r33 = road.first.transformMatrix[2][2];

                // calculate rotation in RADIANS
                auto theta = std::atan2(-1 * r31, std::sqrt(std::pow(r32, 2) + std::pow(r33, 2)));

                if (r11 == 1.0f || r11 == -1.0f) {
                    theta = std::atan2(r13, r34);
                }
                else {
                    theta = std::atan2(-1 * r31, r11);
                }

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

                XYZTriplet lb;
                XYZTriplet le;
                XYZTriplet pb;
                XYZTriplet pe;
                std::optional<XYZTriplet> map3Triplet;
                for (auto& namedSelection : road.second.namedSelections) {
                    if (namedSelection.selectedName == "lb") {
                        lb = road.second.lodPoints[namedSelection.vertexTableIndexes[0]];
                    }
                    if (namedSelection.selectedName == "le") {
                        le = road.second.lodPoints[namedSelection.vertexTableIndexes[0]];
                    }
                    if (namedSelection.selectedName == "pb") {
                        pb = road.second.lodPoints[namedSelection.vertexTableIndexes[0]];
                    }
                    if (namedSelection.selectedName == "pe") {
                        pe = road.second.lodPoints[namedSelection.vertexTableIndexes[0]];
                    }
                }

                auto x0 = centerCorX + (lb[0] * std::cos(theta)) - (lb[2] * std::sin(theta));
                auto y0 = centerCorY + (lb[0] * std::sin(theta)) + (lb[2] * std::cos(theta));

                auto x1 = centerCorX + (le[0] * std::cos(theta)) - (le[2] * std::sin(theta));
                auto y1 = centerCorY + (le[0] * std::sin(theta)) + (le[2] * std::cos(theta));

                auto x2 = centerCorX + (pe[0] * std::cos(theta)) - (pe[2] * std::sin(theta));
                auto y2 = centerCorY + (pe[0] * std::sin(theta)) + (pe[2] * std::cos(theta));

                auto x3 = centerCorX + (pb[0] * std::cos(theta)) - (pb[2] * std::sin(theta));
                auto y3 = centerCorY + (pb[0] * std::sin(theta)) + (pb[2] * std::cos(theta));

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

    // Remove temp dir
    fs::remove_all(basePathGeojsonTemp);
}

void writeSpecialIcons(grad_aff::Wrp& wrp, fs::path& basePathGeojson, uint32_t id, const std::string& name) {

    auto treeLocations = nl::json();
    for (auto& mapInfo : wrp.mapInfo) {
        if (mapInfo->mapType == 1 && mapInfo->infoType == id) {
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
    writeGZJson(name + ".geojson.gz", basePathGeojson, treeLocations);
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

nl::json buildRunwayPolygon(sqf::config_entry& runwayConfig) {
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
    modelInfos.resize(wrp.models.size());

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
                if (findClass->second == "railway" || findClass->second == "road" || findClass->second == "track" || findClass->second == "main road") {
                    geoIndex = -1;
                    for (int j = 0; j < odol.lods.size(); j++) {
                        if (odol.lods[j].lodType == LodType::SPECIAL_LOD) {
                            geoIndex = j;
                        }
                    }
                    auto memLod = odol.readLod(geoIndex);
                    modelMapTypes[i] = { findClass->second, memLod };
                    modelInfos[i] = memLod;
                }
                else {
                    modelMapTypes[i] = { findClass->second, retLod };
                    modelInfos[i] = retLod;
                }
            }
        }
    }

    // mapType, [object, lod]
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
            res->second.push_back({ object, modelMapTypes[object.modelIndex].second });
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