// Includes the complete production command handler, map hook and runtime policy adapter.
#include "RaidThreatControl.cpp"
#include <iostream>
using namespace ai::threat::control;
int main()
{
    AddRaidThreatControlScripts();
    auto table = CommandScript::registered->GetCommands();
    assert(table.size() == 1 && std::string(table[0].name) == "raidthreat");
    assert(table[0].security == SEC_GAMEMASTER && table[0].console == Acore::ChatCommands::Console::No);
    assert(AllMapScript::registered->hooks == std::vector<uint16>{ALLMAPHOOK_ON_DESTROY_MAP});
    auto command = table[0].handler;
    Map tk, other, sameNumber;
    other.instance++;
    sameNumber.id = 548;
    Player user;
    user.map = &tk;
    user.name = "Meliah";
    WorldSession session{&user};
    ChatHandler chat{&session, {}};
    Unit recipient;
    recipient.map = &tk;
    assert(command(&chat, "status"));
    assert(chat.Contains("No healing threat checks"));
    assert(GetSettings(&tk).healing == HealingMode::Emergency);
    assert(command(&chat, "aoe 95"));
    assert(command(&chat, "target 90"));
    assert(command(&chat, "boss 85"));
    assert(command(&chat, "healing emergency 40"));
    auto settings = GetSettings(&tk);
    assert(settings.aoe == 95 && settings.target == 90 && settings.boss == 85);
    assert(settings.emergencyHealth == 40 && settings.healing == HealingMode::Emergency);
    assert(GetSettings(&other).aoe == 90 && GetSettings(&sameNumber).target == 80);
    for (char const* bad : {"aoe -1", "boss 101", "target 90 trailing", "healing nonsense", "reset all"})
    {
        assert(!command(&chat, bad));
        assert(GetSettings(&tk).generation == settings.generation);
    }
    Record(&user, &recipient, "greater heal on party", Reason::Target, settings, 40, 91);
    Record(&user, &recipient, "flash heal on party", Reason::Emergency, settings);
    chat.messages.clear();
    assert(command(&chat, "status"));
    assert(chat.Contains("emergency healing bypass") && chat.Contains("BLOCKED: current-target threat limit"));
    assert(chat.Contains("not evaluated") && chat.Contains("NOT proof a heal cast"));
    assert(chat.Contains("Arinerica") && chat.Contains("Meliah"));
    assert(chat.Contains("[95]") && chat.Contains("[90]") && chat.Contains("[85]"));
    assert(command(&chat, "healing normal"));
    assert(GetSettings(&tk).healing == HealingMode::Normal);
    Record(&user, &recipient, "stale heal", Reason::Target, settings, 40, 91);
    chat.messages.clear();
    assert(command(&chat, "status") && chat.Contains("No healing threat checks"));
    assert(command(&chat, "healing exempt"));
    assert(GetSettings(&tk).healing == HealingMode::Exempt);
    assert(command(&chat, "reset"));
    assert(GetSettings(&tk).healing == HealingMode::Emergency && GetSettings(&tk).aoe == 90);
    config.boss = 0;
    assert(GetSettings(&tk).boss == 1);
    config.boss = 999;
    assert(GetSettings(&tk).boss == 100);
    config.boss = 70;
    assert(command(&chat, "aoe 95"));
    AllMapScript::registered->OnDestroyMap(&tk);
    assert(GetSettings(&tk).aoe == 90);
    chat.messages.clear();
    assert(command(&chat, "status") && chat.Contains("No healing threat checks"));
    tk.raid = false;
    assert(!command(&chat, "aoe 95"));
    assert(GetSettings(&tk).healing == HealingMode::Normal);
    tk.raid = true;
    tk.instance = 0;
    assert(!command(&chat, "aoe 95") && GetSettings(&tk).healing == HealingMode::Normal);
    chat.session = nullptr;
    assert(!command(&chat, "status"));
    assert(GetSettings(nullptr).healing == HealingMode::Normal);
    Record(nullptr, &recipient, "heal", Reason::Aoe, settings);
    delete CommandScript::registered;
    delete AllMapScript::registered;
    std::cout << "Full command/runtime/map-hook fixture passed\n";
}
