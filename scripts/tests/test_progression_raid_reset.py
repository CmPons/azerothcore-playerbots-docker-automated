"""Offline policy/source/setup checks. Never touches live saves or services."""
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
CORE = ROOT / "azerothcore-wotlk"
INSTANCES = CORE / "src/server/game/Instances"
SQL = CORE / "data/sql/updates/pending_db_characters/rev_1788894000000000000.sql"
PATCH = ROOT / "patches/0019-core-progression-raid-resets.patch"


def statement(name):
    source = (CORE / "src/server/database/Database/Implementation/CharacterDatabase.cpp").read_text()
    block = source.split("PrepareStatement(" + name + ",", 1)[1].split(");", 1)[0]
    return "".join(json.loads(s) for s in re.findall(r'"(?:\\.|[^"\\])*"', block))


class ProgressionRaidResetTests(unittest.TestCase):
    def test_cpp_policy(self):
        compiler = shutil.which("g++") or shutil.which("clang++")
        if not compiler:
            self.skipTest("C++20 compiler unavailable")
        with tempfile.TemporaryDirectory(prefix="raid-reset-policy-") as tmp:
            binary = str(Path(tmp) / "policy-test")
            subprocess.run([compiler, "-std=c++20", "-Wall", "-Wextra", "-Werror",
                            "-I" + str(INSTANCES),
                            str(ROOT / "scripts/tests/cpp/ProgressionRaidResetTest.cpp"),
                            "-o", binary], check=True)
            subprocess.run([binary], check=True, timeout=10)

    def test_progress_is_atomic_with_saved_boss_states(self):
        code = (INSTANCES / "InstanceScript.cpp").read_text().split(
            "void InstanceScript::SaveToDB()", 1)[1].split("void InstanceScript::OnPlayerEnter", 1)[0]
        self.assertIn("transaction->Append(stmt)", code)
        self.assertIn("save->UpdateProgressionReset(data, transaction)", code)
        self.assertIn("CharacterDatabase.CommitTransaction(transaction)", code)
        self.assertNotIn("GetCompletedEncounterMask", code)
        self.assertNotIn("CharacterDatabase.Execute(stmt)", code)
        upsert = statement("CHAR_REP_INSTANCE_PROGRESSION_RESET")
        self.assertIn("stage=GREATEST(stage,VALUES(stage))", upsert)
        self.assertIn("VALUES(resetTime)>=resetTime", upsert)

    def test_global_resets_and_warnings_skip_managed_saves(self):
        code = (INSTANCES / "InstanceSaveMgr.cpp").read_text().split(
            "void InstanceSaveMgr::_ResetOrWarnAll", 1)[1].split(
            "InstancePlayerBind* InstanceSaveMgr::PlayerBindToInstance", 1)[0]
        self.assertIn("!itr2->second->UsesProgressionReset()", code)
        self.assertIn("save && save->UsesProgressionReset())\n            continue;", code)

    def test_expiration_and_extension_lifecycle(self):
        code = (INSTANCES / "InstanceSaveMgr.cpp").read_text()
        load = code.split("void InstanceSaveMgr::LoadInstances()", 1)[1].split(
            "void InstanceSaveMgr::LoadResetTimes()", 1)[0]
        self.assertLess(load.index("LoadCharacterBinds();"), load.index("UpdateProgressionResets("))
        expiry = code.split("bool InstanceSaveMgr::UpdateProgressionResets", 1)[1].split(
            "void InstanceSaveMgr::_ResetSave", 1)[0]
        self.assertIn("std::vector<uint32> ids", expiry)
        self.assertIn("while (InstanceSave* save = GetInstanceSave(id))", expiry)
        self.assertIn("script->IsEncounterInProgress()", expiry)
        self.assertIn("player->IsInCombat()", expiry)
        self.assertLess(expiry.index("if (inCombat)"), expiry.index("_ResetSave(itr)"))
        self.assertIn("retained ? &retained->m_playerList : nullptr", expiry)
        self.assertIn("CharacterDatabase.DirectCommitTransaction(transaction)", code)
        self.assertIn("save->SetResetTime(save->GetExtendedResetTime())", code)
        self.assertIn("CHAR_DEL_INSTANCE_PROGRESSION_RESET", code)
        # Map updates must be joined before the world-thread reset pass.
        world = (CORE / "src/server/game/World/World.cpp").read_text()
        self.assertLess(world.index("sMapMgr->Update(diff)"), world.index("sInstanceSaveMgr->Update()"))
        maps = (CORE / "src/server/game/Maps/MapMgr.cpp").read_text()
        self.assertIn("m_updater.wait()", maps)

    def test_entry_and_login_warnings_use_actual_copy(self):
        movement = (CORE / "src/server/game/Handlers/MovementHandler.cpp").read_text()
        self.assertIn("GetResetTimeFor(mEntry->MapID, diff, GetPlayer()->GetInstanceId())", movement)
        character = (CORE / "src/server/game/Handlers/CharacterHandler.cpp").read_text()
        self.assertEqual(character.count("pCurrChar->GetMap()->GetDifficulty(), pCurrChar->GetInstanceId())"), 2)

    def test_setup_is_isolated_idempotent_and_opt_in(self):
        source = (ROOT / "setup.sh").read_text()
        start = source.index("set_conf () {")
        helper = source[start:source.index("\n}\n", start) + 3]
        block = source.split("# BEGIN PROGRESSION RAID RESETS\n", 1)[1].split(
            "# END PROGRESSION RAID RESETS", 1)[0]
        with tempfile.TemporaryDirectory(prefix="raid-reset-config-") as tmp:
            config = Path(tmp) / "worldserver.conf"
            initial = "Instance.ResetTimeHour = 4\nRate.InstanceResetTime = 0.142857\n# keep\n"
            config.write_text(initial)
            env = os.environ.copy()
            env["WS_CONF"] = str(config)
            env.pop("RAID_PROGRESSION_RESET_ENABLE", None)
            env.pop("RAID_PROGRESSION_RESET_DAYS", None)
            for _ in range(2):
                subprocess.run(["bash", "-euc", helper + "\n" + block], env=env, check=True)
                self.assertEqual(config.read_text(), initial +
                                 "Instance.ProgressionReset.Enable = 0\nInstance.ProgressionReset.Days = 3\n")
            env["RAID_PROGRESSION_RESET_ENABLE"] = "1"
            subprocess.run(["bash", "-euc", helper + "\n" + block], env=env, check=True)
            self.assertEqual(config.read_text(), initial +
                             "Instance.ProgressionReset.Enable = 1\nInstance.ProgressionReset.Days = 3\n")

    def test_schema_is_additive_and_idempotent(self):
        sql = SQL.read_text()
        self.assertIn("CREATE TABLE IF NOT EXISTS `instance_progression_reset`", sql)
        self.assertNotRegex(sql, r"(?i)\b(DELETE|DROP|UPDATE|ALTER|TRUNCATE)\b")
        self.assertIn("ENGINE=InnoDB", sql)
        self.assertIn("LEFT JOIN instance_progression_reset", statement("CHAR_SEL_INSTANCE_SAVES_WITH_PROGRESSION"))

    @unittest.skipUnless(os.environ.get("AC_TEST_MYSQL_TEMP") == "1", "Opt-in connection-local MySQL test")
    def test_mysql_temporary_tables(self):
        # Shadow both table names on THIS connection only. No persistent DDL,
        # save edits, binds, respawn writes, or server commands are issued.
        ddl = SQL.read_text().replace("CREATE TABLE IF NOT EXISTS", "CREATE TEMPORARY TABLE IF NOT EXISTS")
        upsert = statement("CHAR_REP_INSTANCE_PROGRESSION_RESET")
        commands = [ddl, ddl, "CREATE TEMPORARY TABLE instance (id INT PRIMARY KEY, map INT, "
                    "resettime INT, difficulty INT, completedEncounters INT, data TEXT);",
                    "INSERT INTO instance VALUES (1,469,0,0,63,'B W L 3 3 3 3 3 3 0 0'),"
                    "(2,409,0,0,1023,'M C 3 3 3 3 3 3 3 3 3 3');"]
        def checkpoint(values):
            sql = upsert
            for value in values:
                sql = sql.replace("?", str(value), 1)
            commands.append(sql + ";")
        checkpoint((1, 1, 1000, 2000))
        checkpoint((1, 2, 3000, 6000))
        checkpoint((1, 1, 1000, 2000))  # stale worker must not undo progression
        commands.append("SELECT stage,resetTime,extendedResetTime FROM instance_progression_reset WHERE instanceId=1;")
        checkpoint((1, 3, 1500, 2500))  # clear may shorten the deadline
        checkpoint((1, 2, 3000, 6000))  # stale worker must not undo the clear
        commands.append("SELECT stage,resetTime,extendedResetTime FROM instance_progression_reset WHERE instanceId=1;")
        checkpoint((1, 3, 2500, 3500))  # consume an explicit extension
        checkpoint((1, 3, 1500, 2500))  # stale clear must not undo extension
        checkpoint((2, 2, 9000, 12000))
        commands.append("SELECT stage,resetTime,extendedResetTime FROM instance_progression_reset WHERE instanceId=1;")
        commands.append(statement("CHAR_DEL_INSTANCE_PROGRESSION_RESET").replace("?", "2") + ";")
        checkpoint((99, 2, 9000, 12000))  # orphan cleanup
        commands.append(statement("CHAR_SANITIZE_INSTANCE_PROGRESSION_RESET") + ";")
        commands.append("SELECT COUNT(*) FROM instance_progression_reset;")
        commands.append(statement("CHAR_SEL_INSTANCE_SAVES_WITH_PROGRESSION") + ";")
        result = subprocess.run(["bash", "-c", 'source .env; exec docker compose exec -T ac-database '
                                 'mysql -uroot -p"$DOCKER_DB_ROOT_PASSWORD" --batch --skip-column-names acore_characters'],
                                cwd=CORE, input="\n".join(commands), text=True,
                                capture_output=True, check=True, timeout=30)
        rows = result.stdout.splitlines()
        self.assertEqual(rows[:4], ["2\t3000\t6000", "3\t1500\t2500", "3\t2500\t3500", "1"])
        self.assertEqual(rows[4].split("\t")[-3:], ["3", "2500", "3500"])
        self.assertEqual(rows[5].split("\t")[-3:], ["0", "0", "0"])

    def test_patch_delivers_exact_core_changes(self):
        if not PATCH.exists():
            self.fail("Canonical core patch missing")
        # Reverse on a throwaway copy to recover baseline, then apply and compare.
        paths = re.findall(r"^diff --git a/(.+) b/", PATCH.read_text(), re.M)
        with tempfile.TemporaryDirectory(prefix="raid-reset-patch-") as tmp:
            work = Path(tmp)
            for path in paths:
                dest = work / path
                dest.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(CORE / path, dest)
            subprocess.run(["git", "apply", "--reverse", str(PATCH)], cwd=work, check=True)
            subprocess.run(["git", "apply", "--check", str(PATCH)], cwd=work, check=True)
            subprocess.run(["git", "apply", str(PATCH)], cwd=work, check=True)
            for path in paths:
                self.assertEqual((work / path).read_bytes(), (CORE / path).read_bytes(), path)


if __name__ == "__main__":
    unittest.main()
