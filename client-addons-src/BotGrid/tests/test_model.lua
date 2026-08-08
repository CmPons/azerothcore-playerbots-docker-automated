return function(H)
  local NS = H.load("Comm.lua")  -- Model depends on Comm helpers
  NS = H.load("Model.lua")
  local M = NS.Model
  local C = NS.Comm

  -- specOf(classToken, t1, t2, t3) -> { spec=<name>, role="tank"|"heal"|"dps" }
  -- Warrior: tree3 = Protection = tank
  local w = M.specOf("WARRIOR", 3, 5, 61)
  H.eq(w.role, "tank", "prot warrior is tank")
  H.eq(w.spec, "Protection", "warrior spec name")

  -- Warrior Fury (tree2) = dps
  H.eq(M.specOf("WARRIOR", 3, 57, 0).role, "dps", "fury warrior dps")

  -- Priest Holy (tree2) and Discipline (tree1) = heal; Shadow (tree3) = dps
  H.eq(M.specOf("PRIEST", 14, 57, 0).role, "heal", "holy priest heal")
  H.eq(M.specOf("PRIEST", 57, 14, 0).role, "heal", "disc priest heal")
  H.eq(M.specOf("PRIEST", 0, 13, 58).role, "dps", "shadow priest dps")

  -- Paladin Holy (tree1) heal, Prot (tree2) tank, Ret (tree3) dps
  H.eq(M.specOf("PALADIN", 53, 10, 8).role, "heal", "holy paladin heal")
  H.eq(M.specOf("PALADIN", 8, 53, 10).role, "tank", "prot paladin tank")
  H.eq(M.specOf("PALADIN", 8, 5, 58).role, "dps", "ret paladin dps")

  -- Druid Feral (tree2) defaults tank in this model; Resto (tree3) heal; Balance (tree1) dps
  H.eq(M.specOf("DRUID", 57, 11, 3).role, "dps", "balance druid dps")
  H.eq(M.specOf("DRUID", 0, 55, 16).role, "tank", "feral druid -> tank default")
  H.eq(M.specOf("DRUID", 0, 14, 57).role, "heal", "resto druid heal")

  -- Death Knight Blood (tree1) tank; Frost (tree2)/Unholy (tree3) dps
  H.eq(M.specOf("DEATHKNIGHT", 53, 10, 8).role, "tank", "blood dk tank")
  H.eq(M.specOf("DEATHKNIGHT", 8, 53, 10).role, "dps", "frost dk dps")

  -- Pure dps classes always dps regardless of tree
  H.eq(M.specOf("MAGE", 50, 0, 21).role, "dps", "mage dps")
  H.eq(M.specOf("HUNTER", 0, 51, 20).role, "dps", "hunter dps")
  H.eq(M.specOf("ROGUE", 51, 0, 20).role, "dps", "rogue dps")
  H.eq(M.specOf("WARLOCK", 0, 21, 50).role, "dps", "warlock dps")

  -- No talents yet -> role "dps" sentinel + loading flag
  local none = M.specOf("WARRIOR", 0, 0, 0)
  H.eq(none.loading, true, "no talents -> loading")

  -- assignedRoleFrom(combatString): whole-token match on the strategy list
  H.eq(M.assignedRoleFrom("tank"), "tank", "assigned tank")
  H.eq(M.assignedRoleFrom("tank,guard,close"), "tank", "assigned tank in csv")
  H.eq(M.assignedRoleFrom("heal"), "heal", "assigned heal")
  H.eq(M.assignedRoleFrom("dps,assist"), "dps", "assigned dps")
  H.eq(M.assignedRoleFrom("close,ranged"), nil, "assigned unknown -> nil")
  -- Real bridge strategy strings (from live debug): only exact role tokens count
  H.eq(M.assignedRoleFrom("pull, pull back, tank, tank assist, tank face, default"),
    "tank", "real tank strategy list")
  H.eq(M.assignedRoleFrom("aoe, default, enh, healing stream, windfury, dps assist"),
    nil, "enh shaman: healing stream/dps assist are NOT role tokens")
  H.eq(M.assignedRoleFrom("resto, save mana, tranquility, dps assist"),
    nil, "resto druid: resto/tranquility/dps assist are NOT role tokens")
  H.eq(M.assignedRoleFrom("default, heal, save mana, dps assist"),
    "heal", "explicit heal strategy wins")
  H.eq(M.assignedRoleFrom("default, dps, dps assist, duel"),
    "dps", "explicit dps strategy token")

  -- buildRecords merges roster + details + states, classifies effective role,
  -- and flags mismatch (assigned present and != spec role).
  local roster = {
    { name="Tankbob", classId=1, alive=true, hpPct=100, mpPct=0 },
    { name="Healcat", classId=5, alive=true, hpPct=80,  mpPct=60 },
  }
  local details = {
    Tankbob = { talent1=3, talent2=5, talent3=61, score=5000, classToken="WARRIOR" },
    Healcat = { talent1=57, talent2=14, talent3=0, score=4200, classToken="PRIEST" },
  }
  local states = {
    Tankbob = { combat="tank" },
    Healcat = { combat="dps" },  -- healer told to dps -> mismatch
  }
  local recs = M.buildRecords(roster, details, states)
  H.eq(recs.Tankbob.role, "tank", "tankbob effective role")
  H.eq(recs.Tankbob.mismatch, false, "tankbob no mismatch")
  H.eq(recs.Healcat.specRole, "heal", "healcat spec role")
  H.eq(recs.Healcat.role, "dps", "healcat effective = assigned dps")
  H.eq(recs.Healcat.mismatch, true, "healcat mismatch flagged")

  -- bucketByRole returns ordered name lists per column
  local buckets = M.bucketByRole(recs)
  H.eq(buckets.tank[1], "Tankbob", "tank bucket")
  H.eq(buckets.dps[1], "Healcat", "dps bucket (effective)")
  H.eq(#buckets.heal, 0, "heal bucket empty")

  -- DK regression: className "Death Knight" must normalize and classify via classId
  local dkRoster = { { name="Dethly", classId=6, alive=true, hpPct=100, mpPct=50 } }
  local dkDetails = { Dethly = C.parseDetail("Dethly~Orc~0~Death Knight~80~53~10~8~5000") }
  local dkRecs = M.buildRecords(dkRoster, dkDetails, {})
  H.eq(dkRecs.Dethly.classToken, "DEATHKNIGHT", "DK token normalized")
  H.eq(dkRecs.Dethly.role, "tank", "blood DK -> tank")
end
