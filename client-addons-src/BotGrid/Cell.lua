local NS = _G.BotGridNS or {}
_G.BotGridNS = NS

local Cell = {}
NS.Cell = Cell

local S = NS.Style
local W, Hgt = 54, 40  -- cell size (kept in sync with Grid.lua CELL_W/CELL_H)
Cell.W, Cell.H = W, Hgt

-- Full spec map: all 10 classes x 3 trees. The old map only covered the 9
-- tank/heal specs, so every dps fell through to one generic crossed-swords icon
-- -- which read as an error mark repeated across the whole DPS column.
local SPEC_ICON = {
  ["WARRIOR/Arms"]              = "Interface\\Icons\\Ability_Warrior_SavageBlow",
  ["WARRIOR/Fury"]              = "Interface\\Icons\\Ability_Warrior_InnerRage",
  ["WARRIOR/Protection"]        = "Interface\\Icons\\Ability_Warrior_DefensiveStance",
  ["PALADIN/Holy"]              = "Interface\\Icons\\Spell_Holy_HolyBolt",
  ["PALADIN/Protection"]        = "Interface\\Icons\\Spell_Holy_DevotionAura",
  ["PALADIN/Retribution"]       = "Interface\\Icons\\Spell_Holy_AuraOfLight",
  ["HUNTER/Beast Mastery"]      = "Interface\\Icons\\Ability_Hunter_BeastTaming",
  ["HUNTER/Marksmanship"]       = "Interface\\Icons\\Ability_Marksmanship",
  ["HUNTER/Survival"]           = "Interface\\Icons\\Ability_Hunter_SwiftStrike",
  ["ROGUE/Assassination"]       = "Interface\\Icons\\Ability_Rogue_Eviscerate",
  ["ROGUE/Combat"]              = "Interface\\Icons\\Ability_BackStab",
  ["ROGUE/Subtlety"]            = "Interface\\Icons\\Ability_Stealth",
  ["PRIEST/Discipline"]         = "Interface\\Icons\\Spell_Holy_PowerWordShield",
  ["PRIEST/Holy"]               = "Interface\\Icons\\Spell_Holy_GuardianSpirit",
  ["PRIEST/Shadow"]             = "Interface\\Icons\\Spell_Shadow_ShadowWordPain",
  ["DEATHKNIGHT/Blood"]         = "Interface\\Icons\\Spell_Deathknight_BloodPresence",
  ["DEATHKNIGHT/Frost"]         = "Interface\\Icons\\Spell_Deathknight_FrostPresence",
  ["DEATHKNIGHT/Unholy"]        = "Interface\\Icons\\Spell_Deathknight_UnholyPresence",
  ["SHAMAN/Elemental"]          = "Interface\\Icons\\Spell_Nature_Lightning",
  ["SHAMAN/Enhancement"]        = "Interface\\Icons\\Spell_Nature_LightningShield",
  ["SHAMAN/Restoration"]        = "Interface\\Icons\\Spell_Nature_MagicImmunity",
  ["MAGE/Arcane"]               = "Interface\\Icons\\Spell_Holy_MagicalSentry",
  ["MAGE/Fire"]                 = "Interface\\Icons\\Spell_Fire_FireBolt02",
  ["MAGE/Frost"]                = "Interface\\Icons\\Spell_Frost_FrostBolt02",
  ["WARLOCK/Affliction"]        = "Interface\\Icons\\Spell_Shadow_DeathCoil",
  ["WARLOCK/Demonology"]        = "Interface\\Icons\\Spell_Shadow_Metamorphosis",
  ["WARLOCK/Destruction"]       = "Interface\\Icons\\Spell_Shadow_RainOfFire",
  ["DRUID/Balance"]             = "Interface\\Icons\\Spell_Nature_StarFall",
  ["DRUID/Feral"]               = "Interface\\Icons\\Ability_Racial_BearForm",
  ["DRUID/Restoration"]         = "Interface\\Icons\\Spell_Nature_HealingTouch",
}
local ICON_UNKNOWN = "Interface\\Icons\\INV_Misc_QuestionMark"

-- Show the health number only below this. Bots idle in the high 80s/low 90s
-- while regenerating, so a 98 threshold lit up every cell at once -- pure noise,
-- and it hid gear score almost permanently.
local HURT_AT = 90

local function showTooltip(self)
  local rec = self.rec
  if not rec then return end
  GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
  GameTooltip:AddLine(rec.name, S.NameColor(rec.classToken))
  local spec = rec.spec or "?"
  GameTooltip:AddLine(spec .. "  |cff7a869a" .. string.upper(rec.role or "") .. "|r", 0.8, 0.8, 0.8)
  if rec.score and rec.score > 0 then
    GameTooltip:AddLine("Gear score " .. rec.score, 0.55, 0.6, 0.68)
  end
  if not rec.alive then
    GameTooltip:AddLine("Dead", 0.92, 0.24, 0.24)
  elseif rec.hpPct and rec.hpPct < 100 then
    GameTooltip:AddLine(rec.hpPct .. "% health", S.HealthTextColor(rec.hpPct))
  end
  if rec.mismatch then
    GameTooltip:AddLine("Spec does not match assigned role", 0.98, 0.70, 0.22)
  end
  GameTooltip:AddLine("Left-click select  ·  Right-click menu", 0.42, 0.46, 0.53)
  GameTooltip:Show()
end

-- Create a reusable cell frame parented to `parent`.
function Cell.New(parent)
  local f = CreateFrame("Button", nil, parent)
  f:SetWidth(W); f:SetHeight(Hgt)
  -- The menu opens on right-button DOWN, so there is no down->up window for a
  -- refresh or a cursor twitch to swallow (an up-registered OnClick is dropped
  -- if the cursor leaves the widget before release). Selection stays on up.
  f:RegisterForClicks("LeftButtonUp", "RightButtonDown")

  -- Wired once at creation, not per refresh: the handler reads the cell's
  -- current botName, so it stays correct as cells are re-assigned.
  f:SetScript("OnClick", function(self, button)
    NS.Grid.OnCellClick(self.botName, button)
  end)

  local bg = f:CreateTexture(nil, "BACKGROUND")
  bg:SetAllPoints(f)
  bg:SetTexture(S.unpackColor(S.color.cellBg))
  f.bg = bg

  -- Drained health shows this through the (bottom-up) fill: a dark wound rather
  -- than empty black, so damage reads even on a nearly-full bar.
  local wound = f:CreateTexture(nil, "BORDER")
  wound:SetPoint("TOPLEFT", f, "TOPLEFT", 3, -1)
  wound:SetPoint("BOTTOMRIGHT", f, "BOTTOMRIGHT", -1, 1)
  wound:SetTexture(S.unpackColor(S.color.wound))
  f.wound = wound

  -- Vertical health: drains TOP->BOTTOM (a vertical statusbar fills bottom-up,
  -- so a falling value empties from the top = Grid2 behaviour).
  local hp = CreateFrame("StatusBar", nil, f)
  hp:SetPoint("TOPLEFT", f, "TOPLEFT", 3, -1)
  hp:SetPoint("BOTTOMRIGHT", f, "BOTTOMRIGHT", -1, 1)
  hp:SetOrientation("VERTICAL")
  hp:SetStatusBarTexture(S.BAR)
  hp:SetMinMaxValues(0, 100)
  f.hp = hp

  -- Thin mana bar along the bottom edge.
  local mp = CreateFrame("StatusBar", nil, f)
  mp:SetPoint("BOTTOMLEFT", f, "BOTTOMLEFT", 3, 1)
  mp:SetPoint("BOTTOMRIGHT", f, "BOTTOMRIGHT", -1, 1)
  mp:SetHeight(3)
  mp:SetStatusBarTexture(S.BAR)
  mp:SetStatusBarColor(S.unpackColor(S.color.mana))
  mp:SetMinMaxValues(0, 100)
  f.mp = mp

  -- Overlay frame ABOVE the health/mana statusbars. A parent's own regions
  -- (FontStrings/textures on `f`) draw BEHIND its child frames like the hp bar,
  -- so the name/icons must live on this higher-level child frame to be visible.
  local fg = CreateFrame("Frame", nil, f)
  fg:SetAllPoints(f)
  fg:SetFrameLevel((hp:GetFrameLevel() or 1) + 5)
  f.fg = fg

  -- Identity spine: the ONE place class colour runs at full brightness.
  local spine = fg:CreateTexture(nil, "BORDER")
  spine:SetPoint("TOPLEFT", fg, "TOPLEFT", 1, -1)
  spine:SetPoint("BOTTOMLEFT", fg, "BOTTOMLEFT", 1, 1)
  spine:SetWidth(2)
  f.spine = spine

  local scrim = fg:CreateTexture(nil, "ARTWORK")
  scrim:SetAllPoints(fg)
  scrim:SetTexture(0, 0, 0, 0)
  f.scrim = scrim

  local sel = fg:CreateTexture(nil, "ARTWORK")
  sel:SetAllPoints(fg)
  sel:SetTexture(0.98, 0.62, 0.18, 0.12)
  sel:Hide()
  f.selOverlay = sel

  local hover = fg:CreateTexture(nil, "ARTWORK")
  hover:SetAllPoints(fg)
  hover:SetTexture(1, 1, 1, 0.06)
  hover:Hide()
  f.hover = hover

  f.edge = S.Hairframe(fg, "OVERLAY", S.color.borderSoft)

  -- Role/spec mismatch: a small amber notch in the otherwise-empty top-left
  -- corner, instead of the old orange wash that buried the health read.
  local warn = fg:CreateTexture(nil, "OVERLAY")
  warn:SetPoint("TOPLEFT", fg, "TOPLEFT", 1, -1)
  warn:SetWidth(4); warn:SetHeight(4)
  warn:SetTexture(S.unpackColor(S.color.warn))
  warn:Hide()
  f.warn = warn

  local name = fg:CreateFontString(nil, "OVERLAY")
  S.Font(name, "body", 11, "OUTLINE")
  name:SetPoint("TOPLEFT", fg, "TOPLEFT", 4, -2)
  name:SetPoint("TOPRIGHT", fg, "TOPRIGHT", -4, -2)
  name:SetJustifyH("CENTER")
  f.nameText = name

  -- Missing health is what matters mid-fight, so it gets the big central slot
  -- and only appears when the bot is actually hurt. Gear score drops to a dim
  -- corner (it never changes during a pull).
  local hpText = fg:CreateFontString(nil, "OVERLAY")
  S.Font(hpText, "body", 13, "OUTLINE")
  hpText:SetPoint("CENTER", fg, "CENTER", 1, -1)
  f.hpText = hpText

  local gs = fg:CreateFontString(nil, "OVERLAY")
  S.Font(gs, "body", 9, "OUTLINE")
  gs:SetPoint("BOTTOMRIGHT", fg, "BOTTOMRIGHT", -3, 5)
  gs:SetTextColor(S.unpackColor(S.color.textDim))
  f.gsText = gs

  local spec = fg:CreateTexture(nil, "OVERLAY")
  spec:SetWidth(11); spec:SetHeight(11)
  spec:SetPoint("BOTTOMLEFT", fg, "BOTTOMLEFT", 4, 5)
  spec:SetTexCoord(0.07, 0.93, 0.07, 0.93)  -- crop the icon's baked-in border
  f.specIcon = spec

  local rti = fg:CreateTexture(nil, "OVERLAY")
  rti:SetWidth(12); rti:SetHeight(12)
  rti:SetPoint("TOPRIGHT", fg, "TOPRIGHT", -1, -1)
  rti:Hide()
  f.rtiIcon = rti

  f:SetScript("OnEnter", function(self)
    self.hover:Show()
    showTooltip(self)
  end)
  f:SetScript("OnLeave", function(self)
    self.hover:Hide()
    GameTooltip:Hide()
  end)

  return f
end

-- Push a Model record into the cell visuals. Runs on every health/mana tick for
-- every cell, so it allocates nothing (the icon map is module scope, not local).
function Cell.SetData(f, rec)
  f.botName = rec.name
  f.rec = rec

  f.nameText:SetText(rec.name)
  f.nameText:SetTextColor(S.NameColor(rec.classToken))
  f.spine:SetTexture(S.ClassColor(rec.classToken))
  f.hp:SetStatusBarColor(S.FillColor(rec.classToken))

  local alive = rec.alive
  local pct = rec.hpPct or 0
  f.hp:SetValue(alive and pct or 0)

  local mp = rec.mpPct or 0
  f.mp:SetValue(mp)
  if mp > 0 and alive then f.mp:Show() else f.mp:Hide() end

  -- Progressive disclosure: gear score at rest, the health deficit when hurt.
  -- They'd fight for the same pixels, and mid-pull only one of them matters.
  local hurt = false
  if not alive then
    f.hpText:SetText("DEAD")
    f.hpText:SetTextColor(S.unpackColor(S.color.danger))
    f.scrim:SetTexture(0.05, 0.0, 0.0, 0.55)
    hurt = true
  else
    f.scrim:SetTexture(0, 0, 0, 0)
    if pct < HURT_AT then
      f.hpText:SetText(pct)
      f.hpText:SetTextColor(S.HealthTextColor(pct))
      hurt = true
    else
      f.hpText:SetText("")
    end
  end

  f.nameText:SetAlpha(alive and 1.0 or 0.45)
  local score = (not hurt) and rec.score and rec.score > 0 and tostring(rec.score) or ""
  f.gsText:SetText(score)

  if rec.loading then
    f.specIcon:SetTexture(ICON_UNKNOWN)
  else
    f.specIcon:SetTexture(SPEC_ICON[(rec.classToken or "") .. "/" .. (rec.spec or "")] or ICON_UNKNOWN)
  end
  f.specIcon:SetAlpha(alive and 0.95 or 0.4)

  if rec.mismatch then f.warn:Show() else f.warn:Hide() end
end

function Cell.SetSelected(f, on)
  if on then
    f.selOverlay:Show()
    f.edge:SetColor(S.color.accent)
  else
    f.selOverlay:Hide()
    f.edge:SetColor(S.color.borderSoft)
  end
end

local RTI_TEX = "Interface\\TargetingFrame\\UI-RaidTargetingIcon_%d"
function Cell.SetRTI(f, iconId)
  local on = (iconId or 0) > 0
  if on then f.rtiIcon:SetTexture(string.format(RTI_TEX, iconId)) end
  -- Re-anchor only when the marker appears/disappears, not on every value tick.
  if on == f.rtiShown then return end
  f.rtiShown = on
  if on then
    f.rtiIcon:Show()
    f.nameText:SetPoint("TOPRIGHT", f.fg, "TOPRIGHT", -14, -2)  -- clear the marker
  else
    f.rtiIcon:Hide()
    f.nameText:SetPoint("TOPRIGHT", f.fg, "TOPRIGHT", -4, -2)
  end
end
