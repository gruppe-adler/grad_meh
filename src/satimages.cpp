#include "satimages.h"

#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>

using namespace OpenImageIO_v2_4;

// Range TODO: replace with C++20 Range
#include <boost/range/counting_range.hpp>

#include <memory>

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
    return static_cast<float_t>(1 - (std::sqrt(std::pow(r2 - r1, 2) + std::pow(g2 - g1, 2) + std::pow(b2 - b1, 2)) / GRAD_MEH_MAX_COLOR_DIF));
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
    std::vector<std::pair<float_t, uint32_t>> bestMatchingOverlaps = {};

    //auto filter = mipmap1.height > 512 ? 6 : 3;
    uint32_t filter = 3;

    for (uint32_t overlap = 1; overlap < (mipmap1.height / filter); overlap++) {
        std::vector<float_t> equalities = {};

        for (uint32_t i = 0; i < overlap; i++) {
            auto beginRowUpper = mipmap1.data.begin() + (mipmap2.height - 1 - (overlap - i)) * static_cast<uint32_t>(mipmap1.width) * 4;
            auto beginRowLower = mipmap2.data.begin() + i * static_cast<uint32_t>(mipmap2.width) * 4;

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
                            uint32_t x = std::stoi(numbers[0]);
                            uint32_t y = std::stoi(numbers[1]);

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
                        if (rap_data.empty()) {
                            PLOG_ERROR << "Rvmat data was empty!";
                        }
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

                std::string upperPaaPath = fmt::format("{:03}_{:03}_lco.paa", x, y);

                std::string lowerPaaPath = fmt::format("{:03}_{:03}_lco.paa", x, y + 1);

                std::string lastPaaPath = fmt::format("{:03}_{:03}_lco.paa", maxX, maxY);

                std::optional<rvff::cxx::MipmapCxx> upperMipmapOpt = std::nullopt;
                std::optional<rvff::cxx::MipmapCxx> lowerMipmapOpt = std::nullopt;
                std::optional<rvff::cxx::MipmapCxx> lastMipmapOpt = std::nullopt;

                for (auto& lcoPath : lcoPaths) {
                    if (boost::iends_with(lcoPath, upperPaaPath)) {
                        auto upperPbo = rvff::cxx::create_pbo_reader_path(findPboPath(lcoPath).string());
                        auto upperData = upperPbo->get_entry_data(lcoPath);
                        upperMipmapOpt = rvff::cxx::get_mipmap_from_paa_vec(upperData, 0);
                    }
                    if (boost::iends_with(lcoPath, lowerPaaPath)) {
                        auto lowerPbo = rvff::cxx::create_pbo_reader_path(findPboPath(lcoPath).string());
                        auto lowerData = lowerPbo->get_entry_data(lcoPath);
                        lowerMipmapOpt = rvff::cxx::get_mipmap_from_paa_vec(lowerData, 0);
                    }
                    if (boost::iends_with(lcoPath, lastPaaPath)) {
                        auto lastPbo = rvff::cxx::create_pbo_reader_path(findPboPath(lcoPath).string());
                        auto lastData = lastPbo->get_entry_data(lcoPath);
                        lastMipmapOpt = rvff::cxx::get_mipmap_from_paa_vec(lastData, 0);
                    }
                }

                if (!upperMipmapOpt.has_value() || !lowerMipmapOpt.has_value()) {
                    PLOG_ERROR << "No mipmaps found for overlap calculation!";
                    throw new std::runtime_error("No mipmaps found for overlap calculation!");
                }
                
                rvff::cxx::MipmapCxx upperMipmap = *upperMipmapOpt;
                rvff::cxx::MipmapCxx lowerMipmap = *lowerMipmapOpt;

                auto overlap = calcOverLap(upperMipmap, lowerMipmap);

                ImageBuf dst(ImageSpec(upperMipmap.width + maxX * (upperMipmap.width - overlap), upperMipmap.height + maxY * (upperMipmap.height - overlap), 4, TypeDesc::UINT8));

                if (fillerTile != "") {
                    auto fillerPbo = rvff::cxx::create_pbo_reader_path(findPboPath(fillerTile).string());
                    auto fillerData = fillerPbo->get_entry_data(fillerTile);
                    auto filler_mm = rvff::cxx::get_mipmap_from_paa_vec(fillerData, 0);

                    ImageBuf src(ImageSpec(filler_mm.height, filler_mm.height, 4, TypeDesc::UINT8), filler_mm.data.data());

                    int32_t noOfTiles = (worldSize / filler_mm.height) + 1;

                    auto cr = boost::counting_range(int32_t(0), noOfTiles);
                    std::for_each(std::execution::par_unseq, cr.begin(), cr.end(), [noOfTiles, &dst, filler_mm, src](int32_t i) {
                        for (int32_t j = 0; j < noOfTiles; j++) {
                            ImageBufAlgo::paste(dst, (i * filler_mm.height), (j * filler_mm.height), 0, 0, src);
                        }
                    });
                }

                auto pbo = rvff::cxx::create_pbo_reader_path(findPboPath(lcoPaths[0]).string());

                bool oldFillerMethodPresent = false;

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
                        oldFillerMethodPresent = true;
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

                int32_t finalWidth = ((upperMipmap.width - overlap) * maxX);
                int32_t finalHeight = ((upperMipmap.height - overlap) * maxY);

                auto config_overlap = getConfigOverlap(worldName);

                ROI cutRoi;
                if (lastMipmapOpt.has_value() && !oldFillerMethodPresent) {

                    auto rbOverlap = config_overlap;

                    if (rbOverlap < 0) {
                        auto lastMipmap = *lastMipmapOpt;
                        ImageBuf lastPaaBuf(ImageSpec(lastMipmap.width, lastMipmap.height, 4, TypeDesc::UINT8), lastMipmap.data.data());
                        auto spec = lastPaaBuf.spec();

                        uint32_t maxHeight = spec.height / 2;

                        auto buf = std::vector<uint8_t>(static_cast<int64_t>(spec.width) * static_cast<int64_t>(spec.nchannels));

                        std::map<int64_t, int32_t> possibleOffsets = {};

                        for (uint32_t curHeight = 0; curHeight < maxHeight; curHeight++) {
                    
                            if (!lastPaaBuf.get_pixels(ROI(0, spec.width, curHeight, curHeight + 1), TypeDesc::UINT8, buf.data())) {
                                PLOG_ERROR << "Couldn't get last sat image tile pixels!";
                                throw new std::runtime_error("Couldn't get last sat image tile pixels!");
                            }

                            int64_t startIndex = spec.width * spec.nchannels * curHeight;
                            int64_t endIndex = spec.width * spec.nchannels * (curHeight + 1);

                            std::optional<std::tuple<uint8_t, uint8_t, uint8_t, uint8_t>> color = {};
                            int64_t found_offset = -1;
                            for (int64_t i = (buf.size() - 4); i > 0; i -= 4) {
                                auto cur_color = std::make_tuple(buf[i], buf[i + 1], buf[i + 2], buf[i + 3]);;
                                if (!color.has_value()) {
                                    color = cur_color;
                                }

                                if (color != cur_color) {
                                    int64_t arrayIndex = (i + 4) / 4;
                                    int64_t offset = arrayIndex;
                                    offset = std::min(offset, static_cast<int64_t>(spec.width));
                                    possibleOffsets[offset]++;
                                    break;
                                }
                            }
                        }

                        std::pair<int64_t, int64_t> maxOffset = *std::max_element
                        (
                            std::begin(possibleOffsets), std::end(possibleOffsets),
                            [](const std::pair<int64_t, int64_t>& p1, const std::pair<int64_t, int64_t>& p2) {
                                return p1.second < p2.second;
                            }
                        );

                        int32_t lastTileRealWidth = static_cast<int32_t>(maxOffset.first);
                        rbOverlap = spec.width - lastTileRealWidth;
                        PLOG_WARNING << fmt::format("Found last tile width: {}", lastTileRealWidth);
                    }
                    PLOG_WARNING << fmt::format("Found last tile end overlap: {}", rbOverlap);

                    finalWidth -= rbOverlap;
                    finalHeight -= rbOverlap;
                    auto dstSpec = dst.spec();
                    cutRoi = ROI(initalOffset, dstSpec.width - rbOverlap, initalOffset, dstSpec.height - rbOverlap);
                }
                else {
                    auto equalCheckSize1 = upperMipmap.width * maxX;
                    auto equalCheckSize2 = (upperMipmap.width - overlap) * maxX;

                    auto hasEqualStrechting = (worldSize == equalCheckSize1) || (worldSize == equalCheckSize2);

                    // "iT jUsT wÖrKs"
                    if (!hasEqualStrechting) {
                        if (finalWidth <= worldSize) {
                            if (worldSize % 2 == 0) {
                                finalWidth = worldSize / 2;
                            }
                            else {
                                finalWidth -= ((worldSize % finalWidth) % upperMipmap.width) / 2;
                                finalWidth += overlap / 2;
                            }
                        }
                        else {
                            finalWidth -= finalWidth % worldSize;
                        }

                        if (finalHeight <= worldSize) {
                            if (worldSize % 2 == 0) {
                                finalHeight = worldSize / 2;
                            }
                            else {
                                finalHeight -= ((worldSize % finalHeight) % upperMipmap.height) / 2;
                                finalHeight += overlap / 2;
                            }
                        }
                        else {
                            finalHeight -= finalHeight % worldSize;
                        }
                    }

                    cutRoi = ROI(initalOffset, initalOffset + finalWidth, initalOffset, initalOffset + finalHeight);
                }

                #ifdef _DEBUG 
                float red[4] = { 1, 0, 0, 1 };
                auto copy = dst.copy(TypeDesc::FLOAT);
                ImageBufAlgo::render_box(copy, cutRoi.xbegin, cutRoi.ybegin, cutRoi.xend, cutRoi.yend, red);
                copy.write((basePathSat / "debug_full_streched.exr").string());
                #endif

                dst = ImageBufAlgo::cut(dst, cutRoi);

                int32_t tileSize = dst.spec().width / 4;

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