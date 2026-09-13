// Offline storage/lookup doubles. Faction and DataMap implementations are native source.
#pragma once
#include "DataMap.h"
#include "NativeFaction.h"
#include <map>

constexpr uint32 UNIT_FLAG_NON_ATTACKABLE = 0x2, UNIT_FLAG_NOT_ATTACKABLE_1 = 0x80,
    UNIT_FLAG_NON_ATTACKABLE_2 = 0x10000, UNIT_FLAG_NOT_SELECTABLE = 0x2000000;
class Unit;
class GameObject;
class WorldObject
{
public:
    virtual ~WorldObject() = default;
    DataMap CustomData;
    virtual Unit const* ToUnit() const { return nullptr; }
    virtual GameObject const* ToGameObject() const { return nullptr; }
};
class GameObject : public WorldObject
{
public:
    ObjectGuid owner;
    GameObject const* ToGameObject() const override { return this; }
    ObjectGuid GetOwnerGUID() const { return owner; }
};
namespace ObjectAccessor
{
    inline std::map<ObjectGuid, WorldObject*> objects;
    inline WorldObject* GetWorldObject(WorldObject const&, ObjectGuid guid)
    {
        auto it = objects.find(guid);
        return it == objects.end() ? nullptr : it->second;
    }
}
struct FactionEntry { int reputationListID = -1; };
struct FactionStore
{
    std::map<uint32, FactionEntry> entries;
    FactionEntry const* LookupEntry(uint32 id) const
    {
        auto it = entries.find(id);
        return it == entries.end() ? nullptr : &it->second;
    }
};
inline FactionStore sFactionStore;
class TempSummon;
