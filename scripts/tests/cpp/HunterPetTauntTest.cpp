#include "PlayerbotsHunterPetTaunt.cpp"
#include <iostream>
#include <memory>

struct Fixture
{
    Group group, otherGroup;
    Player owner, tank, healer;
    Pet pet;
    Unit target, npc;
    SpellInfo growl, info;
    Spell spell;
    Fixture()
    {
        owner.group = tank.group = healer.group = &group;
        tank.tankSpec = true;
        pet.owner = &owner;
        target.victim = &tank;
        info.firstRank = &growl;
        spell.caster = &pet;
        spell.info = &info;
        spell.m_targets.unit = &target;
    }
    void Check(AllSpellScript& hook, bool blocked)
    {
        for (bool strict : {false, true})
        {
            SpellCastResult result = SPELL_CAST_OK;
            hook.OnSpellCheckCast(&spell, strict, result);
            assert(result == (blocked ? SPELL_FAILED_DONT_REPORT : SPELL_CAST_OK));
        }
        assert(hook.CanPrepare(&spell, &spell.m_targets, nullptr) == !blocked);
    }
};

int main()
{
    AddPlayerbotsHunterPetTauntScripts();
    std::unique_ptr<AllSpellScript> hook(AllSpellScript::registered);
    assert(hook && hook->name == "PlayerbotsHunterPetTauntScript");
    assert(hook->hooks == std::vector<uint16>({ALLSPELLHOOK_ON_SPELL_CHECK_CAST, ALLSPELLHOOK_CAN_PREPARE}));
    for (uint32 rank : {2649, 14916, 14917, 14918, 14919, 14920, 14921, 27047, 61676})
    {
        Fixture f;
        f.info.Id = rank;
        f.Check(*hook, true);
        f.owner.group = nullptr;
        f.Check(*hook, false); // Every rank remains usable by solo hunters.
    }
    for (bool botOwner : {false, true})
        for (bool botTank : {false, true})
        {
            Fixture f;
            f.owner.bot = botOwner;
            f.tank.bot = botTank;
            f.Check(*hook, true); // Human/bot owners and tanks use the same policy.
        }
    // All excluded situations must preserve ordinary behavior.
    for (int scenario = 0; scenario < 15; ++scenario)
    {
        Fixture f;
        switch (scenario)
        {
            case 0: f.owner.group = nullptr; break;
            case 1: f.pet.owner = nullptr; break;
            case 2: f.pet.type = SUMMON_PET; break;
            case 3: f.owner.playerClass = CLASS_WARLOCK; break;
            case 4: f.spell.caster = &f.owner; break;
            case 5: f.spell.caster = nullptr; break;
            case 6: f.spell.info = nullptr; break;
            case 7: f.spell.m_targets.unit = nullptr; break;
            case 8: f.target.victim = nullptr; break;
            case 9: f.target.victim = &f.healer; break;
            case 10: f.target.victim = &f.pet; break;
            case 11: f.target.victim = &f.npc; break;
            case 12: f.tank.alive = false; break;
            case 13: f.tank.world = false; break;
            case 14: f.tank.group = &f.otherGroup; break;
        }
        f.Check(*hook, false);
    }
    {
        Fixture f;
        f.tank.tankSpec = false;
        f.tank.runtimeTankStrategy = true;
        f.Check(*hook, false); // A bot strategy alone is not a tank-spec designation.
        f.tank.tankSpec = true;
        f.tank.runtimeTankStrategy = false;
        f.Check(*hook, true);
    }
    for (uint32 id : {17253, 16827, 63900, 24394}) // Bite, Claw, Thunderstomp, Intimidation
    {
        Fixture f;
        f.info.Id = id;
        f.info.firstRank = nullptr;
        f.Check(*hook, false);
    }
    for (bool auraOnly : {false, true})
    {
        Fixture f;
        f.info.Id = 53477;
        f.info.firstRank = nullptr;
        f.info.tauntEffect = !auraOnly;
        f.info.tauntAura = auraOnly;
        f.Check(*hook, true);
        f.target.victim = &f.healer;
        f.Check(*hook, false);
    }
    {
        Fixture f;
        f.target.victim = nullptr;
        f.target.threat.victim = &f.tank;
        f.Check(*hook, true);
        f.target.threat.enabled = false;
        f.Check(*hook, false);
        f.target.threat.enabled = true;
        f.target.victim = &f.healer;
        f.Check(*hook, false); // Actual victim overrides a stale threat-manager fallback.
    }
    {
        Fixture f;
        f.target.victim = &f.healer;
        SpellCastResult result = SPELL_CAST_OK;
        hook->OnSpellCheckCast(&f.spell, true, result);
        assert(result == SPELL_CAST_OK);
        f.target.victim = &f.tank; // tank picks it up between selection and preparation
        assert(!hook->CanPrepare(&f.spell, &f.spell.m_targets, nullptr));
        f.target.victim = &f.healer;
        f.Check(*hook, false); // no saved autocast state to restore
        f.target.victim = &f.tank;
        f.owner.group = nullptr;
        f.Check(*hook, false);
        f.owner.group = &f.group;
        f.Check(*hook, true);
    }
    {
        Fixture f;
        SpellCastResult result = SPELL_FAILED_LINE_OF_SIGHT;
        hook->OnSpellCheckCast(&f.spell, true, result);
        assert(result == SPELL_FAILED_LINE_OF_SIGHT);
        result = SPELL_CAST_OK;
        hook->OnSpellCheckCast(nullptr, true, result);
        assert(result == SPELL_CAST_OK);
        assert(hook->CanPrepare(nullptr, nullptr, nullptr));
    }
    std::cout << "Production hunter pet taunt hook tests passed\n";
}
