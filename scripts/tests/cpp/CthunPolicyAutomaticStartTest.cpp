// Same assertion against deployed0033 and current scope; real pinned Lua, no publisher request.
#include "CthunPolicyScope.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
int main(int argc, char** argv)
{
    assert(argc == 2);
    std::filesystem::path root(argv[1]);
    std::filesystem::create_directories(root / "defaults");
    std::filesystem::create_directories(root / "revisions");
    std::string const source = "return {api=1,plan=function(s) return {} end}";
    std::string const revision = CthunPolicy::Digest(source);
    std::ofstream(root / "revisions" / (revision + ".lua")) << source;
    std::ofstream(root / "defaults/raid.txt") << "1 1 0123456789abcdef " << revision << '\n';
    CthunPolicy::Scope scope("531-1-1", root.string(), root.string());
    scope.Poll();
    scope.Update({}, true, 1000);
    assert(scope.active && scope.revision == revision);
    std::cout << "automatic start without generation request passed\n";
}
