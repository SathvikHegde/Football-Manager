#include "fm/model/tactics.hpp"

namespace fm {

TacticalConfig get_default_tactics(FormationId formation) {
    TacticalConfig config{};
    config.formation = formation;
    config.defensive_line = 0.5f;
    config.pressing_intensity = 0.5f;
    config.passing_directness = 0.5f;
    config.tempo = 0.5f;

    // Default normalized 4-4-2 positions [0.0, 1.0]
    // 0: GK
    config.base_anchors_norm[0]  = Vec2{0.05f, 0.50f};
    // 1-4: Back four (LB, CB, CB, RB)
    config.base_anchors_norm[1]  = Vec2{0.25f, 0.15f};
    config.base_anchors_norm[2]  = Vec2{0.22f, 0.38f};
    config.base_anchors_norm[3]  = Vec2{0.22f, 0.62f};
    config.base_anchors_norm[4]  = Vec2{0.25f, 0.85f};
    // 5-8: Midfield four (LM, CM, CM, RM)
    config.base_anchors_norm[5]  = Vec2{0.50f, 0.15f};
    config.base_anchors_norm[6]  = Vec2{0.48f, 0.38f};
    config.base_anchors_norm[7]  = Vec2{0.48f, 0.62f};
    config.base_anchors_norm[8]  = Vec2{0.50f, 0.85f};
    // 9-10: Strikers (ST, ST)
    config.base_anchors_norm[9]  = Vec2{0.75f, 0.40f};
    config.base_anchors_norm[10] = Vec2{0.75f, 0.60f};

    return config;
}

} // namespace fm

