#include "satimages.h"

#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>

using namespace OpenImageIO_v2_4;

// Range TODO: replace with C++20 Range
#include <boost/range/counting_range.hpp>

#include <boost/algorithm/string/join.hpp>

#include <memory>

struct SatMapTile {
    fs::path path = {};
    TileTransform tt = {};
};

struct FillerMapTile {
    fs::path path = {};
    std::set<TileTransform, CmpTileTransform> tt = {};
};

static const std::vector<std::string> texture_config_path    { "Stage0", "texture" };
static const std::vector<std::string> texgen_config_path     { "Stage0", "texGen" };

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
        std::vector<SatMapTile> satMapTiles = {};
        satMapTiles.reserve(rvmats.size());

        std::string pboPath = "";
        try {
            pboPath = findPboPath(rvmats[0]).string();
            auto rvmatPbo = rvff::cxx::create_pbo_reader_path(pboPath);

            // Has to be not empty
            std::string lastValidRvMat = "1337";
            std::optional<FillerMapTile> fillerTile = std::nullopt;
            std::optional<TileTransform> firstPos = std::nullopt;
            std::string prefix = "s_";

            for (auto& rvmatPath : rvmats) {
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
                        if (boost::istarts_with(rvmatFilename, prefix)) {
                            lastValidRvMat = rvmatFilename.substr(0, 9);

                            auto tt = getTileTransform(rap);

                            if (!firstPos.has_value()) {
                                firstPos = tt;
                            }

                            struct SatMapTile tile = { textureStr, tt };
                            satMapTiles.push_back(tile);
                        }
                        else {
                            if(!fillerTile.has_value()) {
                                struct FillerMapTile tile = { textureStr, {} };
                                fillerTile = tile;
                            }

                            auto tt = getTileTransform(rap);

                            if (!firstPos.has_value()) {
                                firstPos = tt;
                            }
                            fillerTile->tt.emplace(tt);
                        }
                    }

                }
            }

            // Remove Duplicates
            std::sort(satMapTiles.begin(), satMapTiles.end(), [](const auto& lhs, const auto& rhs) {
                return lhs.path.string() < rhs.path.string();
            });
            auto last = std::unique(satMapTiles.begin(), satMapTiles.end(), [](const auto& lhs, const auto& rhs) {
                return lhs.path == rhs.path;
            });
            satMapTiles.erase(last, satMapTiles.end());

            // Fix wrong file extensions (cup summer has png extension)
            for (auto& smt : satMapTiles) {
                fs::path lcoPathAsPath = smt.path;
                if (!boost::iequals(lcoPathAsPath.extension().string(), ".paa")) {
                    smt.path = lcoPathAsPath.replace_extension(".paa").string();
                }
            }

            auto& firstSmt = satMapTiles[0];

            auto pbo = rvff::cxx::create_pbo_reader_path(findPboPath(firstSmt.path.string()).string());
            auto firstData = pbo->get_entry_data(firstSmt.path.string());
            auto firstTile = rvff::cxx::get_mipmap_from_paa_vec(firstData, 0);

            auto tileWidth = firstTile.width;
            auto tileHeight = firstTile.height;

            TileTransform firstTT = firstSmt.tt;
            if (firstPos.has_value()) {
                firstTT = *firstPos;
            }

            auto firstWorldPos = firstTT.pos;
            auto firstAside = firstTT.aside;

            auto firstXPos = static_cast<int32_t>(firstWorldPos[0] * tileWidth);
            auto firstYPos = static_cast<int32_t>(firstWorldPos[1] * tileHeight);

            int32_t finalSatMapSize = firstYPos - firstXPos;

            ImageBuf dst(ImageSpec(finalSatMapSize, finalSatMapSize, 4, TypeDesc::UINT8));
            auto& dstSpec = dst.spec();

            if (fillerTile.has_value()) {
                auto fillerPbo = rvff::cxx::create_pbo_reader_path(findPboPath(fillerTile->path.string()).string());
                auto fillerData = fillerPbo->get_entry_data(fillerTile->path.string());
                auto filler_mm = rvff::cxx::get_mipmap_from_paa_vec(fillerData, 0);

                auto fillerWidth = filler_mm.width;
                auto fillerHeight = filler_mm.height;

                ImageBuf src(ImageSpec(fillerWidth, fillerHeight, 4, TypeDesc::UINT8), filler_mm.data.data());

                int32_t numOfSubTiles = tileWidth / filler_mm.width;

                ImageBuf fullFillerTile(ImageSpec(tileWidth, tileHeight, 4, TypeDesc::UINT8));

                for (int32_t i = 0; i < numOfSubTiles; i++) {
                    for (int32_t j = 0; j < numOfSubTiles; j++) {
                        ImageBufAlgo::paste(fullFillerTile, i * fillerWidth, j * fillerHeight, 0, 0, src);
                    }
                }

                #ifdef _DEBUG
                auto copy_filler = fullFillerTile.copy(TypeDesc::UINT8);
                copy_filler.write((basePathSat / "debug_filler_tile.exr").string());
                #endif

                auto& fillterTTs = fillerTile->tt;

                for (auto& tt : fillterTTs) {
                    auto pos = tt.pos;
                    auto xPos = static_cast<int32_t>(pos[0] * tileWidth * -1);
                    auto yPos = static_cast<int32_t>(((pos[1] * tileHeight) - dstSpec.width) * -1);

                    ImageBufAlgo::paste(dst, xPos, yPos, 0, 0, fullFillerTile);
                }

                //#ifdef _DEBUG
                auto copy_full = dst.copy(TypeDesc::UINT8);
                copy_full.write((basePathSat / "debug_sat_map_only_filler.exr").string());
                //#endif
            }

            int c = 0;
            for (auto& smt : satMapTiles) {
                auto data = pbo->get_entry_data(smt.path.string());

                if (data.empty()) {
                    pbo = rvff::cxx::create_pbo_reader_path(findPboPath(smt.path.string()).string());
                    data = pbo->get_entry_data(smt.path.string());
                }

                auto mipmap = rvff::cxx::get_mipmap_from_paa_vec(data, 0);

                ImageBuf src(ImageSpec(mipmap.width, mipmap.height, 4, TypeDesc::UINT8), mipmap.data.data());

                auto width = mipmap.width;
                auto height = mipmap.height;

                if (mipmap.width == 4 && mipmap.height == 4) {
                    src = ImageBufAlgo::resize(src, "", 0, ROI(0, tileWidth, 0, tileHeight));
                    width = tileWidth;
                    height = tileHeight;
                    PLOG_INFO << fmt::format("Old filler method detected, resizing 4x4 tiles to {}x{} !", tileWidth, tileHeight);
                }

                auto pos = smt.tt.pos;

                auto xPos = static_cast<int32_t>(pos[0] * width * -1);
                auto yPos = static_cast<int32_t>(((pos[1] * height) - dstSpec.width) * -1);

                ImageBufAlgo::paste(dst, xPos, yPos, 0, 0, src);

                auto copy_party = dst.copy(TypeDesc::UINT8);
                copy_party.write((basePathSat / fmt::format("debug_part_{}.exr", c++)).string());
            }

            //#ifdef _DEBUG
            auto copy = dst.copy(TypeDesc::UINT8);
            copy.write((basePathSat / "debug_full_streched.exr").string());
            //#endif

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

TileTransform getTileTransform(rust::Box<rvff::cxx::CfgCxx>& rap) {
    auto texGenValue = rap->get_entry_as_number(texgen_config_path);

    auto tex_gen_class = fmt::format("TexGen{}", texGenValue);
    std::vector<std::string> texgenDirConfigPath { tex_gen_class, "uvTransform", "pos" };
    std::vector<std::string> texgenAsideConfigPath { tex_gen_class, "uvTransform", "aside" };

    auto posArr = rap->get_entry_as_array_float(texgenDirConfigPath);
    std::vector<float_t> posArrStd(posArr.begin(), posArr.end());

    if (posArrStd.size() < 2) {
        auto errMsg = fmt::format("arr size for '{}' was < 2", ba::join(texgenDirConfigPath, " >> "));
        PLOG_ERROR << errMsg;
        throw new std::runtime_error(errMsg);
    }

    auto asideArr = rap->get_entry_as_array_float(texgenAsideConfigPath);
    std::vector<float_t> asideArrStd(asideArr.begin(), asideArr.end());

    if (asideArrStd.size() < 2) {
        auto errMsg = fmt::format("arr size for '{}' was < 2", ba::join(texgenAsideConfigPath, " >> "));
        PLOG_ERROR << errMsg;
        throw new std::runtime_error(errMsg);
    }

    TileTransform tt {
        asideArrStd,
        posArrStd
    };

    return tt;
}

