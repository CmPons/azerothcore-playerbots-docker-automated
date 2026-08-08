-- Minimal WoW widget mock so the UI modules (Cell/Grid/Menu/RosterBar) can be
-- exercised headlessly. Methods whose BEHAVIOUR the tests assert on (Show/Hide
-- bookkeeping, SetScript, click registration, text) are implemented for real;
-- the rest are explicit no-ops.
--
-- NOTE: the no-op list is deliberately explicit rather than a catch-all
-- __index, so that reading an unset DATA field (frame.botName, frame.frameLevel)
-- still yields nil instead of a function. A missing method fails loudly with
-- "attempt to call a nil value" — add it below when the UI starts using it.

local M = {}

M.frames = {}          -- every frame ever created
M.easyMenuCalls = 0
M.closeDropDownCalls = 0
M.callLog = {}         -- ordered global-call trace, e.g. "Close", "EasyMenu"

local widget = {}
local function noop() end

for _, name in ipairs({
  -- interaction / movement
  "RegisterForDrag", "EnableMouse", "EnableMouseWheel", "EnableKeyboard",
  "SetMovable", "SetResizable", "SetClampedToScreen", "SetUserPlaced",
  "StartMoving", "StopMovingOrSizing", "SetHitRectInsets", "SetClipsChildren",
  "Raise", "Lower", "SetToplevel", "SetFrameStrata", "GetFrameStrata",
  "Disable", "Enable", "IsEnabled", "LockHighlight", "UnlockHighlight",
  "SetChecked", "GetChecked", "Click", "SetID", "GetID", "SetParent",
  "SetAttribute", "GetAttribute", "CanChangeAttribute", "SetScale", "GetScale",
  -- backdrop / textures
  "SetBackdrop", "SetBackdropColor", "SetBackdropBorderColor",
  "SetTexCoord", "SetVertexColor", "SetGradientAlpha", "SetGradient",
  "SetBlendMode", "SetDrawLayer", "SetRotation", "SetDesaturated",
  "SetNormalTexture", "SetHighlightTexture", "SetPushedTexture",
  "SetDisabledTexture", "GetNormalTexture", "GetHighlightTexture",
  "SetPushedTextOffset", "SetNormalFontObject", "SetFontString",
  -- statusbar
  "SetOrientation", "SetStatusBarTexture", "GetStatusBarTexture",
  "SetMinMaxValues", "GetValue", "SetReverseFill",
  -- fontstring
  "SetJustifyH", "SetJustifyV", "SetShadowOffset", "SetShadowColor",
  "SetTextColor", "SetWordWrap", "SetNonSpaceWrap", "SetMaxLines",
  "SetFontObject",
  -- tooltip
  "SetOwner", "AddLine", "AddDoubleLine", "ClearLines", "SetTextHeight",
}) do
  widget[name] = noop
end

function widget:Show() self.shown = true; self.showCount = self.showCount + 1 end
function widget:Hide() self.shown = false; self.hideCount = self.hideCount + 1 end
function widget:IsShown() return self.shown == true end
function widget:IsVisible() return self.shown == true end
function widget:SetScript(name, fn) self.scripts[name] = fn end
function widget:GetScript(name) return self.scripts[name] end
function widget:HookScript(name, fn) self.scripts[name] = fn end
function widget:SetWidth(w) self.width = w end
function widget:SetHeight(h) self.height = h end
function widget:SetSize(w, h) self.width = w; self.height = h end
function widget:GetWidth() return self.width or 0 end
function widget:GetHeight() return self.height or 0 end
function widget:SetText(t) self.text = t end
function widget:GetText() return self.text end
function widget:GetStringWidth() return string.len(self.text or "") * 5 end
-- M.failFonts[path] = true makes SetFont report failure, so the fallback path
-- in Style.Font can be exercised.
function widget:SetFont(path, size, flags)
  if M.failFonts and M.failFonts[path] then return false end
  self.font = { path, size, flags }
  return true
end
-- The real client hands the path back NORMALISED, not verbatim -- which is why
-- verifying a SetFont by string-comparing GetFont() to the requested path always
-- reports failure. Mimic that here so the mock can't hide the bug.
function widget:GetFont()
  if not self.font then return nil end
  return (string.gsub(string.lower(self.font[1]), "\\", "/"))
end
function widget:GetFrameLevel() return self.frameLevel or 1 end
function widget:SetFrameLevel(l) self.frameLevel = l end
function widget:SetPoint(...) self.points[#self.points + 1] = { ... } end
function widget:SetAllPoints(...) self.points[#self.points + 1] = { "ALL" } end
function widget:ClearAllPoints() self.points = {} end
function widget:GetName() return self.frameName end
function widget:GetParent() return self.parent end
function widget:GetObjectType() return self.kind end
function widget:RegisterForClicks(...)
  self.clicks = {}
  for _, c in ipairs({ ... }) do self.clicks[c] = true end
end
function widget:SetTexture(...) self.texture = { ... } end
function widget:SetStatusBarColor(r, g, b, a) self.barColor = { r, g, b, a } end
function widget:SetValue(v) self.value = v end
function widget:SetAlpha(a) self.alpha = a end
function widget:GetAlpha() return self.alpha or 1 end
function widget:GetEffectiveScale() return 1 end
function widget:GetCenter() return 0, 0 end
function widget:RegisterEvent() end
function widget:UnregisterEvent() end

local function newWidget(kind, name, parent)
  local f = {
    kind = kind, frameName = name, parent = parent,
    scripts = {}, points = {}, children = {},
    shown = true, showCount = 0, hideCount = 0, clicks = {},
  }
  setmetatable(f, { __index = widget })
  M.frames[#M.frames + 1] = f
  return f
end

function widget:CreateTexture(name, layer) return newWidget("Texture", name, self) end
function widget:CreateFontString(name, layer, tmpl)
  local fs = newWidget("FontString", name, self)
  fs.template = tmpl
  return fs
end

-- Reset Show/Hide counters across every frame (call between refresh passes).
function M.resetCounters()
  for _, f in ipairs(M.frames) do f.showCount = 0; f.hideCount = 0 end
end

function M.install()
  _G.UIParent = _G.UIParent or newWidget("Frame", "UIParent", nil)
  _G.Minimap = _G.Minimap or newWidget("Frame", "Minimap", _G.UIParent)
  _G.GameTooltip = _G.GameTooltip or newWidget("Frame", "GameTooltip", _G.UIParent)

  _G.CreateFrame = function(kind, name, parent, template)
    local f = newWidget(kind, name, parent)
    f.template = template
    if name then _G[name] = f end
    return f
  end

  _G.RAID_CLASS_COLORS = {
    WARRIOR = { r = 0.78, g = 0.61, b = 0.43 },
    PALADIN = { r = 0.96, g = 0.55, b = 0.73 },
    PRIEST  = { r = 1.00, g = 1.00, b = 1.00 },
    DRUID   = { r = 1.00, g = 0.49, b = 0.04 },
    MAGE    = { r = 0.41, g = 0.80, b = 0.94 },
    ROGUE   = { r = 1.00, g = 0.96, b = 0.41 },
    SHAMAN  = { r = 0.00, g = 0.44, b = 0.87 },
    WARLOCK = { r = 0.58, g = 0.51, b = 0.79 },
    HUNTER  = { r = 0.67, g = 0.83, b = 0.45 },
    DEATHKNIGHT = { r = 0.77, g = 0.12, b = 0.23 },
  }

  _G.EasyMenu = function()
    M.easyMenuCalls = M.easyMenuCalls + 1
    M.callLog[#M.callLog + 1] = "EasyMenu"
  end
  _G.CloseDropDownMenus = function()
    M.closeDropDownCalls = M.closeDropDownCalls + 1
    M.callLog[#M.callLog + 1] = "Close"
  end
  _G.UIDropDownMenu_Initialize = noop
  _G.ToggleDropDownMenu = noop
  _G.SendChatMessage = noop
  _G.SetPartyAssignment = noop
  _G.ChatEdit_SendText = noop
  _G.DEFAULT_CHAT_FRAME = { AddMessage = noop, editBox = newWidget("EditBox", nil, nil) }
  _G.IsControlKeyDown = function() return false end
  _G.IsShiftKeyDown = function() return false end
  _G.UnitName = function() return nil end
  _G.UnitHealth = function() return 0 end
  _G.UnitHealthMax = function() return 0 end
  _G.UnitMana = function() return 0 end
  _G.UnitManaMax = function() return 0 end
  _G.UnitIsDeadOrGhost = function() return false end
  _G.UnitClass = function() return "Warrior", "WARRIOR" end
  _G.GetCursorPosition = function() return 0, 0 end
  _G.GetRaidTargetIndex = function() return nil end
  _G.SlashCmdList = _G.SlashCmdList or {}

  M.easyMenuCalls, M.closeDropDownCalls, M.callLog = 0, 0, {}
  M.failFonts = {}
  return M
end

return M
