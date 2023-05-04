#pragma once

#include "util.h"

#include <vector>
#include <filesystem>
#include <execution>

#include <rust-lib/lib.h>

namespace fs = std::filesystem;

float_t mean(std::vector<float_t>& equalities);
float_t compareColors(uint8_t r1, uint8_t g1, uint8_t b1, uint8_t r2, uint8_t g2, uint8_t b2);
float_t compareRows(std::vector<uint8_t>::iterator mipmap1It, std::vector<uint8_t>::iterator mipmap2It, uint32_t width);
uint32_t calcOverLap(rvff::cxx::MipmapCxx& mipmap1, rvff::cxx::MipmapCxx& mipmap2);
void writeSatImages(rvff::cxx::OprwCxx& wrp, const int32_t& worldSize, std::filesystem::path& basePathSat, const std::string& worldName);