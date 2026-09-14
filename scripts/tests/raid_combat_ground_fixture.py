"""Compile exact actual-root spline/MM/owned-stop bodies with explicit ground API doubles."""
from pathlib import Path


def execute(root, core, out, build, crypto, run, body):
    fixture = root / "scripts/tests/fixtures/raid-combat/GroundFixture.h"
    spline = core / "src/server/game/Movement/Spline"
    policy = core / "modules/mod-playerbots/src/Ai/Raid/Policy"
    (out / "Common.h").write_text("""#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
using uint8=uint8_t; using int8=int8_t; using uint16=uint16_t; using int16=int16_t;
using uint32=uint32_t; using int32=int32_t; using uint64=uint64_t; using int64=int64_t;
""")
    (out / "Errors.h").write_text("#pragma once\n#include <cstdlib>\n#define ASSERT(x) do { if (!(x)) std::abort(); } while(0)\n#define ABORT() std::abort()\n")
    (out / "ObjectGuid.h").write_text("""#pragma once
#include "Common.h"
struct ObjectGuid {
uint64 raw=0; constexpr ObjectGuid()=default; constexpr ObjectGuid(uint64 n):raw(n) {}
explicit operator bool() const {return raw!=0;} bool operator==(ObjectGuid const&) const=default;
uint64 GetRawValue() const {return raw;} uint64 WriteAsPacked() const {return raw;}
std::string ToString() const {return std::to_string(raw);}
};
""")
    (out / "Log.h").write_text("#pragma once\n#define LOG_ERROR(...) ((void)0)\n#define LOG_DEBUG(...) ((void)0)\n#define LOG_TRACE(...) ((void)0)\n")
    for name in "Map ModelIgnoreFlags MotionMaster MovementGenerator ObjectAccessor PathGenerator Player Transport Vehicle Unit Creature Opcodes WorldPacket MovementPacketBuilder".split():
        (out / (name + ".h")).write_text(f'#include "{fixture}"\n')
    sources = []
    for name in ("MoveSpline.cpp", "MoveSplineInit.cpp", "Spline.cpp", "MovementUtil.cpp"):
        path = out / name
        path.write_bytes((spline / name).read_bytes())
        sources.append(path)
    fragments = ["namespace RaidCombat {\n" + body(policy / "RaidCombatPolicy.cpp",
        "bool UsesCombatPolicy(PlayerbotAI& ai)") + "\n}"]
    (out / "RecordCthunFollow.inc").write_text(body(core / "modules/mod-playerbots/src/Ai/Base/Actions/MovementActions.cpp",
        "void MovementAction::RecordCthunFollow()"))
    for signature in ("static void CalculatePassengerPosition(", "static void CalculatePassengerOffset("):
        fragment = body(core / "src/server/game/Entities/Vehicle/VehicleDefines.h", signature)
        fragments.append(fragment.replace("static void Calculate", "void TransportBase::Calculate", 1)
                         .replace("float* o = nullptr", "float* o"))
    motion = core / "src/server/game/Movement/MotionMaster.cpp"
    for signature in ("bool MotionMaster::InstallCheckedMovement(", "bool MotionMaster::ExpireOwnedMovement(",
                      "void MotionMaster::Mutate(", "void MotionMaster::DirectExpireSlot(",
                      "void MotionMaster::InitTop(", "void MotionMaster::DirectDelete(",
                      "void MotionMaster::DelayedDelete("):
        fragments.append(body(motion, signature))
    fragments.append(body(core / "src/server/game/Movement/MovementGenerator.cpp", "MovementGenerator::MovementGenerator()"))
    for signature in ("bool Unit::StopOwnedSpline(", "void Unit::StopMoving()"):
        fragments.append(body(core / "src/server/game/Entities/Unit/Unit.cpp", signature))
    native = out / "native-ground.cpp"
    native.write_text(f'#include "{fixture}"\n#include "MoveSplineInit.h"\n#include "Log.h"\n' + "\n\n".join(fragments))
    sources.append(native)
    adapter = (policy / "RaidCombatGround.cpp").read_text()
    (out / "RaidCombatGround.inc").write_text("\n".join(
        line for line in adapter.splitlines() if not line.startswith("#include")))
    g3d = core / "deps/g3dlite"
    flags = ["g++", "-std=gnu++20", "-DMOD_PLAYERBOTS", "-O1", "-g", "-DNDEBUG", "-Wall", "-Wextra",
             "-Werror", "-Wno-unused-parameter", "-Wno-deprecated-copy", "-ffunction-sections", "-fdata-sections",
             "-fsanitize=undefined", "-fno-sanitize-recover=all", "-I" + str(out),
             "-I" + str(fixture.parent), "-I" + str(spline), "-I" + str(policy),
             "-I" + str(core / "src/common/Utilities"), "-I" + str(g3d / "include"),
             "-I" + str(core / "src/common"), "-I" + str(core / "deps/fmt/include")]
    objects = []
    for source in sources + [g3d / "source" / (name + ".cpp") for name in
                             ("Matrix4", "Matrix3", "Vector3", "Vector4", "Ray", "AABox", "g3dmath")]:
        obj = out / (source.stem + ".o")
        run(flags + ["-c", source, "-o", obj])
        objects.append(obj)
    binary = out / "ground-test"
    run(flags + [root / "scripts/tests/cpp/RaidCombatGroundTest.cpp", *objects, build / "libpolicy-core.a",
                 build / "lua/libplayerbot_lua.a", crypto, "-Wl,--gc-sections", "-o", binary])
    return run([binary]).stdout
