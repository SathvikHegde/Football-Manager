#pragma once
#include <string>
#include <cstdint>

namespace fm {

struct Team {
    uint8_t team_id{0};
    std::string name{"Default Team"};
    uint32_t primary_color{0xFF0000};
    uint32_t secondary_color{0xFFFFFF};
};

} // namespace fm

