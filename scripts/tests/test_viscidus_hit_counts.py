"""Offline arithmetic/source-contract tests; no live server or combat simulation."""
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
CORE = ROOT / "azerothcore-wotlk"
SOURCE = Path("src/server/scripts/Kalimdor/TempleOfAhnQiraj/boss_viscidus.cpp")
PATCH = ROOT / "patches/0023-core-viscidus-ten-player-hit-counts.patch"
ENUM = re.compile(r"enum HitCounter\n\{.*?\n\};", re.S)
ORIGINAL = {
    "SLOW": 100, "SLOW_MORE": 150, "FREEZE": 200,
    "CRACK": 50, "SHATTER": 100, "EXPLODE": 150,
}


def counters(source):
    return {name: int(value) for name, value in re.findall(
        r"HITCOUNTER_(\w+)\s*=\s*(\d+)", ENUM.search(source).group())}


def apply(temp, *options):
    result = subprocess.run(["git", "apply", *options, str(PATCH)], cwd=temp,
                            text=True, capture_output=True)
    if result.returncode:
        raise AssertionError(result.stdout + result.stderr)


class ViscidusHitCountTests(unittest.TestCase):
    def test_quarter_counts_with_positive_half_up_rounding(self):
        values = counters((CORE / SOURCE).read_text())
        self.assertEqual(values, {name: (value + 2) // 4 for name, value in ORIGINAL.items()})
        self.assertEqual(values, {
            "SLOW": 25, "SLOW_MORE": 38, "FREEZE": 50,
            "CRACK": 13, "SHATTER": 25, "EXPLODE": 38,
        })

    def test_intermediate_threshold_order_and_counter_width(self):
        values = counters((CORE / SOURCE).read_text())
        for names in (("SLOW", "SLOW_MORE", "FREEZE"), ("CRACK", "SHATTER", "EXPLODE")):
            stages = [values[name] for name in names]
            self.assertEqual(stages, sorted(set(stages)))
            self.assertGreater(stages[0], 0)
            self.assertLess(stages[-1], 256)  # Existing uint8 counter, unchanged.

    def test_hit_qualification_timer_and_finish_gate_preserved(self):
        source = (CORE / SOURCE).read_text()
        for contract in (
            "events.ScheduleEvent(EVENT_RESET_PHASE, 15s);",
            "if (me->HealthBelowPct(5))\n            damage = 0;",
            "if (me->GetHealthPct() <= 5.f)",
            "Unit::Kill(attacker, me);",
            "if (effType == DIRECT_DAMAGE)\n            ++_hitcounter;",
            "if ((spellSchoolMask & SPELL_SCHOOL_MASK_FROST) && _phase == PHASE_FROST)",
            "if (_hitcounter >= HITCOUNTER_FREEZE)",
            "attacker->HasUnitState(UNIT_STATE_MELEE_ATTACKING) && _hitcounter >= HITCOUNTER_EXPLODE",
            "else if (_hitcounter == HITCOUNTER_SLOW_MORE)",
            "else if (_hitcounter == HITCOUNTER_SLOW)",
            "else if (_hitcounter == HITCOUNTER_SHATTER)",
            "else if (_hitcounter == HITCOUNTER_CRACK)",
            "MAX_GLOB_SPAWN             = 20",
        ):
            self.assertIn(contract, source)

    def test_patch_roundtrip_changes_only_threshold_enum(self):
        current = (CORE / SOURCE).read_text()
        names = [line.split(" b/", 1)[1] for line in PATCH.read_text().splitlines()
                 if line.startswith("diff --git ")]
        self.assertEqual(names, [str(SOURCE)])
        with tempfile.TemporaryDirectory() as directory:
            temp = Path(directory)
            path = temp / SOURCE
            path.parent.mkdir(parents=True)
            path.write_text(current)
            apply(temp, "--reverse", "--check")
            apply(temp, "--reverse")
            original = path.read_text()
            self.assertEqual(counters(original), ORIGINAL)
            # All abilities, reset logic, globs, HP protection and timers byte-identical.
            self.assertEqual(ENUM.sub("ENUM", original), ENUM.sub("ENUM", current))
            apply(temp, "--check")
            apply(temp)
            self.assertEqual(path.read_bytes(), (CORE / SOURCE).read_bytes())


if __name__ == "__main__":
    unittest.main()
