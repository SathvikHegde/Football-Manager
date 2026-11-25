#include "fm/model/team.hpp"
#include <algorithm>

namespace fm {

namespace {

constexpr uint8_t clamp_attr(int value) noexcept {
    return static_cast<uint8_t>(std::clamp(value, 1, 20));
}

} // namespace

PlayerAttributes make_role_attributes(RoleId role, uint8_t base_rating) noexcept {
    const int base = static_cast<int>(std::clamp<uint8_t>(base_rating, 1, 20));
    PlayerAttributes attrs{};
    attrs.pace         = clamp_attr(base);
    attrs.acceleration = clamp_attr(base);
    attrs.stamina      = clamp_attr(base);
    attrs.passing      = clamp_attr(base);
    attrs.vision       = clamp_attr(base);
    attrs.shooting     = clamp_attr(base);
    attrs.dribbling    = clamp_attr(base);
    attrs.tackling     = clamp_attr(base);
    attrs.positioning  = clamp_attr(base);
    attrs.decisions    = clamp_attr(base);

    // Preserve realistic positional strengths balanced cleanly around base_rating
    switch (role) {
        case RoleId::Goalkeeper:
            attrs.positioning  = clamp_attr(base + 2);
            attrs.acceleration = clamp_attr(base + 1);
            attrs.decisions    = clamp_attr(base + 1);
            attrs.passing      = clamp_attr(base);
            attrs.pace         = clamp_attr(base - 1);
            attrs.tackling     = clamp_attr(base - 1);
            attrs.dribbling    = clamp_attr(base - 2);
            attrs.shooting     = clamp_attr(base - 3);
            break;

        case RoleId::CenterBack:
            attrs.tackling     = clamp_attr(base + 2);
            attrs.positioning  = clamp_attr(base + 2);
            attrs.decisions    = clamp_attr(base + 1);
            attrs.pace         = clamp_attr(base - 1);
            attrs.passing      = clamp_attr(base);
            attrs.dribbling    = clamp_attr(base - 1);
            attrs.shooting     = clamp_attr(base - 3);
            break;

        case RoleId::FullBack:
            attrs.pace         = clamp_attr(base + 1);
            attrs.stamina      = clamp_attr(base + 1);
            attrs.tackling     = clamp_attr(base + 1);
            attrs.acceleration = clamp_attr(base + 1);
            attrs.shooting     = clamp_attr(base - 2);
            break;

        case RoleId::Midfielder:
            attrs.passing      = clamp_attr(base + 2);
            attrs.vision       = clamp_attr(base + 2);
            attrs.decisions    = clamp_attr(base + 1);
            attrs.stamina      = clamp_attr(base + 1);
            attrs.dribbling    = clamp_attr(base + 1);
            attrs.positioning  = clamp_attr(base + 1);
            break;

        case RoleId::Winger:
            attrs.pace         = clamp_attr(base + 2);
            attrs.acceleration = clamp_attr(base + 2);
            attrs.dribbling    = clamp_attr(base + 2);
            attrs.vision       = clamp_attr(base + 1);
            attrs.stamina      = clamp_attr(base + 1);
            attrs.tackling     = clamp_attr(base - 2);
            attrs.positioning  = clamp_attr(base - 1);
            break;

        case RoleId::Forward:
            attrs.shooting     = clamp_attr(base + 2);
            attrs.dribbling    = clamp_attr(base + 1);
            attrs.pace         = clamp_attr(base + 1);
            attrs.acceleration = clamp_attr(base + 1);
            attrs.decisions    = clamp_attr(base + 1);
            attrs.tackling     = clamp_attr(base - 2);
            break;
    }

    return attrs;
}

void apply_team_rating(MatchState& state, uint8_t team_id, uint8_t base_rating) noexcept {
    const std::size_t start = (team_id == 0) ? 0u : 11u;
    const uint8_t safe_rating = std::clamp<uint8_t>(base_rating, 1, 20);
    for (std::size_t i = 0; i < 11; ++i) {
        auto& p = state.players[start + i];
        p.attrs = make_role_attributes(p.role, safe_rating);
    }
}

} // namespace fm

