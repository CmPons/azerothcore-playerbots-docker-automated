// Production functions against API doubles; does not emulate pathfinding, spell damage or combat AI ticks.
#include "Aq40Helpers.h"
#include "Aq40Actions.h"
#include "Aq40Multipliers.h"
#include "RaidThreatUtils.h"
#include "ThreatStrategy.h"
#include <iostream>

class ThreatValue : public Action
{
public:
    using Action::Action;
    uint8 Calculate(Unit* target);
};
/* THREAT_VALUE */

class IsTargetOfHealingSpell {};
class PartyMemberToHeal : public Action
{
public:
    using Action::Action;
    Unit* Calculate();
    bool Check(Unit*);
    bool IsTargetOfSpellCast(Player*, IsTargetOfHealingSpell&) { return false; }
};
/* HEAL_CALCULATE */
/* HEAL_CHECK */

using namespace TempleOfAhnQirajHelpers;

struct Raid
{
    Map map;
    Group group;
    Creature vn,vl,bug;
    std::deque<Player> members;
    std::vector<std::unique_ptr<PlayerbotAI>> ais;
    Player *ari,*bel,*mage,*mel,*ail,*keil,*hunter,*red,*rogue,*shaman;

    Player* Add(uint32 guid,std::string name,int cls,bool tank=false,bool healer=false,bool automatic=true)
    {
        members.emplace_back();
        Player* p=&members.back();p->guid=guid;p->name=name;p->cls=cls;p->tankSpec=tank;
        p->healer=healer;p->melee=tank || cls==CLASS_ROGUE || cls==CLASS_SHAMAN;
        p->map=&map;p->group=&group;group.Add(p);
        if(automatic) ais.emplace_back(std::make_unique<PlayerbotAI>(p));
        return p;
    }

    Raid()
    {
        static uint32 nextInstance=1;
        map.instance=nextInstance++;
        vn.guid=50001;vl.guid=50002;bug.guid=50003;
        vn.name="Veknilash";vl.name="Veklor";bug.name="Mutated bug";
        vn.map=vl.map=bug.map=&map;
        vn.Relocate(0,0);vl.Relocate(95,0);vn.home.Relocate(0,0);vl.home.Relocate(95,0);
        bug.Relocate(90,5);bug.boss=false;
        map.script.creatures[AQT_DATA_VEKNILASH]=&vn;map.script.creatures[AQT_DATA_VEKLOR]=&vl;
        // Deliberately add members out of GUID order. Mage GUID is lower than warlock's.
        red=Add(1501,"Redshift",CLASS_WARRIOR,true,false,false);
        mage=Add(1118,"Raney",CLASS_MAGE);
        ail=Add(1180,"Ailina",CLASS_DRUID,false,true);
        bel=Add(1159,"Beliona",CLASS_WARLOCK);
        keil=Add(1315,"Keilmere",CLASS_PRIEST,false,true);
        ari=Add(142,"Arinerica",CLASS_PALADIN,true);
        mel=Add(815,"Meliah",CLASS_PRIEST,false,true);
        hunter=Add(1433,"Feelesia",CLASS_HUNTER);
        rogue=Add(1274,"Pilbok",CLASS_ROGUE);
        shaman=Add(1297,"Kaaren",CLASS_SHAMAN);
        ari->Relocate(-5,0);red->Relocate(-5,3);bel->Relocate(72,0);mage->Relocate(70,10);
        mel->Relocate(-20,10);ail->Relocate(72,20);keil->Relocate(-20,-10);
        hunter->Relocate(-25,0);rogue->Relocate(5,0);shaman->Relocate(5,3);
        vn.victim=ari;vl.victim=bel;
        for(auto& ai:ais)
        {
            ai->master=red;
            ai->ctx.GetValue<Unit*>("main tank")->Set(red);
            ai->ctx.GetValue<Unit*>("current target")->Set(GetTwinsAssignedTwin(ai->bot,ai.get()));
        }
    }
    void Fresh(){clockMs+=600;}
};

namespace legacy
{
    TwinsRole GetTwinsRole(Player*, PlayerbotAI*);
    bool GetTwinsTankSpot(Player*, PlayerbotAI*, Position&);
    bool GetTwinsHealerSpot(Player*, PlayerbotAI*, Position&);
}
namespace legacyThreat
{
    bool ShouldHoldDamageOnTauntImmuneBoss(PlayerbotAI*, Unit*, uint8);
}

void ReproduceLegacyFailures()
{
    Raid r;Position spot;
    assert(legacy::GetTwinsRole(r.mage,r.mage->ai)==TwinsRole::WarlockTank);
    assert(GetTwinsRole(r.mage,r.mage->ai)==TwinsRole::Dps);
    r.vl.victim=r.ari;
    assert(legacyThreat::ShouldHoldDamageOnTauntImmuneBoss(r.bel->ai,&r.vl,70));
    assert(!ai::threat::ShouldHoldDamageOnTauntImmuneBoss(r.bel->ai,&r.vl,70));
    r.vl.victim=r.bel;r.vl.tm.threat[r.bel]=1000;
    assert(legacyThreat::ShouldHoldDamageOnTauntImmuneBoss(r.bel->ai,&r.vl,70));
    assert(!ai::threat::ShouldHoldDamageOnTauntImmuneBoss(r.bel->ai,&r.vl,70));
    r.bel->Relocate(0,0);
    assert(!legacy::GetTwinsTankSpot(r.bel,r.bel->ai,spot));
    assert(GetTwinsTankSpot(r.bel,r.bel->ai,spot));
    assert(legacy::GetTwinsHealerSpot(r.mel,r.mel->ai,spot));
    assert(!GetTwinsHealerSpot(r.mel,r.mel->ai,spot));
}

void RolesAndFallbacks()
{
    Raid r;
    assert(GetTwinsTank(r.ari,AQT_DATA_VEKNILASH)==r.ari);
    assert(GetTwinsTank(r.bel,AQT_DATA_VEKLOR)==r.bel);
    assert(GetTwinsRole(r.bel,r.bel->ai)==TwinsRole::WarlockTank);
    assert(GetTwinsRole(r.mage,r.mage->ai)==TwinsRole::Dps);
    assert(GetTwinsAssignedData(r.hunter,r.hunter->ai)==AQT_DATA_VEKNILASH);
    assert(GetTwinsAssignedData(r.shaman,r.shaman->ai)==AQT_DATA_VEKNILASH);
    assert(GetTwinsAssignedData(r.rogue,r.rogue->ai)==AQT_DATA_VEKNILASH);
    assert(GetTwinsAssignedData(r.mage,r.mage->ai)==AQT_DATA_VEKLOR);
    r.bel->alive=false;
    assert(GetTwinsTank(r.mage,AQT_DATA_VEKLOR)==r.mage);
    assert(GetTwinsRole(r.mage,r.mage->ai)==TwinsRole::WarlockTank);
    r.bel->alive=true;r.bel->phase=2;
    assert(GetTwinsTank(r.mage,AQT_DATA_VEKLOR)==r.mage);
    r.bel->phase=1;r.bel->inWorld=false;
    assert(GetTwinsTank(r.mage,AQT_DATA_VEKLOR)==r.mage);
    r.bel->inWorld=true;
    Map elsewhere;r.bel->map=&elsewhere;
    assert(GetTwinsTank(r.mage,AQT_DATA_VEKLOR)==r.mage);
    r.bel->map=&r.map;r.bel->group=nullptr;
    assert(GetTwinsTank(r.mage,AQT_DATA_VEKLOR)==r.mage);
    r.bel->group=&r.group;
    Player* spare=r.Add(999,"Spare",CLASS_WARRIOR,true);
    assert(GetTwinsRole(spare,spare->ai)==TwinsRole::PhysicalReserve);
    r.ari->alive=false;
    assert(GetTwinsTank(r.mage,AQT_DATA_VEKNILASH)==spare);
    spare->alive=false;
    assert(GetTwinsTank(r.mage,AQT_DATA_VEKNILASH)==r.red);
    r.red->alive=false;
    assert(GetTwinsTank(r.mage,AQT_DATA_VEKNILASH)==nullptr);
    r.group.raid=false;
    assert(GetTwinsTank(r.bel,AQT_DATA_VEKLOR)==nullptr);
}

void ThreatOwnership()
{
    Raid r;
    r.vl.victim=r.ari;
    // Assigned caster opens without human threat, and does not stop when it gains aggro.
    assert(!ai::threat::ShouldHoldDamageOnTauntImmuneBoss(r.bel->ai,&r.vl,70));
    assert(ai::threat::ShouldHoldDamageOnTauntImmuneBoss(r.mage->ai,&r.vl,70));
    r.vl.victim=r.bel;r.vl.tm.threat[r.bel]=1000;
    assert(!ai::threat::ShouldHoldDamageOnTauntImmuneBoss(r.bel->ai,&r.vl,70));
    r.vl.tm.threat[r.mage]=699;
    assert(!ai::threat::ShouldHoldDamageOnTauntImmuneBoss(r.mage->ai,&r.vl,70));
    r.vl.tm.threat[r.mage]=700;
    assert(ai::threat::ShouldHoldDamageOnTauntImmuneBoss(r.mage->ai,&r.vl,70));
    assert(IsTwinsAssignedTank(r.bel,&r.vl));
    assert(!IsTwinsAssignedTank(r.ari,&r.vl));
    assert(IsTwinsAssignedTank(r.ari,&r.vn));
    assert(!ai::threat::ShouldHoldDamageOnTauntImmuneBoss(r.ari->ai,&r.vn,70));
    Player* spare=r.Add(999,"Spare",CLASS_WARRIOR,true);
    r.vn.tm.threat[r.ari]=1000;r.vn.tm.threat[spare]=700;
    assert(ai::threat::ShouldHoldDamageOnTauntImmuneBoss(spare->ai,&r.vn,70));
    ThreatValue tv(r.bel->ai,"threat");
    assert(tv.Calculate(&r.vl)==0);
    ThreatValue mtv(r.mage->ai,"threat");
    assert(mtv.Calculate(&r.vl)==70);
    r.vl.tm.threat[r.mage]=100000;
    assert(mtv.Calculate(&r.vl)==255);
    r.vl.tm.threat[r.bel]=0;
    assert(mtv.Calculate(&r.vl)==100);
    // Legacy threat values deliberately report 100; encounter-aware multiplier must not reapply them.
    r.bel->ai->ctx.GetValue<uint8>("threat","current target")->Set(100);
    r.bel->ai->ctx.GetValue<uint8>("threat","aoe")->Set(100);
    ThreatMultiplier mult(r.bel->ai);
    AttackAction cast(r.bel->ai,"test cast");
    assert(mult.GetValue(&cast)==1.0f);
    // Heals carry AoE threat; they and friendly dispels/buffs must not be silenced by a DPS hold.
    r.ail->ai->ctx.GetValue<Unit*>("current target")->Set(&r.vn);
    r.ail->ai->ctx.GetValue<uint8>("threat","aoe")->Set(100);
    ThreatMultiplier healingThreat(r.ail->ai);
    CastHealingSpellAction healing(r.ail->ai,"heal");
    CastSpellAction dispel(r.ail->ai,"dispel");dispel.recipient=r.bel;
    assert(healingThreat.GetValue(&healing)==1);assert(healingThreat.GetValue(&dispel)==1);
    r.bel->alive=false;r.mage->alive=false;
    assert(ai::threat::ShouldHoldDamageOnTauntImmuneBoss(r.hunter->ai,&r.vl,70));

    // Outside this exact encounter/target, original generic tank exemptions and human MT remain.
    Creature other;other.map=&r.map;other.guid=51000;
    assert(!IsTwinsBossTarget(r.ari,&other));
    assert(!ai::threat::ShouldHoldDamageOnTauntImmuneBoss(r.ari->ai,&other,70));
    assert(ai::threat::ShouldHoldDamageOnTauntImmuneBoss(r.hunter->ai,&other,70));
    r.map.script.state=DONE;
    assert(!IsTwinsAssignedTank(r.ari,&r.vn));
    assert(!IsTwinsBossTarget(r.bel,&r.vl));
}

void IndependentMovementAndTargets()
{
    Raid r;
    for(Player* p:{r.ari,r.bel,r.mage,r.mel,r.ail,r.keil,r.hunter})
    {
        TwinEmperorsMovementMultiplier m(p->ai);
        FollowAction follow(p->ai,"follow");CombatFormationMoveAction formation(p->ai,"formation");
        FleeAction flee(p->ai,"flee");FleeToGroupLeaderAction leader(p->ai,"flee to group leader");
        ReachTargetAction reach(p->ai,"reach spell");CastHealingSpellAction heal(p->ai,"heal");
        MeleeAction hit(p->ai,"melee");Aq40TwinsClearArcaneAction arc(p->ai);
        assert(m.GetValue(&follow)==0);assert(m.GetValue(&formation)==0);
        assert(m.GetValue(&flee)==0);assert(m.GetValue(&leader)==0);
        FollowAction retreat(p->ai,"flee chat shortcut");assert(m.GetValue(&retreat)==1);
        assert(m.GetValue(&heal)==1);assert(m.GetValue(&hit)==1);assert(m.GetValue(&arc)==1);
        assert(m.GetValue(&reach)==(GetTwinsRole(p,p->ai)==TwinsRole::Dps?1.0f:0.0f));
    }
    r.red->Relocate(400,400);
    Position spot;
    assert(!GetTwinsHealerSpot(r.mel,r.mel->ai,spot));
    assert(!GetTwinsTankSpot(r.bel,r.bel->ai,spot));
    r.bug.auras.insert(SPELL_MUTATE_BUG);r.map.bugs.push_back(&r.bug);
    assert(GetTwinsAttackTarget(r.bel,r.bel->ai)==&r.vl);
    assert(GetTwinsAttackTarget(r.ari,r.ari->ai)==&r.vn);
    assert(GetTwinsAttackTarget(r.mage,r.mage->ai)==&r.bug);
    r.bug.Relocate(5,4);
    assert(GetTwinsAttackTarget(r.rogue,r.rogue->ai)==&r.bug);
    r.bug.Relocate(50,0); // visible but outside either camp's add-cleanup radius
    assert(GetTwinsAttackTarget(r.mage,r.mage->ai)==&r.vl);
    r.mage->victim=nullptr;
    assert(!TwinsShouldRetarget(r.mage,r.mage->ai));
    TwinEmperorsTargetHoldMultiplier hold(r.mage->ai);
    DpsAssistAction assist(r.mage->ai,"dps assist");PetAttackAction pet(r.mage->ai,"pet attack");
    assert(hold.GetValue(&assist)==0);assert(hold.GetValue(&pet)==0);
    r.map.script.state=DONE;
    TwinEmperorsMovementMultiplier move(r.mage->ai);
    FollowAction follow(r.mage->ai,"follow");
    assert(move.GetValue(&follow)==1);assert(hold.GetValue(&assist)==1);
}

void HealerCoverageAndTriage()
{
    Raid r;Position spot;
    assert(GetTwinsHealerTank(r.mel,r.mel->ai)==r.ari);
    assert(GetTwinsHealerTank(r.ail,r.ail->ai)==r.bel);
    assert(GetTwinsHealerTank(r.keil,r.keil->ai)==r.ari);
    r.mel->alive=false;
    assert(GetTwinsHealerTank(r.ail,r.ail->ai)==r.bel);
    assert(GetTwinsHealerTank(r.keil,r.keil->ai)==r.ari);
    r.mel->alive=true;r.ail->alive=false;
    assert(GetTwinsHealerTank(r.mel,r.mel->ai)==r.ari);
    assert(GetTwinsHealerTank(r.keil,r.keil->ai)==r.bel);
    r.ail->alive=true;
    r.vn.Relocate(10,0);r.Fresh();
    assert(!GetTwinsHealerSpot(r.mel,r.mel->ai,spot)); // boss motion alone doesn't move healer
    r.ari->Relocate(40,0);
    assert(GetTwinsHealerSpot(r.mel,r.mel->ai,spot));
    assert(spot.GetExactDist2d(r.ari)<=24.1f);
    assert(spot.GetExactDist2d(&r.vl)>=TWINS_ARCANE_CLEARANCE);
    r.ari->Relocate(-5,0);
    // Safe in-range movement settles immediately instead of completing an old follow command.
    r.mel->moving=true;
    Aq40TwinsHoldHealerSpotAction hold(r.mel->ai);
    assert(TwinsShouldHoldHealerSpot(r.mel,r.mel->ai));
    hold.Execute({});assert(!r.mel->moving);
    // In-range cast may finish while inside real range but beyond the movement buffer.
    r.mel->Relocate(-40,0);r.mel->casting=true;r.mel->ai->moves.clear();
    hold.Execute({});assert(r.mel->casting && r.mel->ai->moves.empty());

    PartyMemberToHeal choose(r.ail->ai,"heal");
    r.bel->hp=80;r.mage->hp=70;r.red->hp=5;r.red->Relocate(500,500);
    assert(choose.Calculate()==r.bel);
    r.mage->hp=10;
    assert(choose.Calculate()==r.mage); // urgent non-assigned player still wins
    r.mage->hp=100;r.bel->hp=100;
    assert(choose.Calculate()==nullptr); // no manufactured need to heal a full-health tank
    r.red->Relocate(73,18);r.red->hp=50;
    assert(choose.Calculate()==r.red); // nearby human off-tank is not excluded
    Map other;r.red->map=&other;
    assert(!choose.Check(r.red));r.red->map=&r.map;r.red->phase=2;
    assert(!choose.Check(r.red));r.red->phase=1;
    r.red->Relocate(130,20);assert(!choose.Check(r.red));
    r.map.script.state=DONE;
    assert(choose.Check(r.red)); // original up-to-two-heal-ranges behavior elsewhere
}

void TeleportHandoffAndSeparation()
{
    Raid r;Position spot;
    // At 8.5y, tank is outside native melee despite the old 5+4y positioning tolerance.
    r.ari->Relocate(-8.5f,0);
    assert(GetTwinsTankSpot(r.ari,r.ari->ai,spot));
    r.ari->Relocate(-5,0);
    r.vl.Relocate(40,0);r.vn.victim=r.red;r.Fresh();
    assert(!TwinsShouldFixSeparation(r.ari,r.ari->ai));
    r.vn.victim=r.ari;
    assert(TwinsShouldFixSeparation(r.ari,r.ari->ai));
    r.ari->moving=true;
    assert(!TwinsShouldMoveTank(r.ari,r.ari->ai)); // must not settle/cancel a separation move
    assert(GetTwinsRangedTankSpot(r.bel,&r.vl,spot));
    assert(spot.GetExactDist2d(&r.vn)>TWINS_SEAT_SEPARATION+45);

    r.ari->moving=false;r.vl.Relocate(95,0);r.Fresh();GetTwinsSnapshot(r.ari);
    r.vn.Relocate(95,0);r.vl.Relocate(0,0);r.red->Relocate(83,0);r.Fresh();
    r.vn.victim=r.red;r.vl.victim=r.ari;
    assert(TwinsShouldClearArcane(r.ari,r.ari->ai));
    assert(GetTwinsArcaneClearSpot(r.ari,spot));
    assert(spot.GetExactDist2d(&r.vl)>=TWINS_ARCANE_CLEARANCE);
    // Sideways exit permits a subsequent direct path toward the far melee emperor.
    assert(std::abs(spot.y)>=TWINS_ARCANE_CLEARANCE && std::abs(spot.x)<0.01f);
    r.ari->Relocate(spot.x,spot.y);
    assert(!GetTwinsTankSpot(r.ari,r.ari->ai,spot)); // holds incoming caster, doesn't drag it
    assert(GetTwinsTankSpot(r.bel,r.bel->ai,spot));
    assert(spot.GetExactDist2d(&r.vl)<=27);
    assert(GetTwinsTank(r.ari,AQT_DATA_VEKNILASH)==r.red);
    assert(GetTwinsRole(r.ari,r.ari->ai)==TwinsRole::PhysicalReserve);
    assert(GetTwinsHealerTank(r.keil,r.keil->ai)==r.ari); // floating healer covers incoming Shadow Bolts
    r.vl.victim=r.bel;
    assert(!GetTwinsTankSpot(r.ari,r.ari->ai,spot)); // Ari waits for the next incoming melee boss
    assert(GetTwinsAttackTarget(r.ari,r.ari->ai)==nullptr);
    TwinEmperorsTargetHoldMultiplier reserve(r.ari->ai);
    AttackAction hit(r.ari->ai,"melee");CastHealingSpellAction heal(r.ari->ai,"heal");
    CastSpellAction buff(r.ari->ai,"defensive buff");buff.recipient=r.ari;
    assert(reserve.GetValue(&hit)==0);assert(reserve.GetValue(&heal)==1);assert(reserve.GetValue(&buff)==1);
    // A second full teleport gives Ari the local melee emperor without a cross-room tank race.
    r.vn.Relocate(0,0);r.vl.Relocate(95,0);r.vn.victim=r.ari;r.vl.victim=r.red;r.Fresh();
    assert(GetTwinsTank(r.ari,AQT_DATA_VEKNILASH)==r.ari);
    assert(GetTwinsRole(r.ari,r.ari->ai)==TwinsRole::WarriorTank);
    assert(GetTwinsTankSpot(r.ari,r.ari->ai,spot));
    assert(spot.GetExactDist2d(&r.vn)<=TWINS_TANK_ENGAGE_RANGE+0.01f);
    // Cloth accidentally caught incoming melee: it holds rather than dragging him toward Vek'lor.
    r.vn.victim=r.bel;
    assert(!GetTwinsTankSpot(r.bel,r.bel->ai,spot));
    Aq40TwinsCasterTankAction cast(r.bel->ai);assert(!cast.isUseful());
}

void CastingPetsAndDiagnostics()
{
    Raid r;
    Aq40TwinsCasterTankAction tank(r.bel->ai);
    r.vl.victim=r.ari;
    assert(tank.Execute({}));assert(r.bel->ai->casts.back()=="searing pain");
    r.vl.victim=r.bel;
    assert(tank.Execute({}));assert(r.bel->ai->casts.back()=="shadow ward");
    assert(tank.Execute({}));assert(r.bel->ai->casts.back()=="searing pain");
    r.bel->ai->castAllowed=false;assert(!tank.Execute({}));
    r.bel->ai->castAllowed=true;r.bel->ai->known.erase("searing pain");assert(!tank.Execute({}));
    Aq40TwinsCasterTankAction dps(r.mage->ai);assert(!dps.Execute({}));
    r.bel->alive=false;assert(dps.Execute({}));assert(r.mage->ai->casts.back()=="frostbolt");
    r.bel->alive=true;
    Pet pet;pet.map=&r.map;pet.victim=&r.vn;pet.charm.attack=true;r.hunter->pet=&pet;
    r.vn.tm.threat[r.ari]=1000;r.vn.tm.threat[r.hunter]=700;
    Aq40TwinsDirectPetsAction pets(r.hunter->ai);
    assert(!pets.Execute({}));assert(pet.victim==nullptr && !pet.charm.attack);
    r.vn.tm.threat[r.hunter]=0;
    assert(pets.Execute({}));assert(pet.victim==&r.vn);
    Aq40TwinsStatusAction status(r.ail->ai);
    assert(status.Execute({}));
    assert(r.ail->ai->messages.back().find("caster tank=Beliona")!=std::string::npos);
    assert(r.ail->ai->messages.back().find("healing=Beliona")!=std::string::npos);
    r.map.script.state=DONE;assert(!tank.Execute({}));assert(!pets.Execute({}));
}

void HazardMovesAndCastInterruption()
{
    Raid r;Position spot;
    r.ari->Relocate(95,0);
    DynamicObject blizzard;blizzard.Relocate(95,20);r.vl.blizzard=&blizzard;
    assert(GetTwinsArcaneClearSpot(r.ari,spot));
    assert(spot.GetExactDist2d(&blizzard)>=TWINS_BLIZZARD_RADIUS+TWINS_STATION_TOLERANCE);
    assert(spot.GetExactDist2d(&r.vl)>=TWINS_ARCANE_CLEARANCE);
    r.ari->casting=true;r.ari->moving=true;
    auto& last=r.ari->ai->ctx.GetValue<LastMovement&>("last movement")->Get();
    last.priority=MovementPriority::MOVEMENT_COMBAT;last.lastMoveShort.Relocate(-100,0);
    Aq40TwinsClearArcaneAction escape(r.ari->ai);
    assert(escape.Execute({}));assert(!r.ari->casting);assert(r.ari->interrupts==1);
    assert(r.ari->moveStops>0 && !r.ari->ai->moves.empty());
    r.ari->ai->canMove=false;r.ari->casting=true;
    size_t const moves=r.ari->ai->moves.size();
    assert(!escape.Execute({}));assert(r.ari->casting && r.ari->ai->moves.size()==moves);
    // LOS-candidate search must not do sixteen separate spatial scans for the same bomb.
    r.mel->Relocate(-100,0);r.map.searches=0;
    GetTwinsHealerSpot(r.mel,r.mel->ai,spot);
    assert(r.map.searches<=2);
}

void CampTrackingAndIsolation()
{
    Raid first;Raid second;
    auto initial=GetTwinsSnapshot(first.ari);
    assert(initial.valid && !initial.meleeAtCasterHome);
    GetTwinsSnapshot(second.ari);
    first.vn.Relocate(95,0);first.vl.Relocate(0,0);first.Fresh();
    assert(GetTwinsSnapshot(first.ari).meleeAtCasterHome);
    assert(!GetTwinsSnapshot(second.ari).meleeAtCasterHome);
    assert(!initial.meleeAtCasterHome); // returned snapshot is an independent value
    // Normal drift across the room's midpoint is NOT another teleport/ownership flip.
    first.vn.Relocate(40,0);first.vl.Relocate(-40,0);first.Fresh();
    assert(GetTwinsSnapshot(first.ari).meleeAtCasterHome);
    first.vn.Relocate(35,0);first.vl.Relocate(-45,0);first.Fresh();
    assert(GetTwinsSnapshot(first.ari).meleeAtCasterHome);
    // DONE must invalidate immediately, even within the half-second cache TTL.
    first.map.script.state=DONE;
    assert(!GetTwinsSnapshot(first.ari).valid);
    assert(GetTwinsSnapshot(second.ari).valid);
    TwinsEraseTrackers(second.ari);
    assert(!twinsSnapshotByInstance.contains(second.map.instance));
}

int main()
{
    ReproduceLegacyFailures();RolesAndFallbacks();ThreatOwnership();IndependentMovementAndTargets();HealerCoverageAndTriage();
    TeleportHandoffAndSeparation();CastingPetsAndDiagnostics();HazardMovesAndCastInterruption();CampTrackingAndIsolation();
    std::cout<<"Twins production coordination regressions passed\n";
}
