#pragma once
#include <string_view>

namespace fm {

[[nodiscard]] constexpr std::string_view engine_status() noexcept {
    return "fm_core C++20 engine ready";
}

[[nodiscard]] constexpr std::string_view engine_version() noexcept {
    return "0.3.0-stage3";
}

} // namespace fm
