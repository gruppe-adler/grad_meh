#pragma once

#include <intercept.hpp>

#include <string>
#include <string_view>
#include <vector>

#ifdef __linux__
std::vector<std::string> generate_pbo_list();
#else
std::vector<std::wstring> generate_pbo_list();
#endif // __linux__
