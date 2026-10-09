#pragma once
#include <cassert>
#include <cstdint>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>
using uint8 = std::uint8_t;
using uint16 = std::uint16_t;
using uint32 = std::uint32_t;
constexpr int SEC_GAMEMASTER = 2;
constexpr uint16 ALLMAPHOOK_ON_DESTROY_MAP = 7;
struct Map
{
    bool raid = true;
    uint32 id = 550, instance = 6510;
    bool IsRaid() const { return raid; }
    uint32 GetId() const { return id; }
    uint32 GetInstanceId() const { return instance; }
};
struct Unit
{
    Map* map = nullptr;
    std::string name = "Arinerica";
    float health = 14;
    Map* GetMap() const { return map; }
    std::string GetName() const { return name; }
    float GetHealthPct() const { return health; }
};
struct Guid
{
    std::uint64_t value = 815;
    std::uint64_t GetRawValue() const { return value; }
};
struct Player : Unit { Guid GetGUID() const { return {}; } };
struct WorldSession
{
    Player* player;
    Player* GetPlayer() { return player; }
};
struct ChatHandler
{
    WorldSession* session;
    std::vector<std::string> messages;
    WorldSession* GetSession() { return session; }
    void SendSysMessage(char const* text) { messages.emplace_back(text); }
    template<class... Args> void PSendSysMessage(char const* text, Args const&... args)
    {
        std::ostringstream out;
        out << text;
        ((out << " [" << args << "]"), ...);
        messages.push_back(out.str());
    }
    bool Contains(std::string const& text) const
    {
        for (auto const& message : messages)
            if (message.find(text) != std::string::npos) return true;
        return false;
    }
};
namespace Acore::ChatCommands
{
    enum class Console { No, Yes };
    struct Entry
    {
        char const* name;
        bool (*handler)(ChatHandler*, char const*);
        int security;
        Console console;
    };
    using ChatCommandTable = std::vector<Entry>;
}
struct CommandScript
{
    inline static CommandScript* registered = nullptr;
    explicit CommandScript(char const*) { registered = this; }
    virtual ~CommandScript() = default;
    virtual Acore::ChatCommands::ChatCommandTable GetCommands() const = 0;
};
struct AllMapScript
{
    inline static AllMapScript* registered = nullptr;
    std::vector<uint16> hooks;
    AllMapScript(char const*, std::vector<uint16> enabled) : hooks(enabled) { registered = this; }
    virtual ~AllMapScript() = default;
    virtual void OnDestroyMap(Map*) { }
};
struct Config
{
    uint32 boss = 70;
    bool enabled = true;
    template<class T> T GetOption(char const*, T)
    {
        if constexpr (std::is_same_v<T, bool>) return enabled;
        return boss;
    }
};
inline Config config;
inline Config* sConfigMgr = &config;
