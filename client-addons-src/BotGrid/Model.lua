local NS = _G.BotGridNS or {}
_G.BotGridNS = NS

local M = {}
NS.Model = M

-- For each class, map the dominant talent tree index (1/2/3) to a {spec, role}.
-- Trees are in WoW's canonical order. "dps" classes map all trees to dps.
local SPECS = {
  WARRIOR  = { {"Arms","dps"},        {"Fury","dps"},     {"Protection","tank"} },
  PALADIN  = { {"Holy","heal"},       {"Protection","tank"}, {"Retribution","dps"} },
  HUNTER   = { {"Beast Mastery","dps"},{"Marksmanship","dps"},{"Survival","dps"} },
  ROGUE    = { {"Assassination","dps"},{"Combat","dps"},   {"Subtlety","dps"} },
  PRIEST   = { {"Discipline","heal"}, {"Holy","heal"},    {"Shadow","dps"} },
  DEATHKNIGHT = { {"Blood","tank"},   {"Frost","dps"},    {"Unholy","dps"} },
  SHAMAN   = { {"Elemental","dps"},   {"Enhancement","dps"},{"Restoration","heal"} },
  MAGE     = { {"Arcane","dps"},      {"Fire","dps"},     {"Frost","dps"} },
  WARLOCK  = { {"Affliction","dps"},  {"Demonology","dps"},{"Destruction","dps"} },
  DRUID    = { {"Balance","dps"},     {"Feral","tank"},   {"Restoration","heal"} },
}
M.SPECS = SPECS

-- Returns { spec=<name>, role="tank"|"heal"|"dps", tree=1..3, loading=bool }.
-- Note: Feral druid defaults to tank (bear); spec icon still distinguishes it.
function M.specOf(classToken, t1, t2, t3)
  classToken = string.upper(classToken or "")
  local trees = SPECS[classToken]
  t1, t2, t3 = tonumber(t1) or 0, tonumber(t2) or 0, tonumber(t3) or 0
  local total = t1 + t2 + t3
  if not trees then
    return { spec = "Unknown", role = "dps", tree = 0, loading = (total == 0) }
  end
  if total == 0 then
    return { spec = "Unknown", role = "dps", tree = 0, loading = true }
  end
  local tree = 1
  if t2 >= t1 and t2 >= t3 then tree = 2 end
  if t3 > t1 and t3 > t2 then tree = 3 end
  local entry = trees[tree]
  return { spec = entry[1], role = entry[2], tree = tree, loading = false }
end

-- Tolerant parse of the STATES combat string into a role, or nil if not stated.
-- The bridge's combat field is the bot's full active STRATEGY list (comma
-- separated), e.g. "avoid aoe, default, tank, tank assist, ...". Match only the
-- exact role strategies as whole tokens so noise like "healing stream" or
-- "dps assist" / "resto" doesn't false-trigger. Returns nil when no explicit
-- role strategy is set (caller then falls back to the talent spec role).
function M.assignedRoleFrom(combat)
  combat = string.lower(combat or "")
  if combat == "" then return nil end
  local tokens = {}
  for tok in string.gmatch(combat, "([^,]+)") do
    tokens[(string.gsub(tok, "^%s*(.-)%s*$", "%1"))] = true
  end
  if tokens["tank"] then return "tank" end
  if tokens["heal"] then return "heal" end
  if tokens["dps"] then return "dps" end
  return nil
end

local CLASS_TOKEN = (NS.Comm and NS.Comm.CLASS_TOKEN) or {}

-- Merge roster/details/states keyed by name into per-bot records.
function M.buildRecords(roster, details, states)
  local recs = {}
  for _, r in ipairs(roster or {}) do
    local d = (details or {})[r.name] or {}
    local s = (states or {})[r.name] or {}
    local classToken = CLASS_TOKEN[r.classId] or d.classToken or "UNKNOWN"
    local spec = M.specOf(classToken, d.talent1, d.talent2, d.talent3)
    local assigned = M.assignedRoleFrom(s.combat)
    local effective = assigned or spec.role
    recs[r.name] = {
      name = r.name,
      classToken = classToken,
      classId = r.classId,
      alive = r.alive,
      hpPct = r.hpPct,
      mpPct = r.mpPct,
      score = d.score or 0,
      spec = spec.spec,
      specTree = spec.tree,
      specRole = spec.role,
      assignedRole = assigned,
      role = effective,
      loading = spec.loading,
      mismatch = (assigned ~= nil and assigned ~= spec.role),
    }
  end
  return recs
end

-- Group records into ordered name lists per role column.
function M.bucketByRole(recs)
  local buckets = { tank = {}, heal = {}, dps = {} }
  local names = {}
  for name in pairs(recs) do names[#names + 1] = name end
  table.sort(names)
  for _, name in ipairs(names) do
    local role = recs[name].role
    if not buckets[role] then role = "dps" end
    buckets[role][#buckets[role] + 1] = name
  end
  return buckets
end
