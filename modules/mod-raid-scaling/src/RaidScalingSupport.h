#ifndef MOD_RAID_SCALING_SUPPORT_H
#define MOD_RAID_SCALING_SUPPORT_H

#include "SpellInfo.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace RaidScalingSupport
{
    inline bool IsFlatHealing(SpellInfo const* spell)
    {
        if (!spell)
            return false;

        // These already depend on health/damage. Fail closed for mixed spells as well: the
        // final-heal hook cannot separate their flat and percentage/leech contributions.
        if (spell->HasEffect(SPELL_EFFECT_HEAL_PCT) || spell->HasEffect(SPELL_EFFECT_HEAL_MAX_HEALTH) ||
            spell->HasEffect(SPELL_EFFECT_HEALTH_LEECH) || spell->HasAura(SPELL_AURA_OBS_MOD_HEALTH) ||
            spell->HasAura(SPELL_AURA_PERIODIC_LEECH) || spell->HasAura(SPELL_AURA_PERIODIC_HEALTH_FUNNEL))
            return false;

        return spell->HasEffect(SPELL_EFFECT_HEAL) || spell->HasEffect(SPELL_EFFECT_HEAL_MECHANICAL) ||
            spell->HasAura(SPELL_AURA_PERIODIC_HEAL);
    }

    inline uint32 ScaleAmount(uint32 amount, float scale)
    {
        if (!amount || scale == 1.0f)
            return amount;

        // DealHeal applies a signed health delta; finite shields are signed amounts too.
        double const scaled = std::round(double(amount) * scale);
        return uint32(std::clamp(scaled, 1.0, double(std::numeric_limits<int32>::max())));
    }
}

#endif
