local NS = _G.BotGridNS or {}
_G.BotGridNS = NS

-- Design tokens + widget helpers for the BotGrid look ("Obsidian Command"):
-- a cold near-black console, hairline etched rules, ONE ember-amber accent, and
-- class colour used as a signal (a full-brightness spine, a tinted name) rather
-- than as a wall of paint -- so the health read and the text stay legible on top
-- of a busy 3D scene. Everything here is 3.3.5a-safe.
local Style = {}
NS.Style = Style

-- 1x1 white texture; tinted via SetTexture(r,g,b,a) for hairlines and plates.
Style.WHITE = "Interface\\Buttons\\WHITE8X8"
Style.BAR   = "Interface\\TargetingFrame\\UI-StatusBar"

Style.color = {
  panelBg    = { 0.043, 0.047, 0.059, 0.94 },  -- cold near-black
  titleBg    = { 0.086, 0.094, 0.114, 0.98 },
  plateBg    = { 0.071, 0.078, 0.094, 0.75 },
  cellBg     = { 0.051, 0.055, 0.067, 0.92 },
  wound      = { 0.157, 0.047, 0.055, 0.95 },  -- drained health reads as a wound
  border     = { 0.243, 0.263, 0.314, 1.00 },  -- cold steel hairline
  borderSoft = { 0.129, 0.141, 0.169, 1.00 },
  accent     = { 0.980, 0.616, 0.176, 1.00 },  -- ember amber
  accentDim  = { 0.980, 0.616, 0.176, 0.28 },
  text       = { 0.878, 0.898, 0.937, 1.00 },
  textDim    = { 0.478, 0.518, 0.596, 1.00 },
  danger     = { 0.918, 0.243, 0.243, 1.00 },
  warn       = { 0.980, 0.702, 0.220, 1.00 },
  mana       = { 0.220, 0.420, 0.850, 1.00 },
}

-- Morpheus (the quest-title serif) as the display face; Arial Narrow for data --
-- condensed means a full bot name fits where the default font truncates it.
Style.font = {
  display  = "Fonts\\MORPHEUS.TTF",
  body     = "Fonts\\ARIALN.TTF",
  fallback = "Fonts\\FRIZQT__.TTF",
}

-- Apply a font, falling back only if the face genuinely didn't load.
--
-- Do NOT verify by comparing GetFont() to the requested path: the client hands
-- the path back in its own normalised form, so the comparison fails even on
-- success and quietly drops every custom face back to Friz Quadrata (which is
-- what made names truncate -- Arial Narrow was never actually applied).
function Style.Font(fs, face, size, flags)
  local path = Style.font[face] or face
  local ok = fs:SetFont(path, size, flags)
  if ok == false or not fs:GetFont() then
    fs:SetFont(Style.font.fallback, size, flags)
  end
  return fs
end

local function unpackColor(c)
  return c[1], c[2], c[3], c[4] or 1
end
Style.unpackColor = unpackColor

-- A solid tinted texture (the building block for plates, rules and hairlines).
function Style.Solid(parent, layer, color)
  local t = parent:CreateTexture(nil, layer or "ARTWORK")
  t:SetTexture(Style.WHITE)
  if color then t:SetTexture(unpackColor(color)) end
  return t
end

-- 1px border built from four hairlines, so it can be recoloured (selection)
-- without touching the frame's backdrop. Returns a table with a SetColor().
function Style.Hairframe(parent, layer, color)
  local h = { parent = parent }
  for _, k in ipairs({ "top", "bottom", "left", "right" }) do
    h[k] = Style.Solid(parent, layer or "BORDER", color)
  end
  h.top:SetPoint("TOPLEFT", parent, "TOPLEFT", 0, 0)
  h.top:SetPoint("TOPRIGHT", parent, "TOPRIGHT", 0, 0)
  h.top:SetHeight(1)
  h.bottom:SetPoint("BOTTOMLEFT", parent, "BOTTOMLEFT", 0, 0)
  h.bottom:SetPoint("BOTTOMRIGHT", parent, "BOTTOMRIGHT", 0, 0)
  h.bottom:SetHeight(1)
  h.left:SetPoint("TOPLEFT", parent, "TOPLEFT", 0, -1)
  h.left:SetPoint("BOTTOMLEFT", parent, "BOTTOMLEFT", 0, 1)
  h.left:SetWidth(1)
  h.right:SetPoint("TOPRIGHT", parent, "TOPRIGHT", 0, -1)
  h.right:SetPoint("BOTTOMRIGHT", parent, "BOTTOMRIGHT", 0, 1)
  h.right:SetWidth(1)
  function h:SetColor(c)
    local r, g, b, a = unpackColor(c)
    self.top:SetTexture(r, g, b, a)
    self.bottom:SetTexture(r, g, b, a)
    self.left:SetTexture(r, g, b, a)
    self.right:SetTexture(r, g, b, a)
  end
  return h
end

-- Flat backdrop: 1px hairline edge, no artwork. The stock dialog backdrop's
-- parchment is what made the old panel bleed beige through the grid.
function Style.Backdrop(frame, bg, edge)
  if not frame.SetBackdrop then return frame end
  frame:SetBackdrop({
    bgFile = Style.WHITE, edgeFile = Style.WHITE,
    tile = false, edgeSize = 1,
    insets = { left = 0, right = 0, top = 0, bottom = 0 },
  })
  frame:SetBackdropColor(unpackColor(bg or Style.color.panelBg))
  frame:SetBackdropBorderColor(unpackColor(edge or Style.color.border))
  return frame
end

-- Class colour, plus the two derived tints the cell uses.
function Style.ClassColor(token)
  local c = RAID_CLASS_COLORS and RAID_CLASS_COLORS[token or ""]
  if c then return c.r, c.g, c.b end
  return 0.6, 0.6, 0.6
end

-- Health fill: the class colour darkened HARD. A raid idles near full health, so
-- the fill covers ~90% of the cell and any brightness here reads as a saturated
-- slab (the exact problem this design set out to kill). Identity lives in the
-- spine and the name; the fill only needs to be a dark tint that says "topped
-- off", and to stay dark enough for an outlined 13px number to sit on it.
function Style.FillColor(token)
  local r, g, b = Style.ClassColor(token)
  return r * 0.30, g * 0.30, b * 0.30
end

-- Name tint: the class colour lifted toward white so it reads at 10px.
function Style.NameColor(token)
  local r, g, b = Style.ClassColor(token)
  return r + (1 - r) * 0.42, g + (1 - g) * 0.42, b + (1 - b) * 0.42
end

-- Severity ramp for the missing-health readout: amber -> red as it drops.
function Style.HealthTextColor(pct)
  if pct >= 70 then return 0.92, 0.84, 0.55 end
  if pct >= 40 then return 0.98, 0.70, 0.22 end
  return 0.95, 0.35, 0.30
end

-- A flat chip button (roster bar). `tint` recolours the hover/active edge.
function Style.Chip(parent, label, width, tint)
  local b = CreateFrame("Button", nil, parent)
  b:SetHeight(19)
  b:SetWidth(width)
  local bg = Style.Solid(b, "BACKGROUND", { 0.098, 0.106, 0.129, 0.95 })
  bg:SetAllPoints(b)
  b.bg = bg
  b.edge = Style.Hairframe(b, "BORDER", Style.color.borderSoft)
  local fs = b:CreateFontString(nil, "OVERLAY")
  Style.Font(fs, "body", 11, "OUTLINE")
  fs:SetPoint("CENTER", b, "CENTER", 0, 0)
  fs:SetText(label)
  fs:SetTextColor(unpackColor(Style.color.text))
  b.label = fs
  b.tint = tint or Style.color.accent
  b:SetScript("OnEnter", function(self)
    self.edge:SetColor(self.tint)
    self.label:SetTextColor(unpackColor(self.tint))
    self.bg:SetTexture(0.145, 0.157, 0.184, 0.98)
  end)
  b:SetScript("OnLeave", function(self)
    self.edge:SetColor(Style.color.borderSoft)
    self.label:SetTextColor(unpackColor(Style.color.text))
    self.bg:SetTexture(0.098, 0.106, 0.129, 0.95)
  end)
  return b
end

return Style
