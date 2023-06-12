#include "satimages.h"

#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>

using namespace OpenImageIO_v2_4;

// Range TODO: replace with C++20 Range
#include <boost/range/counting_range.hpp>

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

float_t compareRows(rust::slice<uint8_t>::iterator mipmap1It, rust::slice<uint8_t>::iterator mipmap2It, uint32_t width) {
    std::vector<float_t> equalities = {};
    for (size_t i = 0; i < width; i++) {
        equalities.push_back(compareColors(*mipmap1It, *(mipmap1It + 1), *(mipmap1It + 2), *mipmap2It, *(mipmap2It + 1), *(mipmap2It + 2)));
        mipmap1It += 4;
        mipmap2It += 4;
    }

    return mean(equalities);
}

uint32_t calcOverLap(rvff::cxx::MipmapCxx& mipmap1, rvff::cxx::MipmapCxx& mipmap2) {
    std::vector<std::pair<float_t, size_t>> bestMatchingOverlaps = {};

    //auto filter = mipmap1.height > 512 ? 6 : 3;
    auto filter = 3;

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

void writeSatImages(rvff::cxx::OprwCxx& wrp, const int32_t& worldSize, std::filesystem::path& basePathSat, const std::string& worldName)
{
        std::vector<std::string> rvmats = {};
        for (auto& rv : wrp.texures) {
            if (!rv.texture_filename.empty()) {
                rvmats.push_back(static_cast<std::string>(rv.texture_filename));
            }
        }

        std::sort(rvmats.begin(), rvmats.end());

        if (rvmats.size() > 1) {
            std::vector<std::string> lcoPaths = {};
            lcoPaths.reserve(rvmats.size());

            std::string pboPath = "";
            try {
                pboPath = findPboPath(rvmats[0]).string();
                auto rvmatPbo = rvff::cxx::create_pbo_reader_path(pboPath);

                // Has to be not empty
                std::string lastValidRvMat = "1337";
                std::string fillerTile = "";
                std::string prefix = "s_";

                uint32_t maxX2 = 0;
                uint32_t maxY2 = 0;

                std::vector<std::string> texture_config_path{ "Stage0", "texture" };

                // TODO: refactor this when implementing the new rap api
                for (auto& rvmatPath : rvmats) {

                    std::vector<std::string> parts;
                    boost::split(parts, ((fs::path)rvmatPath).filename().string(), boost::is_any_of("_"));

                    if (parts.size() > 1) {
                        std::vector<std::string> numbers;
                        boost::split(numbers, parts[1], boost::is_any_of("-"));

                        if (numbers.size() > 1) {
                            auto x = std::stoi(numbers[0]);
                            auto y = std::stoi(numbers[1]);

                            if (x > maxX2) {
                                maxX2 = x;
                            }

                            if (y > maxY2) {
                                maxY2 = y;
                            }
                        }
                    }

                    if (!boost::istarts_with(((fs::path)rvmatPath).filename().string(), lastValidRvMat)) {
                        PLOG_INFO << fmt::format("Getting data for {}", rvmatPath);
                        auto rap_data = rvmatPbo->get_entry_data(rvmatPath);
                        PLOG_INFO << fmt::format("Parsing rvmat {}", rvmatPath);
                        auto rap = rvff::cxx::create_cfg_vec(rap_data);
                        auto textureStr = static_cast<std::string>(rap->get_entry_as_string(texture_config_path));
                        if (!textureStr.empty()) {
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
                        }
                    }
                }

                // Remove Duplicates
                std::sort(lcoPaths.begin(), lcoPaths.end());
                auto last = std::unique(lcoPaths.begin(), lcoPaths.end());
                lcoPaths.erase(last, lcoPaths.end());

                // Fix wrong file extensions (cup summer has png extension)
                for (auto& lcoPath : lcoPaths) {
                    fs::path lcoPathAsPath = (fs::path)lcoPath;
                    if (!boost::iequals(lcoPathAsPath.extension().string(), ".paa")) {
                        lcoPath = lcoPathAsPath.replace_extension(".paa").string();
                    }
                }

                // "strechted border" overlap is og overlap/2

                // find highest x/y
                std::vector<std::string> splitResult = {};
                boost::split(splitResult, ((fs::path)lcoPaths[lcoPaths.size() - 1]).filename().string(), boost::is_any_of("_"));
                uint32_t maxX = maxX2;
                uint32_t maxY = maxY2;

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

                rvff::cxx::MipmapCxx upperMipmap;
                rvff::cxx::MipmapCxx lowerMipmap;

                for (auto& lcoPath : lcoPaths) {
                    if (boost::iends_with(lcoPath, upperPaaPath)) {
                        auto upperPbo = rvff::cxx::create_pbo_reader_path(findPboPath(lcoPath).string());
                        auto upperData = upperPbo->get_entry_data(lcoPath);
                        upperMipmap = rvff::cxx::get_mipmap_from_paa_vec(upperData, 0);
                    }
                    if (boost::iends_with(lcoPath, lowerPaaPath)) {
                        auto lowerPbo = rvff::cxx::create_pbo_reader_path(findPboPath(lcoPath).string());
                        auto lowerData = lowerPbo->get_entry_data(lcoPath);
                        lowerMipmap = rvff::cxx::get_mipmap_from_paa_vec(lowerData, 0);
                    }
                }

                auto overlap = calcOverLap(upperMipmap, lowerMipmap);

                ImageBuf dst(ImageSpec(upperMipmap.width + maxX * (upperMipmap.width - overlap), upperMipmap.height + maxY * (upperMipmap.height - overlap), 4, TypeDesc::UINT8));

                if (fillerTile != "") {
                    auto fillerPbo = rvff::cxx::create_pbo_reader_path(findPboPath(fillerTile).string());
                    auto fillerData = fillerPbo->get_entry_data(fillerTile);
                    auto filler_mm = rvff::cxx::get_mipmap_from_paa_vec(fillerData, 0);

                    ImageBuf src(ImageSpec(filler_mm.height, filler_mm.height, 4, TypeDesc::UINT8), filler_mm.data.data());

                    size_t noOfTiles = (worldSize / filler_mm.height);

                    auto cr = boost::counting_range(size_t(0), noOfTiles);
                    std::for_each(std::execution::par_unseq, cr.begin(), cr.end(), [noOfTiles, &dst, filler_mm, src](size_t i) {
                        for (size_t j = 0; j < noOfTiles; j++) {
                            ImageBufAlgo::paste(dst, (i * filler_mm.height), (j * filler_mm.height), 0, 0, src);
                        }
                    });
                }

                auto pbo = rvff::cxx::create_pbo_reader_path(findPboPath(lcoPaths[0]).string());

                for (auto& lcoPath : lcoPaths) {
                    auto data = pbo->get_entry_data(lcoPath);

                    if (data.empty()) {
                        pbo = rvff::cxx::create_pbo_reader_path(findPboPath(lcoPath).string());
                        data = pbo->get_entry_data(lcoPath);
                    }

                    auto lco_mm = rvff::cxx::get_mipmap_from_paa_vec(data, 0);

                    std::vector<std::string> splitResult = {};
                    boost::split(splitResult, ((fs::path)lcoPath).filename().string(), boost::is_any_of("_"));

                    ImageBuf src(ImageSpec(lco_mm.width, lco_mm.height, 4, TypeDesc::UINT8), lco_mm.data.data());

                    auto width = lco_mm.width;
                    auto height = lco_mm.height;

                    if (lco_mm.width == 4 && lco_mm.height == 4) {
                        src = ImageBufAlgo::resize(src, "", 0, ROI(0, upperMipmap.width, 0, upperMipmap.height));
                        width = upperMipmap.width;
                        height = upperMipmap.height;
                        PLOG_INFO << fmt::format("Old filler method detected, resizing 4x4 tiles to {}x{} !", upperMipmap.width, upperMipmap.height);
                    }

                    uint32_t x = std::stoi(splitResult[1]);
                    uint32_t y = std::stoi(splitResult[2]);

                    ImageBufAlgo::paste(dst, (x * width - x * overlap), (y * height - y * overlap), 0, 0, src);
                }

                // https://github.com/pennyworth12345/A3_MMSI/wiki/Mapframe-Information#stretching-at-edge-tiles

                auto tileWidth = upperMipmap.width / maxX;
                auto tileHeight = upperMipmap.height / maxY;

                maxX += 1;
                maxY += 1;

                int32_t initalOffset = overlap / 2; // always half of overlap

                auto hasEqualStrechting = (worldSize == (upperMipmap.width * maxX)) || (worldSize == ((upperMipmap.width - overlap) * maxX));

                auto finalWidth = ((upperMipmap.width - overlap) * maxX);
                auto finalHeight = ((upperMipmap.height - overlap) * maxY);

                // "It just works"
                if (!hasEqualStrechting) {
                    if (finalWidth <= worldSize) {
                        finalWidth -= overlap;
                    }
                    else {
                        finalWidth -= finalWidth % worldSize;;
                    }

                    if (finalHeight <= worldSize) {
                        finalHeight -= overlap;
                    }
                    else {
                        finalHeight -= finalHeight % worldSize;
                    }
                }

                //dst.write((basePathSat / "full_out.png").string());
                dst = ImageBufAlgo::cut(dst, ROI(initalOffset, initalOffset + finalWidth, initalOffset, initalOffset + finalHeight));

                size_t tileSize = dst.spec().width / 4;

                auto cr = boost::counting_range(0, 4);
                std::for_each(std::execution::par_unseq, cr.begin(), cr.end(), [basePathSat, dst, tileSize](int i) {
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
            catch (const rust::Error& ex) {
                PLOG_ERROR << fmt::format("Exception in writeSatImages PBO: {}", pboPath);
                PLOG_ERROR << ex.what();
                throw;
            }
        }
}