#include "CthunPolicyRuntime.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
using namespace CthunPolicy;

int main(int argc, char** argv)
{
    Snapshot snapshot;
    snapshot.count = 1;
    snapshot.members[0].eligible = true;
    snapshot.members[0].count = 2;
    Plan plan;
    if (argc == 2)
    {
        std::ifstream input(argv[1]);
        std::ostringstream text;
        text << input.rdbuf();
        Runtime runtime;
        if (!runtime.Load(text.str()) || !runtime.Evaluate(snapshot, plan))
        {
            std::cerr << runtime.Error() << '\n';
            return 1;
        }
        std::cout << "checked API v1 with Lua 5.4.8\n";
        return 0;
    }
    for (std::string const source : {
        "return {api=1,plan=function(s) return {0} end}",
        "return {api=1,plan=function(s) return {1} end}"})
    {
        Runtime a, b;
        assert(a.Load(source));
        assert(b.Load("return {api=1,plan=function(s) return {-1} end}"));
        assert(a.Evaluate(snapshot, plan));
        assert(plan.choices[0] >= 0);
        assert(b.Evaluate(snapshot, plan));
        assert(plan.choices[0] == -1);
    }
    for (std::string const source : {
        "invalid syntax !", "return {api=2}", "return {}", "while true do end",
        "local t={} for i=1,99999999 do t[i]=i end",
        "return {api=1,plan=3}", "return os.execute('false')", "return io.open('x')",
        "return require('x')", "return load('return 1')()", "return debug.getregistry()",
        "return coroutine.create(function() end)", "return pcall(function() end)",
        "return setmetatable({}, {})"})
    {
        Runtime runtime;
        assert(!runtime.Load(source));
        assert(!runtime.Error().empty());
        assert(runtime.Memory() <= MEMORY_LIMIT);
    }
    for (std::string const body : {
        "while true do end", "local t={} for i=1,99999999 do t[i]=i end",
        "return {}", "return {2}", "return {0/0}", "return {math.huge}",
        "return {0,0}", "return {[1]=0, surprise=1}", "return {'0'}",
        "error({})", "local function f() return 1+f() end return f()"})
    {
        Runtime runtime;
        assert(runtime.Load("return {api=1,plan=function(s) " + body + " end}"));
        plan.choices[0] = 123;
        assert(!runtime.Evaluate(snapshot, plan));
        assert(plan.choices[0] == 123);
        assert(runtime.Memory() <= MEMORY_LIMIT);
    }
    // Real allocation exhaustion rather than instruction exhaustion: exponentially-sized strings
    // use only a few VM instructions per allocation, with no string library required.
    {
        Runtime runtime;
        assert(runtime.Load("return {api=1,plan=function(s) local x='xxxxxxxx' "
                            "for i=1,30 do x=x..x end return {0} end}"));
        assert(!runtime.Evaluate(snapshot, plan));
        assert(runtime.Error().find("memory") != std::string::npos);
    }
    {
        Runtime runtime;
        assert(!runtime.Load(std::string(MAX_SOURCE + 1, ' ')));
    }
    std::cout << "real Lua sandbox, budgets, isolated states and teardown passed\n";
}
