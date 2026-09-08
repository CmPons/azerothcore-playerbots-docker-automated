#include "PBChatterWorld.h"
#include "PBChatterConfig.h"
#include "PBChatterMemory.h"
#include "PBChatterPersona.h"
#include "PBChatterQueue.h"
#include "PBChatterAmbient.h"
#include "PBChatterChannelPolicy.h"
#include "Group.h"
#include "PBChatterEvents.h"
#include "PBChatterAreaBackfill.h"
#include "Playerbots.h"
#include "Player.h"
#include "ObjectAccessor.h"
#include "ObjectGuid.h"

void PBChatterWorld::OnAfterConfigLoad(bool /*reload*/)
{
    PBChatterLoadConfig();
}

void PBChatterWorld::OnStartup()
{
    if (g_PBChatEnable)
    {
        PBChatterMemory::LoadAllFromDB();
        PBChatterPersona::Initialize();
    }
    // Resolve area/zone names for any new creature entries on every startup —
    // idempotent (PK + INSERT IGNORE + skip-already-resolved), throttled across
    // ticks, and silent when the table is already full. `.chatter backfillareas`
    // still works for manual re-runs after content updates.
    PBChatterAreaBackfill::Start();
}

void PBChatterWorld::OnUpdate(uint32 diff)
{
    PBChatterAreaBackfill::Tick(diff);   // auto on startup + operator-triggered; no-op when idle

    if (!g_PBChatEnable)
        return;

    PBChatterAmbient::Tick(diff);
    PBChatterEvents::Tick(diff);

    for (PBChatResult const& r : PBChatterQueue::DrainResults())
    {
        Player* bot = ObjectAccessor::FindPlayer(
            ObjectGuid::Create<HighGuid::Player>(static_cast<ObjectGuid::LowType>(r.botGuid)));
        if (!bot)
            continue;
        PlayerbotAI* ai = GET_PLAYERBOT_AI(bot);
        if (!ai)
            continue;

        // Generated group chatter belongs to the original audience. Membership
        // can change while Pi is answering; drop stale output, never reroute it.
        if (r.ambient && r.ambientKind == AMB_GROUP)
        {
            Player* anchor = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(
                static_cast<ObjectGuid::LowType>(r.anchorPlayerGuid)));
            Group* group = bot->GetGroup();
            Group* anchorGroup = anchor ? anchor->GetGroup() : nullptr;
            PlayerbotAI* anchorAI = anchor ? GET_PLAYERBOT_AI(anchor) : nullptr;
            if (!bot->IsInWorld() || !anchor || !anchor->IsInWorld() ||
                (anchorAI && !anchorAI->IsRealPlayer()) ||
                !PBChatterChannelPolicy::SameGroup(r.ambientIdent,
                    group ? group->GetGUID().GetRawValue() : 0,
                    anchorGroup ? anchorGroup->GetGUID().GetRawValue() : 0))
                continue;
            PBChatChannel expectedChannel = group->isRaidGroup() ? PBChatChannel::Raid : PBChatChannel::Party;
            if (r.channel != expectedChannel)
                continue;
        }

        bool sent = false;
        switch (r.channel)
        {
            case PBChatChannel::Whisper: ai->Whisper(r.reply, r.playerName); sent = true; break;
            case PBChatChannel::Say:     ai->Say(r.reply);                   sent = true; break;
            case PBChatChannel::Party:   sent = ai->SayToParty(r.reply);                  break;
            case PBChatChannel::Raid:    sent = ai->SayToRaid(r.reply);                   break;
            case PBChatChannel::General: sent = ai->SayToChannel(r.reply, ChatChannelId::GENERAL); break;
            case PBChatChannel::Guild:   sent = ai->SayToGuild(r.reply);                  break;
        }

        if (r.ambient && sent)
            PBChatterAmbient::OnBotLineDispatched(r.ambientKind, r.ambientIdent,
                                                  r.botGuid, bot->GetName(), r.reply);
    }

    // Periodic flush (every 5 minutes).
    _saveTimer += diff;
    if (_saveTimer >= 300000)
    {
        _saveTimer = 0;
        PBChatterMemory::FlushToDB();
    }
}

void PBChatterWorld::OnShutdown()
{
    if (g_PBChatEnable)
        PBChatterMemory::FlushToDB();
}
