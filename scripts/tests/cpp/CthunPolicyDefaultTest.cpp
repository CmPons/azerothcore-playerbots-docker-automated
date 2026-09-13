// Installed-default transactions with real pinned Lua and production Scope. No live files.
#include "CthunPolicyScope.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <sys/stat.h>
using namespace CthunPolicy;
namespace fs = std::filesystem;
int main(int argc, char** argv)
{
    assert(argc == 2);
    fs::path root(argv[1]);
    for (char const* name : {"defaults", "revisions", "requests", "status"})
        fs::create_directories(root / name);
    unsigned nonce = 0;
    auto unique = [&]()
    {
        std::ostringstream out;
        out << std::hex << std::setw(16) << std::setfill('0') << ++nonce;
        return out.str();
    };
    auto bytes = [&](std::string const& source)
    {
        std::string digest = Digest(source);
        std::ofstream(root / "revisions" / (digest + ".lua")) << source;
        return digest;
    };
    auto install = [&](std::string const& digest)
    {
        std::string token = unique();
        std::ofstream(root / "defaults/raid.tmp") << "1 1 " << token << ' ' << digest << '\n';
        fs::rename(root / "defaults/raid.tmp", root / "defaults/raid.txt");
        return token + ":" + digest;
    };
    auto request = [&](Scope& scope, std::string const& digest, std::string const& expected,
                       std::string const& publication)
    {
        std::ofstream(root / "requests" / (scope.id + ".txt"))
            << unique() << ' ' << expected << ' ' << digest << ' ' << publication << '\n';
        scope.Poll();
    };
    Snapshot snapshot;
    snapshot.count = 1;
    snapshot.members[0].count = 2;
    snapshot.members[0].eligible = true;
    std::string good = bytes("return {api=1,plan=function(s) return {1} end}");
    std::string other = bytes("return {api=1,plan=function(s) return {0} end}");
    Scope a("531-1-1", root.string(), (root / "status").string());
    Scope b("531-2-2", root.string(), (root / "status").string());
    a.Poll(); a.Update(snapshot, true, 0);
    assert(!a.active && !a.defaultError.empty() && a.queued.empty());
    {
        Scope observed("531-3-3", root.string(), (root / "status").string());
        auto status = [&]()
        {
            observed.Status();
            std::ifstream file(root / "status" / (observed.id + ".json"));
            std::string text((std::istreambuf_iterator<char>(file)), {});
            assert(text.size() < 4096); // The host parser's existing byte budget.
            return text;
        };
        install(good);
        request(observed, other, "native", ""); // Same Poll observes default AND rejects legacy request.
        observed.Update(snapshot, true, 100); // Production writes Status only AFTER this safe adoption.
        assert(observed.active && observed.revision == good && observed.error.empty());
        assert(status().find("\"diagnostic_error\":\"diagnostic default publication mismatch\"") != std::string::npos);
        install(other); // Automatic-only adoption must not erase that otherwise unobservable rejection.
        observed.Poll(); observed.Update(snapshot, true, 200);
        assert(observed.revision == other && observed.error.empty());
        assert(status().find("\"diagnostic_error\":\"diagnostic default publication mismatch\"") != std::string::npos);
        request(observed, good, "native", observed.defaultPublication);
        assert(status().find("\"diagnostic_error\":\"expected revision mismatch\"") != std::string::npos);
        request(observed, good, other, observed.defaultPublication); // A new accepted diagnostic clears it.
        assert(status().find("\"diagnostic_error\":\"\"") != std::string::npos);
        observed.Update(snapshot, true, 300);
        assert(observed.revision == good && observed.diagnosticOverride);
        assert(status().find("\"diagnostic_error\":\"\"") != std::string::npos);
        install(other); observed.Poll(); observed.Update(snapshot, true, 400);
        assert(observed.revision == other && !observed.diagnosticOverride && observed.error.empty());
        assert(status().find("\"diagnostic_error\":\"diagnostic expired: default publication changed\"") !=
               std::string::npos);
    }
    std::string publication = install(good);
    a.Poll(); b.Poll();
    a.Update(snapshot, false, 250);
    assert(!a.active && a.desired == good && a.queued == good);
    a.Update(snapshot, true, 500);
    b.Update(snapshot, true, 500);
    assert(a.active && b.active && a.revision == good && b.revision == good);
    auto generation = a.generation;
    a.Poll(); a.Update(snapshot, true, 750);
    assert(a.generation == generation);
    // A diagnostic is a bounded override, not a permanent pin; legacy cannot override defaults.
    request(a, other, good, "");
    assert(a.error == "diagnostic default publication mismatch");
    request(a, other, good, publication);
    a.Update(snapshot, true, 1000);
    assert(a.revision == other && a.diagnosticOverride && b.revision == good);
    a.Poll(); a.Update(snapshot, true, 1250);
    assert(a.revision == other && a.desired == other);
    fs::remove(root / "defaults/raid.txt");
    a.Poll(); a.Update(snapshot, true, 1300);
    assert(a.revision == other && !a.diagnosticOverride && a.desired.empty());
    std::ofstream(root / "defaults/raid.txt") << "1 1 " << publication.substr(0, 16) << ' ' << good << '\n';
    a.Poll(); a.Update(snapshot, true, 1350);
    assert(a.revision == good && a.activeSource == "default"); // Successful default was not a failed attempt.
    request(a, other, good, publication);
    a.Update(snapshot, true, 1400);
    assert(a.diagnosticOverride && a.revision == other);
    publication = install(good); // Same bytes, new publication expires diagnostic override.
    a.Poll(); a.Update(snapshot, false, 1500);
    assert(!a.diagnosticOverride && a.revision == other && a.queued == good);
    a.Update(snapshot, true, 1750);
    assert(a.revision == good && a.activeSource == "default");
    request(a, other, good, publication);
    a.Update(snapshot, false, 2000);
    install(good);
    a.Update(snapshot, true, 2250); // Revalidation catches supersession WITHOUT intervening Poll.
    assert(a.revision == good && a.queued.empty() && !a.diagnosticOverride);
    a.Poll(); a.Update(snapshot, true, 2500);
    assert(a.revision == good); // Expired request is not replayed.
    // Disappearance cancels pending work, retains last-good; reappearance can queue afresh.
    publication = install(other);
    a.Poll();
    fs::remove(root / "defaults/raid.txt");
    a.Update(snapshot, true, 2750);
    assert(a.revision == good && a.queued.empty() && a.desired.empty() && !a.defaultError.empty());
    std::ofstream(root / "defaults/raid.txt") << "1 1 " << publication.substr(0, 16) << ' ' << other << '\n';
    a.Poll(); a.Update(snapshot, true, 3000);
    assert(a.revision == other);
    // Malformed and oversized manifest cannot replace an existing active VM.
    for (std::string const& invalid : {std::string("2 1 bad bad"), std::string(257, 'x')})
    {
        std::ofstream(root / "defaults/raid.txt") << invalid;
        a.Poll(); a.Update(snapshot, true, 3250);
        assert(a.revision == other && !a.defaultError.empty());
    }
    fs::remove(root / "defaults/raid.txt");
    fs::create_symlink(root / "revisions" / (good + ".lua"), root / "defaults/raid.txt");
    a.Poll(); assert(!a.defaultError.empty());
    fs::remove(root / "defaults/raid.txt");
    assert(mkfifo((root / "defaults/raid.txt").c_str(), 0600) == 0);
    a.Poll(); assert(!a.defaultError.empty());
    fs::remove(root / "defaults/raid.txt");
    // Missing source is retryable at30s, not every safe update; only bytes are read before retry.
    fs::remove(root / "revisions" / (good + ".lua"));
    publication = install(good);
    a.Poll(); a.Update(snapshot, true, 4000);
    assert(a.revision == other && a.queued == good && a.error.find("30s") != std::string::npos);
    bytes("return {api=1,plan=function(s) return {1} end}");
    for (uint32_t now = 4250; now < 34000; now += 250)
    {
        a.Poll(); a.Update(snapshot, true, now);
        assert(a.revision == other);
    }
    a.Update(snapshot, false, 34000);
    assert(a.revision == other);
    a.Update(snapshot, true, 34250);
    assert(a.revision == good);
    // Hash/oversize/forbidden and Lua errors are suppressed for unchanged publication.
    for (std::string const& invalid : {std::string("wrong hash"), std::string(MAX_SOURCE + 1, 'x')})
    {
        install(other);
        std::ofstream(root / "revisions" / (other + ".lua")) << invalid;
        a.Poll(); a.Update(snapshot, true, 35000);
        assert(a.revision == good && a.queued.empty());
        std::string error = a.error;
        bytes("return {api=1,plan=function(s) return {0} end}");
        a.Poll(); a.Update(snapshot, true, 70000);
        assert(a.revision == good && a.error == error); // Repair in place is not publication.
    }
    for (std::string const& invalid : {std::string("return os.execute('false')"),
            std::string("while true do end"), std::string("local t={} for i=1,100000 do t[i]={i,i,i,i} end")})
    {
        install(bytes(invalid)); a.Poll(); a.Update(snapshot, true, 70500);
        assert(a.revision == good && a.error.find("rejected") != std::string::npos);
    }
    for (bool fifo : {false, true})
    {
        fs::remove(root / "revisions" / (other + ".lua"));
        if (fifo)
            assert(mkfifo((root / "revisions" / (other + ".lua")).c_str(), 0600) == 0);
        else
            fs::create_symlink(root / "revisions" / (good + ".lua"), root / "revisions" / (other + ".lua"));
        install(other); a.Poll(); a.Update(snapshot, true, 70750);
        assert(a.revision == good && a.queued.empty() && !a.error.empty());
    }
    fs::remove(root / "revisions" / (other + ".lua"));
    bytes("return {api=1,plan=function(s) return {0} end}");
    std::string broken = bytes("broken!");
    install(broken); a.Poll(); a.Update(snapshot, true, 71000);
    assert(a.revision == good && a.error.find("rejected") != std::string::npos);
    for (unsigned i = 0; i < 100; ++i)
    {
        a.Poll(); a.Update(snapshot, true, 72000 + i * 1000);
        assert(a.revision == good && a.queued.empty());
    }
    // Failed diagnostics do not suppress production following.
    publication = install(other);
    a.Poll();
    request(a, broken, good, publication);
    a.Update(snapshot, true, 180000);
    assert(a.revision == good && !a.diagnosticOverride);
    a.Poll(); a.Update(snapshot, true, 181000);
    assert(a.revision == other && a.activeSource == "default");
    // Active fault quarantine cannot be cleared by nonce churn or another rejected candidate.
    std::string fault = bytes("local n=0 return {api=1,plan=function(s) n=n+1 "
                              "if n>1 then while true do end end return {0} end}");
    publication = install(fault); a.Poll(); a.Update(snapshot, true, 182000);
    a.Update(snapshot, false, 182250);
    assert(!a.active && a.faultedRevision == fault && !a.faultError.empty());
    generation = a.generation;
    for (unsigned i = 0; i < 3; ++i)
    {
        publication = install(fault); a.Poll(); a.Update(snapshot, true, 183000 + i * 1000);
        assert(!a.active && a.generation == generation && a.queued.empty());
    }
    install(broken); a.Poll(); a.Update(snapshot, true, 187000);
    assert(a.faultedRevision == fault);
    publication = install(fault); a.Poll();
    request(a, fault, "native", publication);
    assert(a.error.find("quarantined") != std::string::npos);
    install(other); a.Poll(); a.Update(snapshot, true, 188000);
    assert(a.active && a.revision == other && a.faultedRevision.empty());
    // Pending default can recover even if the old VM faults before its safe adoption boundary.
    install(fault); a.Poll(); a.Update(snapshot, true, 189000);
    install(good); a.Poll(); a.Update(snapshot, false, 189250);
    assert(!a.active && a.queued == good);
    a.Update(snapshot, true, 189500);
    assert(a.active && a.revision == good);
    // CAS expected-active is checked again after the old active VM faults.
    publication = install(fault); a.Poll(); a.Update(snapshot, true, 190000);
    request(a, other, fault, publication);
    a.Update(snapshot, false, 190250);
    assert(!a.active && a.queued == other && a.desired == other && a.desiredSource == "diagnostic");
    a.Update(snapshot, true, 190500);
    assert(!a.active && a.error == "queued expected revision mismatch");
    assert(a.queued.empty() && a.desired == fault && a.desiredSource == "default");
    a.Status();
    std::ifstream status(root / "status" / (a.id + ".json"));
    std::string text((std::istreambuf_iterator<char>(status)), {});
    assert(text.find("\"faulted_revision\":\"" + fault) != std::string::npos);
    assert(text.find("\"active_source\":\"native\"") != std::string::npos);
    // Scope destruction/recreation inherits installed default, not another generation's CAS.
    install(good);
    {
        Scope recreated("531-1-999", root.string(), (root / "status").string());
        recreated.Poll(); recreated.Update(snapshot, true, 191000);
        assert(recreated.active && recreated.revision == good && !recreated.diagnosticOverride);
    }
    assert(b.active && b.revision == good); // Independent VM throughout all first-scope faults.
    std::cout << "default lifecycle precedence/failure/quarantine/recreation passed\n";
}
