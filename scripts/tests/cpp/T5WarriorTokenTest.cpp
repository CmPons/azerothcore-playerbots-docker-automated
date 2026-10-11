// Appended to the shared production-loot/scorer fixture by test_t5_warrior_token_loot.py.
void T5WarriorTests()
{
    std::array<uint32, 5> tokens{30237, 30240, 30243, 30246, 30249};
    std::array<uint32, 5> tankItems{30113, 30114, 30115, 30116, 30117};
    std::array<uint32, 5> damageItems{30118, 30119, 30120, 30121, 30122};
    Player warrior, priest, secondPriest, druid;
    warrior.cls = CLASS_WARRIOR;
    warrior.tab = WARRIOR_TAB_PROTECTION;
    priest.cls = secondPriest.cls = CLASS_PRIEST;
    priest.tab = secondPriest.tab = PRIEST_TAB_DISCIPLINE;
    druid.cls = CLASS_DRUID;
    druid.tab = DRUID_TAB_RESTORATION;
    Group group;
    group.Set({&priest, &warrior, &secondPriest, &druid});
    PlayerbotsDirectTokenLootScript hook;
    for (uint32 item : tankItems)
        objects.items.at(item).skill = 293; // Native ItemTemplate::GetSkill: plate mail.
    for (uint32 item : damageItems)
        objects.items.at(item).skill = 293;

    for (unsigned slot = 0; slot < tokens.size(); ++slot)
    {
        auto rewards = FindDirectRewards(tokens[slot], 3);
        uint32 mask = 0;
        for (auto const& reward : rewards)
            mask |= reward.classes;
        assert(mask == ((1u << 0) | (1u << 4) | (1u << 10)));
        for (uint32 index = 0; index < 3; ++index)
        {
            rolls = {index};
            rollBounds.clear();
            assert((PickRewardClass(rewards) == std::array<uint8, 3>{1, 5, 11}[index]));
            assert((rollBounds == std::vector<std::pair<uint32, uint32>>{{0, 2}}));
            // The complete hook with a priest loot owner still sees the human warrior.
            rolls = {index, 0};
            Loot loot;
            loot.items = {{tokens[slot]}};
            hook.OnAfterLootTemplateProcess(&loot, nullptr, LootTemplates_Creature, &priest, false, false, 1);
            assert(loot.items[0].itemid != tokens[slot]);
            assert(objects.items.at(loot.items[0].itemid).AllowableClass == (1u << (std::array<uint8, 3>{1, 5, 11}[index] - 1)));
            if (index == 0)
                assert(loot.items[0].itemid == tankItems[slot]);
        }
        auto winner = BestSpecRewards(rewards, PresentPlayers(&priest, false), CLASS_WARRIOR);
        assert(winner == std::vector<uint32>{tankItems[slot]});
        StatsWeightCalculator scorer(&warrior, true);
        float const tankScore = scorer.CalculateItem(tankItems[slot]);
        float const damageScore = scorer.CalculateItem(damageItems[slot]);
        assert(std::isfinite(tankScore) && tankScore > damageScore);
        std::cout << tokens[slot] << '\t' << tankItems[slot] << '\t' << tankScore << '\t' << damageScore << '\n';

        // All six without-replacement orders yield exactly one warrior, priest and druid.
        for (uint32 first = 0; first < 3; ++first)
            for (uint32 second = 0; second < 2; ++second)
            {
                rolls = {first, 0, second, 0, 0, 0};
                Loot loot;
                loot.items = {{tokens[slot]}, {tokens[slot]}, {tokens[slot]}};
                hook.OnAfterLootTemplateProcess(&loot, nullptr, LootTemplates_Creature, &priest, false, false, 1);
                std::set<uint32> classes;
                unsigned tankCount = 0;
                for (auto const& item : loot.items)
                {
                    classes.insert(objects.items.at(item.itemid).AllowableClass);
                    tankCount += item.itemid == tankItems[slot];
                }
                assert(classes == std::set<uint32>({1, 16, 1024}) && tankCount == 1);
            }

        // Being elsewhere retains the token, not a reroll to another class. Return restores eligibility.
        warrior.map = 2;
        rolls = {0};
        Loot absent;
        absent.items = {{tokens[slot]}};
        hook.OnAfterLootTemplateProcess(&absent, nullptr, LootTemplates_Creature, &priest, false, false, 1);
        assert(absent.items[0].itemid == tokens[slot]);
        warrior.map = 1;
        warrior.tab = WARRIOR_TAB_FURY;
        assert(BestSpecRewards(rewards, {&warrior}, CLASS_WARRIOR) == std::vector<uint32>{damageItems[slot]});
        warrior.tab = WARRIOR_TAB_ARMS;
        assert(BestSpecRewards(rewards, {&warrior}, CLASS_WARRIOR) == std::vector<uint32>{damageItems[slot]});
        warrior.tab = WARRIOR_TAB_PROTECTION;
        warrior.skilled = false;
        assert(BestSpecRewards(rewards, {&warrior}, CLASS_WARRIOR).empty());
        warrior.skilled = true;
        // A fresh corpse can repeat an already-owned slot: there is no history/upgrade guarantee.
        rolls = {0, 0};
        Loot fresh;
        fresh.items = {{tokens[slot]}};
        hook.OnAfterLootTemplateProcess(&fresh, nullptr, LootTemplates_Creature, &warrior, false, false, 1);
        assert(fresh.items[0].itemid == tankItems[slot]);
    }
    // Both ordinary gear families exclude warrior; the converter does not turn them into Defender tokens.
    for (uint32 token = 30236; token <= 30250; ++token)
        if (std::find(tokens.begin(), tokens.end(), token) == tokens.end())
        {
            uint32 mask = 0;
            for (auto const& reward : FindDirectRewards(token, 3))
                mask |= reward.classes;
            assert(!(mask & 1));
        }
    std::cout << "PASS: five T5 Protection winners, all class indices, repeated-token permutations, presence and DPS tabs\n";
}

int main()
{
    LoadFixture();
    T5WarriorTests();
}
