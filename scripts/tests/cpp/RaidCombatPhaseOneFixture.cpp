/* API1 adapter tests intentionally isolate the legacy snapshot; API2 collector has its own tests. */
#include "CthunPolicyRuntime.h"
class Map;
class PlayerbotAI;
namespace RaidCombat
{
void Collect(Map*, CthunPolicy::Snapshot&) { }
bool SafeBoundary(Map*) { return true; }
bool UsesCombatPolicy(PlayerbotAI&) { return false; }
}
