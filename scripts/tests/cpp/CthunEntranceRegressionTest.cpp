// This same assertion must fail original0031: hard spread blocks a constrained entry route.
#include "Aq40Cthun.h"
#include <iostream>
using namespace CthunPositioning;
int main()
{
    Map map; Group group; Creature eye;
    eye.map = &map; eye.guid = 10001; eye.reach = 15;
    eye.Relocate(-8578.79f, 1986.18f, 100.304f);
    map.script.creatures[DATA_EYE] = &eye;
    Player human, bot, blocker;
    human.guid = 1; bot.guid = 2; blocker.guid = 3;
    for (Player* p : {&human, &bot, &blocker}) {p->map = &map; p->group = &group; group.Add(p);}
    PlayerbotAI humanAI(&human), ai(&bot), blockerAI(&blocker);
    humanAI.real = true; ai.master = blockerAI.master = &human;
    human.Relocate(-8625, 1968, 100.713f);
    bot.Relocate(-8640, 1968, 100.713f);
    blocker.Relocate(-8630, 1968, 100.713f);
    ai.ctx.GetValue<Unit*>("current target")->Set(&eye);
    map.pathCheck = [](float x, float y, float) {return x >= -8640 && y >= 1966 && y <= 1970;};
    Position destination;
    assert(FindPosition(&bot, &ai, destination));
    assert(destination.x > bot.x); // inward progress despite a temporarily linked group ahead
    std::cout << "constrained entrance no-exterior-hold regression passed\n";
}
