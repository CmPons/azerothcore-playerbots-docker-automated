#include "CthunPolicyScope.h"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace CthunPolicy;
namespace fs = std::filesystem;

int main(int argc, char** argv)
{
    assert(argc == 2);
    fs::path root(argv[1]);
    fs::create_directories(root / "revisions");
    fs::create_directories(root / "requests");
    fs::create_directories(root / "status");
    DataMap first, second;
    first.Set("policy", new Scope("531-1-1", root.string(), (root / "status").string()));
    second.Set("policy", new Scope("531-2-2", root.string(), (root / "status").string()));
    auto* a = first.Get<Scope>("policy");
    auto* b = second.Get<Scope>("policy");
    Snapshot snapshot;
    snapshot.count = 1;
    snapshot.members[0].count = 2;
    snapshot.members[0].eligible = true;
    unsigned nonce = 0;
    auto request = [&](Scope* scope, std::string const& source, std::string const& expected)
    {
        std::string const rev = Digest(source);
        std::ofstream(root / "revisions" / (rev + ".lua")) << source;
        std::ofstream(root / "requests" / (scope->id + ".txt")) << "000000000000000" << ++nonce
            << ' ' << expected << ' ' << rev << '\n';
        scope->Poll();
        return rev;
    };
    std::string const good = "return {api=1,plan=function(s) return {1} end}";
    std::string rev = request(a, good, "native");
    a->Update(snapshot, false, 100);
    assert(!a->active && a->queued == rev);
    a->Update(snapshot, true, 200);
    assert(a->active && a->revision == rev && a->Fresh(1199) && !a->Fresh(1200));
    assert(a->plan.choices[0] == 1 && !b->active);
    uint64_t const generation = a->generation;
    request(a, "broken!", rev);
    a->Update(snapshot, true, 300);
    assert(a->active && a->revision == rev && a->generation == generation);
    assert(a->error.find("rejected") != std::string::npos);
    request(a, good, "native");
    assert(a->error == "expected revision mismatch");
    std::string const other = "return {api=1,plan=function(s) return {0} end}";
    std::string alternate = request(a, other, rev);
    a->Update(snapshot, true, 400);
    assert(a->revision == alternate && a->plan.choices[0] == 0);
    request(a, good, alternate); // code-only rollback follows exactly the same transaction
    a->Update(snapshot, true, 500);
    assert(a->revision == rev && a->plan.choices[0] == 1);
    request(b, other, "native");
    b->Update(snapshot, true, 600);
    assert(b->active && b->revision == alternate && b->plan.choices[0] == 0);
    std::string const failsActive = "local n=0 return {api=1,plan=function(s) n=n+1 "
                                   "if n>1 then while true do end end return {0} end}";
    request(a, failsActive, rev);
    a->Update(snapshot, true, 700);
    assert(a->active);
    a->Update(snapshot, false, 800);
    assert(!a->active && a->revision == "native");
    std::string error = a->error;
    a->Update(snapshot, false, 900);
    assert(a->error == error); // quarantined, no repeated evaluation/error storm
    {
        Scope budget("531-budget", root.string(), (root / "status").string());
        Snapshot roster;
        roster.count = MAX_MEMBERS;
        for (std::size_t i = 0; i < roster.count; ++i)
        {
            auto& member = roster.members[i];
            std::snprintf(member.guid, sizeof(member.guid), "%zu", i);
            member.eligible = true; member.count = 1;
        }
        budget.Update(roster, false, 1000);
        for (std::size_t i = 0; i < roster.count; ++i)
        {
            unsigned granted = 0;
            while (budget.SpendPath(roster.members[i].guid)) ++granted;
            assert(granted >= 1 && granted <= 8); // early retry floods cannot starve the last member
        }
        assert(budget.paths == 64 && !budget.SpendPath("39"));
        budget.Update(roster, false, 1250);
        assert(budget.paths == 0 && budget.SpendPath("39"));
    }
    {
        Scope feedback("531-feedback", root.string(), (root / "status").string());
        Snapshot observed;
        observed.count = 1;
        auto& member = observed.members[0];
        std::snprintf(member.guid, sizeof(member.guid), "actor");
        member.eligible = true; member.count = 3;
        member.candidates[1].x = 3; member.candidates[2].x = 6;
        request(&feedback, "return {api=1,plan=function(s) local o={} for i,m in ipairs(s.members) do "
                "o[i]=#m.candidates>1 and 1 or 0 end return o end}", "native");
        feedback.Update(observed, true, 1000);
        feedback.RejectRoute("actor", member.candidates[0], member.candidates[1]);
        assert(feedback.plan.choices[0] == 0);
        feedback.Update(observed, false, 61000); // even slow actors must explore before retrying the best
        assert(feedback.snapshot.members[0].count == 2 && feedback.plan.choices[0] == 1);
        assert(feedback.snapshot.members[0].candidates[1].x == 6);
        feedback.RejectRoute("actor", member.candidates[0], member.candidates[2]);
        feedback.Update(observed, false, 61250);
        assert(feedback.plan.choices[0] == 0); // exhausted; aged failures can now be retried
        feedback.Update(observed, false, 61500);
        assert(feedback.snapshot.members[0].count == 3);
        feedback.RejectRoute("actor", member.candidates[0], member.candidates[1]);
        member.candidates[0].x = 1;
        feedback.Update(observed, false, 61750);
        assert(feedback.snapshot.members[0].count == 3); // changed origin forgets old route failures
        feedback.RejectRoute("actor", member.candidates[0], member.candidates[1]);
        observed.eye = true;
        feedback.Update(observed, false, 62000);
        assert(feedback.snapshot.members[0].count == 3); // changed phase also forgets
        feedback.RejectRoute("actor", member.candidates[0], member.candidates[1]);
        observed.count = 2; observed.members[1] = member;
        std::snprintf(observed.members[1].guid, sizeof(member.guid), "other");
        std::swap(observed.members[0], observed.members[1]);
        feedback.Update(observed, false, 62250);
        assert(feedback.snapshot.members[0].count == 3 && feedback.snapshot.members[1].count == 2);
        // Stationary actor, changing candidate pools: departed failures must not fill the bounded cache
        // and prevent learning the next blocked best endpoint once observations stabilize again.
        observed.count = 1;
        for (unsigned i = 0; i < 70; ++i)
        {
            auto& actor = observed.members[0];
            actor.candidates[1].x = 100 + i;
            feedback.Update(observed, false, 62500 + i * 500);
            feedback.RejectRoute(actor.guid, actor.candidates[0], actor.candidates[1]);
            feedback.Update(observed, false, 62750 + i * 500);
            assert(feedback.snapshot.members[0].count == 2);
            assert(feedback.snapshot.members[0].candidates[1].x == 6);
        }
    }
    first.Erase("policy");
    assert(b->active && b->Fresh(900));
    std::ifstream status(root / "status/531-1-1.json");
    std::string text((std::istreambuf_iterator<char>(status)), {});
    assert(text.find("\"destroyed\":true") != std::string::npos);
    std::cout << "real instance ownership/reload/CAS/rollback/fault/freshness/destruction passed\n";
}
