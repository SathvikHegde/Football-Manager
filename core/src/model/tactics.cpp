#include "fm/model/tactics.hpp"
#include <algorithm>

namespace fm {

TacticalConfig get_default_tactics(FormationId formation) {
    TacticalConfig config{};
    config.formation = formation;
    config.defensive_line = 0.5f;
    config.pressing_intensity = 0.5f;
    config.passing_directness = 0.5f;
    config.tempo = 0.5f;

    // 0 is always Goalkeeper
    config.base_anchors_norm[0] = Vec2{0.05f, 0.50f};

    switch (formation) {
        case FormationId::F_433:
            config.base_anchors_norm[1]  = Vec2{0.25f, 0.15f}; // LB
            config.base_anchors_norm[2]  = Vec2{0.22f, 0.38f}; // CB
            config.base_anchors_norm[3]  = Vec2{0.22f, 0.62f}; // CB
            config.base_anchors_norm[4]  = Vec2{0.25f, 0.85f}; // RB
            config.base_anchors_norm[5]  = Vec2{0.42f, 0.50f}; // DM
            config.base_anchors_norm[6]  = Vec2{0.55f, 0.32f}; // CM
            config.base_anchors_norm[7]  = Vec2{0.55f, 0.68f}; // CM
            config.base_anchors_norm[8]  = Vec2{0.72f, 0.18f}; // LW
            config.base_anchors_norm[9]  = Vec2{0.78f, 0.50f}; // ST
            config.base_anchors_norm[10] = Vec2{0.72f, 0.82f}; // RW
            break;

        case FormationId::F_4231:
            config.base_anchors_norm[1]  = Vec2{0.25f, 0.15f}; // LB
            config.base_anchors_norm[2]  = Vec2{0.22f, 0.38f}; // CB
            config.base_anchors_norm[3]  = Vec2{0.22f, 0.62f}; // CB
            config.base_anchors_norm[4]  = Vec2{0.25f, 0.85f}; // RB
            config.base_anchors_norm[5]  = Vec2{0.40f, 0.38f}; // DM
            config.base_anchors_norm[6]  = Vec2{0.40f, 0.62f}; // DM
            config.base_anchors_norm[7]  = Vec2{0.62f, 0.18f}; // LAM
            config.base_anchors_norm[8]  = Vec2{0.62f, 0.50f}; // CAM
            config.base_anchors_norm[9]  = Vec2{0.62f, 0.82f}; // RAM
            config.base_anchors_norm[10] = Vec2{0.78f, 0.50f}; // ST
            break;

        case FormationId::F_352:
            config.base_anchors_norm[1]  = Vec2{0.22f, 0.28f}; // CB
            config.base_anchors_norm[2]  = Vec2{0.20f, 0.50f}; // CB
            config.base_anchors_norm[3]  = Vec2{0.22f, 0.72f}; // CB
            config.base_anchors_norm[4]  = Vec2{0.46f, 0.12f}; // LWB
            config.base_anchors_norm[5]  = Vec2{0.44f, 0.36f}; // CM
            config.base_anchors_norm[6]  = Vec2{0.42f, 0.50f}; // CM
            config.base_anchors_norm[7]  = Vec2{0.44f, 0.64f}; // CM
            config.base_anchors_norm[8]  = Vec2{0.46f, 0.88f}; // RWB
            config.base_anchors_norm[9]  = Vec2{0.75f, 0.40f}; // ST
            config.base_anchors_norm[10] = Vec2{0.75f, 0.60f}; // ST
            break;

        case FormationId::F_532:
            config.base_anchors_norm[1]  = Vec2{0.30f, 0.12f}; // LWB
            config.base_anchors_norm[2]  = Vec2{0.20f, 0.32f}; // CB
            config.base_anchors_norm[3]  = Vec2{0.18f, 0.50f}; // CB
            config.base_anchors_norm[4]  = Vec2{0.20f, 0.68f}; // CB
            config.base_anchors_norm[5]  = Vec2{0.30f, 0.88f}; // RWB
            config.base_anchors_norm[6]  = Vec2{0.46f, 0.32f}; // CM
            config.base_anchors_norm[7]  = Vec2{0.44f, 0.50f}; // CM
            config.base_anchors_norm[8]  = Vec2{0.46f, 0.68f}; // CM
            config.base_anchors_norm[9]  = Vec2{0.74f, 0.40f}; // ST
            config.base_anchors_norm[10] = Vec2{0.74f, 0.60f}; // ST
            break;

        case FormationId::F_442:
        default:
            config.base_anchors_norm[1]  = Vec2{0.25f, 0.15f}; // LB
            config.base_anchors_norm[2]  = Vec2{0.22f, 0.38f}; // CB
            config.base_anchors_norm[3]  = Vec2{0.22f, 0.62f}; // CB
            config.base_anchors_norm[4]  = Vec2{0.25f, 0.85f}; // RB
            config.base_anchors_norm[5]  = Vec2{0.50f, 0.15f}; // LM
            config.base_anchors_norm[6]  = Vec2{0.48f, 0.38f}; // CM
            config.base_anchors_norm[7]  = Vec2{0.48f, 0.62f}; // CM
            config.base_anchors_norm[8]  = Vec2{0.50f, 0.85f}; // RM
            config.base_anchors_norm[9]  = Vec2{0.75f, 0.40f}; // ST
            config.base_anchors_norm[10] = Vec2{0.75f, 0.60f}; // ST
            break;
    }

    return config;
}

RoleId get_formation_role(FormationId formation, uint8_t local_slot) {
    if (local_slot == 0) {
        return RoleId::Goalkeeper;
    }

    switch (formation) {
        case FormationId::F_433:
            if (local_slot == 1 || local_slot == 4) return RoleId::FullBack;
            if (local_slot == 2 || local_slot == 3) return RoleId::CenterBack;
            if (local_slot >= 5 && local_slot <= 7) return RoleId::Midfielder;
            if (local_slot == 8 || local_slot == 10) return RoleId::Winger;
            return RoleId::Forward;

        case FormationId::F_4231:
            if (local_slot == 1 || local_slot == 4) return RoleId::FullBack;
            if (local_slot == 2 || local_slot == 3) return RoleId::CenterBack;
            if (local_slot == 5 || local_slot == 6 || local_slot == 8) return RoleId::Midfielder;
            if (local_slot == 7 || local_slot == 9) return RoleId::Winger;
            return RoleId::Forward;

        case FormationId::F_352:
            if (local_slot >= 1 && local_slot <= 3) return RoleId::CenterBack;
            if (local_slot == 4 || local_slot == 8) return RoleId::Winger;
            if (local_slot >= 5 && local_slot <= 7) return RoleId::Midfielder;
            return RoleId::Forward;

        case FormationId::F_532:
            if (local_slot == 1 || local_slot == 5) return RoleId::FullBack;
            if (local_slot >= 2 && local_slot <= 4) return RoleId::CenterBack;
            if (local_slot >= 6 && local_slot <= 8) return RoleId::Midfielder;
            return RoleId::Forward;

        case FormationId::F_442:
        default:
            if (local_slot == 1 || local_slot == 4) return RoleId::FullBack;
            if (local_slot == 2 || local_slot == 3) return RoleId::CenterBack;
            if (local_slot == 5 || local_slot == 8) return RoleId::Winger;
            if (local_slot == 6 || local_slot == 7) return RoleId::Midfielder;
            return RoleId::Forward;
    }
}

namespace {

void sync_team_roles(MatchState& state, uint8_t team_id, FormationId formation) {
    const std::size_t start = (team_id == 0) ? 0 : 11;
    for (std::size_t local = 0; local < 11; ++local) {
        state.players[start + local].role = get_formation_role(formation, static_cast<uint8_t>(local));
    }
}

} // namespace

std::string_view formation_name(FormationId formation) noexcept {
    switch (formation) {
        case FormationId::F_433:  return "4-3-3";
        case FormationId::F_4231: return "4-2-3-1";
        case FormationId::F_352:  return "3-5-2";
        case FormationId::F_532:  return "5-3-2";
        case FormationId::F_442:
        default:                  return "4-4-2";
    }
}

FormationId sanitize_formation(int formation_id) noexcept {
    switch (formation_id) {
        case static_cast<int>(FormationId::F_442):  return FormationId::F_442;
        case static_cast<int>(FormationId::F_433):  return FormationId::F_433;
        case static_cast<int>(FormationId::F_4231): return FormationId::F_4231;
        case static_cast<int>(FormationId::F_352):  return FormationId::F_352;
        case static_cast<int>(FormationId::F_532):  return FormationId::F_532;
        default:                                    return FormationId::F_442;
    }
}

bool apply_tactical_change(MatchState& state,
                           uint8_t team_id,
                           FormationId formation,
                           float defensive_line,
                           float pressing_intensity) {
    const uint8_t safe_team = (team_id == 0) ? 0 : 1;
    TacticalConfig& current = (safe_team == 0) ? state.home_tactics : state.away_tactics;
    TacticalConfig next = current;

    const FormationId safe_form = sanitize_formation(static_cast<int>(formation));
    if (safe_form != current.formation ||
        (next.base_anchors_norm[0].x == 0.0f && next.base_anchors_norm[0].y == 0.0f)) {
        next.formation = safe_form;
        next.base_anchors_norm = get_default_tactics(safe_form).base_anchors_norm;
    }

    next.defensive_line = std::clamp(defensive_line, 0.0f, 1.0f);
    next.pressing_intensity = std::clamp(pressing_intensity, 0.0f, 1.0f);

    const bool changed = !(next == current);
    current = next;
    sync_team_roles(state, safe_team, current.formation);
    return changed;
}

bool apply_tactical_change(MatchState& state,
                           uint8_t team_id,
                           const TacticalConfig& new_tactics) {
    const uint8_t safe_team = (team_id == 0) ? 0 : 1;
    TacticalConfig& current = (safe_team == 0) ? state.home_tactics : state.away_tactics;

    TacticalConfig next = new_tactics;
    next.formation = sanitize_formation(static_cast<int>(new_tactics.formation));
    next.defensive_line = std::clamp(new_tactics.defensive_line, 0.0f, 1.0f);
    next.pressing_intensity = std::clamp(new_tactics.pressing_intensity, 0.0f, 1.0f);
    next.passing_directness = std::clamp(new_tactics.passing_directness, 0.0f, 1.0f);
    next.tempo = std::clamp(new_tactics.tempo, 0.0f, 1.0f);

    if (next.base_anchors_norm[0].x == 0.0f && next.base_anchors_norm[0].y == 0.0f) {
        next.base_anchors_norm = get_default_tactics(next.formation).base_anchors_norm;
    }

    const bool changed = !(next == current);
    current = next;
    sync_team_roles(state, safe_team, current.formation);
    return changed;
}

} // namespace fm
