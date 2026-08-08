local NS = _G.BotGridNS or {}
_G.BotGridNS = NS

local Menu = {}
NS.Menu = Menu

local menuFrame = CreateFrame("Frame", "BotGridMenuFrame", UIParent, "UIDropDownMenuTemplate")

local C = NS.Comm

-- Issue a combat command to a scope of bot names.
local function combat(names, cmd) C.run("COMBAT", names, cmd) end
local function position(names, cmd) C.run("POSITION", names, cmd) end
local function loot(names, cmd) C.run("LOOT", names, cmd) end
local function rti(names, cmd) C.run("RTI", names, cmd) end

-- Role change goes through mod-raid-roster (.raidroster syncone) so the bot gets a real
-- spec + gear for the role, not just a strategy flip. One command per selected bot; a full
-- `.raidroster sync` (Sync button) later reverts everyone to roster defaults.
local function setRole(names, role)
  for _, n in ipairs(names) do
    NS.RosterBar.Run(".raidroster syncone " .. n .. " " .. role)
  end
  if NS.RequestSoon then NS.RequestSoon() end  -- re-pull spec/state so cells re-sort
end

-- Promote a bot to raid Main Tank via the WoW raid role flag (MEMBER_FLAG_MAINTANK).
-- Playerbots' IsMainTank reads this flag; every other tank then becomes an off/assist
-- tank automatically (the flag is unique). Requires being in a raid and its leader.
local function setMainTank(names)
  setRole(names, "tank")  -- ensure it's a real tank first (spec + gear)
  for _, n in ipairs(names) do
    local u = NS.UnitForName and NS.UnitForName(n)
    if u and type(SetPartyAssignment) == "function" then
      SetPartyAssignment("MAINTANK", u)
    end
  end
end

local RTI_ICONS = {
  {"Skull", 8}, {"Cross", 7}, {"Square", 6}, {"Moon", 5},
  {"Triangle", 4}, {"Diamond", 3}, {"Circle", 2}, {"Star", 1},
}
local FORMATIONS = { "arrow", "line", "circle", "shield", "melee", "near", "queue", "chaos" }

local function item(text, func, sub)
  local t = { text = text, notCheckable = true }
  if sub then t.hasArrow = true; t.menuList = sub else t.func = func end
  return t
end

-- Casting a bot spell goes through a WHISPER ("cast <spell>"), not the bridge:
-- playerbots' `cast` is a chat-command trigger, so RUN~COMBAT never fires it.
-- (Matches the reference addon's SpellBookFrame.) Bots must be grouped with you.
local function castSpell(names, spell)
  for _, n in ipairs(names) do
    SendChatMessage("cast " .. spell, "WHISPER", nil, n)
  end
end

-- Build the Cast submenu from the spell catalog for a single bot record.
local function castSubmenu(names, rec)
  if not rec then return { item("(select one bot)", function() end) } end
  local spells = NS.Spells.forClass(rec.classToken, rec.spec, function() return true end)
  if #spells == 0 then return { item("(no spells)", function() end) } end
  local util, conv = {}, {}
  for _, e in ipairs(spells) do
    local bucket = (e.category == "convenience") and conv or util
    bucket[#bucket + 1] = item(e.label, function() castSpell(names, e.spell) end)
  end
  local out = {}
  if #util > 0 then out[#out + 1] = item("Utility", nil, util) end
  if #conv > 0 then out[#out + 1] = item("Convenience", nil, conv) end
  return out
end

-- Open the menu for a click scope. `clicked` is the right-clicked bot name;
-- `recs` is the current record map (for class-aware Cast).
function Menu.Open(clicked, recs)
  local names = NS.Selection.scopeOrClicked(clicked)
  local rec = recs and recs[clicked]
  local single = (#names == 1) and recs and recs[names[1]] or rec

  local rtiSub = {}
  for _, r in ipairs(RTI_ICONS) do
    rtiSub[#rtiSub + 1] = item(r[1], function() rti(names, "rti " .. string.lower(r[1])) end)
  end
  local formSub = {}
  for _, f in ipairs(FORMATIONS) do
    formSub[#formSub + 1] = item(f, function() position(names, "formation " .. f) end)
  end

  local menu = {
    { text = string.format("BotGrid (%d)", #names), isTitle = true, notCheckable = true },
    item("Set role (respec + gear)", nil, {
      item("Main tank", function() setMainTank(names) end),
      item("Off-tank",  function() setRole(names, "tank") end),
      item("DPS",       function() setRole(names, "dps") end),
      item("Healer",    function() setRole(names, "heal") end),
    }),
    item("Attack my target", function() combat(names, "do attack my target") end),
    item("Flee / Retreat",   function() combat(names, "flee") end),
    item("Mark (RTI)", nil, rtiSub),
    item("Position", nil, {
      item("Follow",     function() position(names, "follow") end),
      item("Stay / Hold",function() position(names, "stay") end),
      item("Come to me", function() position(names, "come") end),
      item("Formation",  nil, formSub),
    }),
    item("Cast", nil, castSubmenu(names, single)),
    item("Sustain", nil, {
      item("Drink",   function() combat(names, "drink") end),
      item("Revive",  function() combat(names, "revive") end),
      item("Release", function() combat(names, "release") end),
      item("Summon",  function() combat(names, "summon") end),
    }),
    item("Loot mode", nil, {
      item("Loot all",  function() loot(names, "loot all") end),
      item("Loot none", function() loot(names, "loot none") end),
    }),
    item("More… (open MultiBot)", function()
      if SlashCmdList and SlashCmdList["MULTIBOT"] then SlashCmdList["MULTIBOT"]("") end
    end),
  }
  -- ALWAYS close first. EasyMenu ends in ToggleDropDownMenu, which *hides* the
  -- list when it is already open for this same frame -- so a menu the user
  -- abandoned (moved off and never clicked an item) turns the next right-click
  -- into a silent no-op. That was half of the "right-click 3-4 times" bug.
  CloseDropDownMenus()
  -- 6s auto-hide: an abandoned list can't sit on top of the grid swallowing
  -- clicks meant for the cells underneath. The timer only counts down once the
  -- cursor has left the menu, so navigating it is unaffected.
  EasyMenu(menu, menuFrame, "cursor", 0, 0, "MENU", 6)
end
