#include "fm/model/tactics.hpp"

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

} // namespace fm
