local NS = _G.BotGridNS or {}
_G.BotGridNS = NS

local S = {}
NS.Spells = S

-- Each entry: { label, spell, category }. cmd is derived as "cast <spell>".
-- category is used to group the Cast submenu (utility / convenience).
-- Spells are WotLK 3.3.5a. Spec-gated entries set spec = "<SpecName>".
local CATALOG = {
  WARRIOR = {
    { label="Pummel", spell="Pummel", category="utility" },
  },
  ROGUE = {
    { label="Kick", spell="Kick", category="utility" },
    { label="Tricks of the Trade", spell="Tricks of the Trade", category="utility" },
  },
  HUNTER = {
    { label="Misdirection", spell="Misdirection", category="utility" },
  },
  DEATHKNIGHT = {
    { label="Mind Freeze", spell="Mind Freeze", category="utility" },
  },
  SHAMAN = {
    { label="Wind Shear", spell="Wind Shear", category="utility" },
    { label="Heroism", spell="Heroism", category="utility" },
    { label="Bloodlust", spell="Bloodlust", category="utility" },
    { label="Ancestral Spirit (rez)", spell="Ancestral Spirit", category="utility" },
  },
  MAGE = {
    { label="Counterspell", spell="Counterspell", category="utility" },
    { label="Conjure Refreshment", spell="Conjure Refreshment", category="convenience" },
    { label="Conjure Water", spell="Conjure Water", category="convenience" },
    { label="Conjure Food", spell="Conjure Food", category="convenience" },
    { label="Conjure Mana Gem", spell="Conjure Mana Gem", category="convenience" },
    { label="Portal: Stormwind", spell="Portal: Stormwind", category="convenience" },
    { label="Portal: Ironforge", spell="Portal: Ironforge", category="convenience" },
    { label="Portal: Darnassus", spell="Portal: Darnassus", category="convenience" },
    { label="Portal: Exodar", spell="Portal: Exodar", category="convenience" },
    { label="Portal: Theramore", spell="Portal: Theramore", category="convenience" },
    { label="Portal: Dalaran", spell="Portal: Dalaran", category="convenience" },
    { label="Teleport: Stormwind", spell="Teleport: Stormwind", category="convenience" },
    { label="Teleport: Dalaran", spell="Teleport: Dalaran", category="convenience" },
  },
  WARLOCK = {
    { label="Create Healthstone", spell="Create Healthstone", category="convenience" },
    { label="Ritual of Refreshment", spell="Ritual of Refreshment", category="convenience" },
    { label="Ritual of Souls", spell="Ritual of Souls", category="convenience" },
    { label="Soulstone (battle-rez)", spell="Create Soulstone", category="utility" },
  },
  PRIEST = {
    { label="Power Infusion", spell="Power Infusion", category="utility", spec="Discipline" },
    { label="Pain Suppression", spell="Pain Suppression", category="utility", spec="Discipline" },
    { label="Resurrection", spell="Resurrection", category="utility" },
    { label="Dispel Magic", spell="Dispel Magic", category="utility" },
  },
  PALADIN = {
    { label="Hand of Protection", spell="Hand of Protection", category="utility" },
    { label="Hand of Sacrifice", spell="Hand of Sacrifice", category="utility" },
    { label="Cleanse", spell="Cleanse", category="utility" },
    { label="Redemption (rez)", spell="Redemption", category="utility" },
  },
  DRUID = {
    { label="Innervate", spell="Innervate", category="utility" },
    { label="Rebirth (battle-rez)", spell="Rebirth", category="utility" },
    { label="Remove Curse", spell="Remove Curse", category="utility" },
    { label="Revive", spell="Revive", category="utility" },
  },
}
S.CATALOG = CATALOG

-- forClass(classToken, specName, isKnown) -> filtered list with .cmd added.
-- isKnown(spellName) decides whether the bot actually has the spell; entries
-- gated to a different spec are dropped.
function S.forClass(classToken, specName, isKnown)
  classToken = string.upper(classToken or "")
  local list = CATALOG[classToken]
  local out = {}
  if not list then return out end
  for _, e in ipairs(list) do
    local specOk = (e.spec == nil) or (e.spec == specName)
    if specOk and (not isKnown or isKnown(e.spell)) then
      out[#out + 1] = {
        label = e.label,
        spell = e.spell,
        category = e.category,
        cmd = "cast " .. e.spell,
      }
    end
  end
  return out
end
