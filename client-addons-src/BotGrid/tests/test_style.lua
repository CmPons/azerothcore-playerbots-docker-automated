-- Style tokens. The font test guards a bug that already shipped: verifying a
-- SetFont by comparing GetFont() to the requested path fails even on success
-- (the client normalises the path), which silently replaced Arial Narrow and
-- Morpheus with the Friz Quadrata fallback everywhere -- so names truncated and
-- the display face never appeared.

return function(H)
  local mock = dofile("client-addons-src/BotGrid/tests/framemock.lua").install()
  local NS = H.load("Style.lua")
  local S = NS.Style

  local host = CreateFrame("Frame", nil, UIParent)

  local body = host:CreateFontString(nil, "OVERLAY")
  S.Font(body, "body", 11, "OUTLINE")
  H.eq(body.font[1], "Fonts\\ARIALN.TTF", "a face that loads is KEPT, not replaced by the fallback")
  H.eq(body.font[2], 11, "size is applied")

  local display = host:CreateFontString(nil, "OVERLAY")
  S.Font(display, "display", 14)
  H.eq(display.font[1], "Fonts\\MORPHEUS.TTF", "the display face survives too")

  -- A face the client really doesn't have must fall back rather than leaving
  -- the string with no font at all (invisible text).
  mock.failFonts["Fonts\\MORPHEUS.TTF"] = true
  local missing = host:CreateFontString(nil, "OVERLAY")
  S.Font(missing, "display", 12)
  H.eq(missing.font[1], "Fonts\\FRIZQT__.TTF", "a genuinely missing face falls back")
  mock.failFonts["Fonts\\MORPHEUS.TTF"] = nil

  -- The health fill must stay dark: a raid idles near full, so the fill covers
  -- most of the cell and any brightness reads as a saturated slab.
  local r, g, b = S.FillColor("PRIEST")   -- white = the brightest class colour
  H.truthy(math.max(r, g, b) <= 0.35, "fill stays dark even for the brightest class")

  local nr, ng, nb = S.NameColor("SHAMAN")  -- dark blue = the hardest to read
  H.truthy(math.max(nr, ng, nb) >= 0.4, "name tint is lifted toward white for legibility")

  H.eq(#S.color.panelBg, 4, "colours carry an alpha channel")
end
