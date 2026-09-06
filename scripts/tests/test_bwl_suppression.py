"""Offline regression checks; no server, database connection, or build required.

Exercise the shipped SQL in SQLite and replay the core's GO bounding-box test
against a captured spawn fixture. The C++ guard check is a source contract,
not an execution test of GameObjectAI. See Documents/bwl-suppression-bug.md.
"""

import json
import math
from pathlib import Path
import sqlite3
import unittest

ROOT = Path(__file__).resolve().parents[2]
PATCH = ROOT / "patches/0018-core-bwl-suppression-despawn.patch"
SQL_PATH = "data/sql/updates/pending_db_world/rev_1788721200000000000.sql"


def added_file(path):
    """Extract the actual new-file payload shipped in our setup patch."""
    section = PATCH.read_text().split(f"+++ b/{path}\n", 1)[1]
    section = section.split("diff --git ", 1)[0]
    return "\n".join(line[1:] for line in section.splitlines()
                     if line.startswith("+") and not line.startswith("+++")) + "\n"


def in_explosion_range(device, fixture):
    """Replay GameObject::IsInRange3d (rotated box, not spherical distance)."""
    _, x, y, z, orientation, scale = device
    cx, cy, cz = fixture["example_caster_position"]
    dx, dy, dz = cx - x, cy - y, cz - z
    if math.isclose(math.hypot(dx, dy), 0, abs_tol=1e-6):
        return True
    dx, dy = (math.cos(orientation) * dy + math.sin(orientation) * dx,
              math.cos(orientation) * dx - math.sin(orientation) * dy)
    bounds, radius = fixture["model_bounds"], fixture["radius"]
    return all(lo * scale - radius < value < hi * scale + radius
               for value, lo, hi in zip((dx, dy, dz), bounds[:3], bounds[3:]))


class SuppressionRegressionTests(unittest.TestCase):
    def setUp(self):
        self.db = sqlite3.connect(":memory:")
        self.addCleanup(self.db.close)
        self.db.executescript("""
            CREATE TABLE conditions (
                SourceTypeOrReferenceId INTEGER, SourceGroup INTEGER,
                SourceEntry INTEGER, SourceId INTEGER, ElseGroup INTEGER,
                ConditionTypeOrReference INTEGER, ConditionTarget INTEGER,
                ConditionValue1 INTEGER, ConditionValue2 INTEGER,
                ConditionValue3 INTEGER, NegativeCondition INTEGER,
                ErrorType INTEGER, ErrorTextId INTEGER, ScriptName TEXT, Comment TEXT
            );
            INSERT INTO conditions VALUES
                (13,1,20038,0,0,31,0,4,0,0,0,0,0,'','Explosion targets players'),
                (13,1,19873,0,0,31,0,5,177807,0,0,0,0,'','Destroy Egg target'),
                (13,2,99999,0,0,31,0,5,179784,0,0,0,0,'','Unrelated spell');
        """)
        self.sql = added_file(SQL_PATH)
        self.fixture = json.loads((ROOT / "scripts/tests/fixtures/bwl_suppression_spawns.json").read_text())

    def allowed(self, effect_mask, type_id, entry):
        # This migration intentionally uses one positive OBJECT_ENTRY_GUID
        # condition per effect. Mirror its type/entry predicate, not all of
        # ConditionMgr's more general ElseGroup/reference semantics.
        rows = self.db.execute("""
            SELECT ConditionTypeOrReference, ConditionTarget, ConditionValue1,
                   ConditionValue2, ConditionValue3, NegativeCondition, ElseGroup
            FROM conditions WHERE SourceTypeOrReferenceId=13 AND SourceEntry=20038
            AND SourceId=0 AND (SourceGroup & ?) != 0
        """, (effect_mask,)).fetchall()
        if not rows:
            return True  # Current bug: no GO entry restriction on effect 1.
        self.assertEqual(len(rows), 1)
        kind, target, wanted_type, wanted_entry, guid, negative, group = rows[0]
        self.assertEqual((kind, target, guid, negative, group), (31, 0, 0, 0, 0))
        return type_id == wanted_type and (wanted_entry == 0 or entry == wanted_entry)

    def test_reproduces_cross_room_targeting_then_excludes_all_devices(self):
        devices = self.fixture["devices"]
        self.assertEqual([row[0] for row in devices], list(range(75120, 75158)))
        self.assertTrue(all(in_explosion_range(row, self.fixture) for row in devices))
        self.assertTrue(self.allowed(2, 5, 179784))
        self.db.executescript(self.sql)
        self.assertFalse(self.allowed(2, 5, 179784))

    def test_eggs_remain_targets_but_doors_orb_and_other_objects_do_not(self):
        self.db.executescript(self.sql)
        self.assertTrue(self.allowed(2, 5, 177807))
        for entry in (179784, 176964, 179365, 180631, 180632, 177808):
            with self.subTest(entry=entry):
                self.assertFalse(self.allowed(2, 5, entry))
        self.assertFalse(self.allowed(2, 4, 177807))
        self.assertFalse(self.allowed(2, 3, 177807))

    def test_player_wipe_and_unrelated_conditions_unchanged(self):
        before = self.db.execute("SELECT * FROM conditions ORDER BY SourceEntry").fetchall()
        self.db.executescript(self.sql)
        after = self.db.execute("""SELECT * FROM conditions
            WHERE NOT (SourceEntry=20038 AND SourceGroup=2) ORDER BY SourceEntry""").fetchall()
        self.assertEqual(before, after)
        self.assertTrue(self.allowed(1, 4, 0))
        self.assertFalse(self.allowed(1, 3, 12435))
        self.assertFalse(self.allowed(1, 5, 179784))

    def test_migration_is_idempotent(self):
        self.db.executescript(self.sql)
        first = self.db.execute("SELECT * FROM conditions ORDER BY SourceEntry,SourceGroup").fetchall()
        self.db.executescript(self.sql)
        self.assertEqual(first, self.db.execute(
            "SELECT * FROM conditions ORDER BY SourceEntry,SourceGroup").fetchall())

    def test_cast_guard_is_local_and_does_not_stop_respawn_event_updates(self):
        patch = PATCH.read_text()
        self.assertIn("+                    if (me->isSpawned() && me->GetGoState() == GO_STATE_READY)", patch)
        # No early return in UpdateAI: scheduled reset events must keep running.
        added = "\n".join(line for line in patch.splitlines()
                          if line.startswith("+") and not line.startswith("+++"))
        self.assertNotIn("return", added)
        self.assertIn("me->CastSpell(nullptr, SPELL_SUPPRESSION_AURA);", patch)

    def test_optional_working_tree_matches_shipped_sql(self):
        source = ROOT / "azerothcore-wotlk" / SQL_PATH
        if not source.exists():
            self.skipTest("Core checkout is optional for root-only tests")
        self.assertEqual(source.read_text(), self.sql)


if __name__ == "__main__":
    unittest.main()
