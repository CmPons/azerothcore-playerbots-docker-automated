local NS = _G.BotGridNS or {}
_G.BotGridNS = NS

local Grid = {}
NS.Grid = Grid

local S = NS.Style

local CELL_W, CELL_H, PAD = 54, 40, 3   -- cells (kept in sync with Cell.lua)
local TITLE_H   = 22                     -- title strip / drag handle
local PANEL_PAD = 7                      -- panel edge -> content
local HDR_H     = 19                     -- role header + its etched rule
local PLATE_PAD = 4                      -- role plate edge -> cells
local GAP       = 7                      -- gap between role plates
local BAR_ROOM  = 30                     -- vertical room for the roster bar
local COLS = { tank = "TANKS", heal = "HEALERS", dps = "DPS" }
local ORDER = { "tank", "heal", "dps" }
local WRAP = { tank = 2, heal = 3, dps = 5 }  -- cells per row before wrap

-- Tanks and healers STACK in one narrow column beside DPS. Three side-by-side
-- plates left an L-shaped void: DPS needs 5-6 rows while the other two need 2,
-- so most of the panel under tanks/healers was dead space. Stacked, the short
-- groups add up to roughly the DPS height at every raid size.
local COLUMNS = { { "tank", "heal" }, { "dps" } }

local panel, sections
local cells = {}      -- botName -> cell frame; a bot keeps ONE frame while rostered
local free = {}       -- frames recycled from bots that left the roster
local layoutSig       -- shape of the last laid-out roster (see Refresh)
Grid.flatOrder = {}   -- current display order of bot names (for shift-range)

-- A bot owns its frame for as long as it is in the roster. Refresh must never
-- Hide/Show a live cell: a frame hidden between mouse-down and mouse-up
-- swallows the click, and health ticks used to rebuild the whole grid ~5x/sec
-- -- which is what made right-click intermittent.
local function cellFor(name)
  local c = cells[name]
  if not c then
    c = table.remove(free) or NS.Cell.New(panel)
    cells[name] = c
  end
  c.botName = name
  return c
end

-- Build the panel chrome once.
function Grid.Ensure()
  if panel then return panel end
  panel = CreateFrame("Frame", "BotGridPanel", UIParent)
  panel:SetWidth(300); panel:SetHeight(160)  -- resized to fit content on layout
  panel:SetPoint("CENTER")
  panel:SetMovable(true); panel:EnableMouse(true)
  panel:SetClampedToScreen(true)
  panel:RegisterForDrag("LeftButton")
  panel:SetScript("OnDragStart", panel.StartMoving)
  panel:SetScript("OnDragStop", panel.StopMovingOrSizing)
  S.Backdrop(panel, S.color.panelBg, S.color.border)

  -- Title strip: the drag handle, and the only place the display face appears
  -- at size. Doubles as the etched top edge of the console.
  local title = CreateFrame("Frame", nil, panel)
  title:SetPoint("TOPLEFT", panel, "TOPLEFT", 1, -1)
  title:SetPoint("TOPRIGHT", panel, "TOPRIGHT", -1, -1)
  title:SetHeight(TITLE_H)
  title:EnableMouse(true)
  title:RegisterForDrag("LeftButton")
  title:SetScript("OnDragStart", function() panel:StartMoving() end)
  title:SetScript("OnDragStop", function() panel:StopMovingOrSizing() end)
  local tbg = S.Solid(title, "BACKGROUND", S.color.titleBg)
  tbg:SetAllPoints(title)
  local trule = S.Solid(title, "ARTWORK", S.color.accentDim)
  trule:SetPoint("BOTTOMLEFT", title, "BOTTOMLEFT", 0, 0)
  trule:SetPoint("BOTTOMRIGHT", title, "BOTTOMRIGHT", 0, 0)
  trule:SetHeight(1)
  panel.title = title

  local brand = title:CreateFontString(nil, "OVERLAY")
  S.Font(brand, "display", 14)
  brand:SetPoint("LEFT", title, "LEFT", 8, -1)
  brand:SetText("BotGrid")
  brand:SetTextColor(S.unpackColor(S.color.accent))

  local close = CreateFrame("Button", nil, title)
  close:SetWidth(18); close:SetHeight(18)
  close:SetPoint("RIGHT", title, "RIGHT", -4, 0)
  local cx = close:CreateFontString(nil, "OVERLAY")
  S.Font(cx, "body", 14, "OUTLINE")
  cx:SetPoint("CENTER", close, "CENTER", 0, 0)
  cx:SetText("x")
  cx:SetTextColor(S.unpackColor(S.color.textDim))
  close:SetScript("OnEnter", function() cx:SetTextColor(S.unpackColor(S.color.danger)) end)
  close:SetScript("OnLeave", function() cx:SetTextColor(S.unpackColor(S.color.textDim)) end)
  close:SetScript("OnClick", function() panel:Hide() end)

  local total = title:CreateFontString(nil, "OVERLAY")
  S.Font(total, "body", 10, "OUTLINE")
  total:SetPoint("RIGHT", close, "LEFT", -4, 0)
  total:SetTextColor(S.unpackColor(S.color.textDim))
  panel.totalText = total

  -- One inset plate per role, so the three columns read as separate groups
  -- instead of one undifferentiated field of cells.
  sections = {}
  for _, role in ipairs(ORDER) do
    local plate = CreateFrame("Frame", nil, panel)
    local pbg = S.Solid(plate, "BACKGROUND", S.color.plateBg)
    pbg:SetAllPoints(plate)
    S.Hairframe(plate, "BORDER", S.color.borderSoft)

    local header = plate:CreateFontString(nil, "OVERLAY")
    S.Font(header, "display", 12)
    header:SetPoint("TOPLEFT", plate, "TOPLEFT", PLATE_PAD + 1, -3)
    header:SetTextColor(0.82, 0.84, 0.89, 1)

    local count = plate:CreateFontString(nil, "OVERLAY")
    S.Font(count, "body", 10, "OUTLINE")
    count:SetPoint("LEFT", header, "RIGHT", 4, -1)
    count:SetTextColor(S.unpackColor(S.color.textDim))

    local rule = S.Solid(plate, "ARTWORK", S.color.accentDim)
    rule:SetPoint("TOPLEFT", plate, "TOPLEFT", PLATE_PAD, -(HDR_H - 4))
    rule:SetPoint("TOPRIGHT", plate, "TOPRIGHT", -PLATE_PAD, -(HDR_H - 4))
    rule:SetHeight(1)

    sections[role] = { plate = plate, header = header, count = count, rule = rule }
  end

  -- Above the plates: for the offline banner, which would otherwise be hidden
  -- behind them (a panel's own regions draw under its child frames).
  local overlay = CreateFrame("Frame", nil, panel)
  overlay:SetAllPoints(panel)
  overlay:SetFrameLevel(panel:GetFrameLevel() + 20)
  panel.overlay = overlay

  panel:Hide()
  return panel
end

-- Push current values into the cells. The hot path: called on every health /
-- mana tick, so it only re-anchors when the roster SHAPE actually changed.
function Grid.Refresh(recs)
  Grid.Ensure()
  local buckets = NS.Model.bucketByRole(recs)

  -- Signature of the laid-out shape (role order + names). Health ticking does
  -- not change it, so the common refresh is a pure value update: no re-anchor,
  -- no Show/Hide, no panel resize.
  local sig = {}
  for _, role in ipairs(ORDER) do
    sig[#sig + 1] = role
    for _, name in ipairs(buckets[role]) do sig[#sig + 1] = name end
  end
  sig = table.concat(sig, "\1")
  if sig ~= layoutSig then
    Grid.Relayout(buckets)
    layoutSig = sig
  end

  for name, cell in pairs(cells) do
    local rec = recs[name]
    if rec then
      NS.Cell.SetData(cell, rec)
      NS.Cell.SetSelected(cell, NS.Selection.has(name))
      NS.Cell.SetRTI(cell, rec.rti)
    end
  end
end

-- Anchor the cells into the three role plates. Only runs when the roster shape
-- changes (a bot joins/leaves/changes role), never on a plain value update.
function Grid.Relayout(buckets)
  Grid.flatOrder = {}
  local present = {}

  local contentTop = -(TITLE_H + PANEL_PAD)
  local total = 0

  -- Measure every plate first: stacked plates have to share a column width, so
  -- their edges line up instead of stepping in and out.
  local geom = {}
  for _, role in ipairs(ORDER) do
    local names = buckets[role]
    local wrap = WRAP[role]
    local cols = math.max(1, math.min(#names, wrap))
    local rows = math.max(1, math.ceil(#names / wrap))
    total = total + #names
    geom[role] = {
      names = names, wrap = wrap,
      w = cols * (CELL_W + PAD) - PAD + PLATE_PAD * 2,
      h = HDR_H + rows * (CELL_H + PAD) - PAD + PLATE_PAD,
    }
  end

  local x0 = PANEL_PAD
  local maxColH = HDR_H + CELL_H + PLATE_PAD

  for _, roles in ipairs(COLUMNS) do
    local colW, colH = 0, 0
    for i, role in ipairs(roles) do
      colW = math.max(colW, geom[role].w)
      colH = colH + geom[role].h + (i > 1 and GAP or 0)
    end
    maxColH = math.max(maxColH, colH)

    local y = contentTop
    for _, role in ipairs(roles) do
      local g = geom[role]
      local sec = sections[role]

      sec.plate:ClearAllPoints()
      sec.plate:SetPoint("TOPLEFT", panel, "TOPLEFT", x0, y)
      sec.plate:SetWidth(colW)
      sec.plate:SetHeight(g.h)
      sec.header:SetText(COLS[role])
      sec.count:SetText(tostring(#g.names))

      local col, row = 0, 0
      for _, name in ipairs(g.names) do
        Grid.flatOrder[#Grid.flatOrder + 1] = name
        present[name] = true
        local cell = cellFor(name)
        cell:ClearAllPoints()
        cell:SetPoint("TOPLEFT", panel, "TOPLEFT",
          x0 + PLATE_PAD + col * (CELL_W + PAD),
          y - HDR_H - row * (CELL_H + PAD))
        if not cell:IsShown() then cell:Show() end
        col = col + 1
        if col >= g.wrap then col = 0; row = row + 1 end
      end

      -- The header selects the whole group.
      if not sec.clickProxy then
        local p = CreateFrame("Button", nil, sec.plate)
        p:SetScript("OnEnter", function() sec.header:SetTextColor(S.unpackColor(S.color.accent)) end)
        p:SetScript("OnLeave", function() sec.header:SetTextColor(0.82, 0.84, 0.89, 1) end)
        p:SetScript("OnClick", function(self)
          NS.Selection.selectNames(self.names)
          Grid.Refresh(NS.store.recs)
        end)
        sec.clickProxy = p
      end
      sec.clickProxy:ClearAllPoints()
      sec.clickProxy:SetPoint("TOPLEFT", sec.plate, "TOPLEFT", 0, 0)
      sec.clickProxy:SetWidth(colW)
      sec.clickProxy:SetHeight(HDR_H - 4)
      sec.clickProxy.names = { unpack(g.names) }

      y = y - g.h - GAP
    end

    x0 = x0 + colW + GAP
  end

  -- Retire cells whose bot left the roster (frames can't be destroyed in WoW,
  -- so they go back to the free list for the next bot that joins).
  for name, cell in pairs(cells) do
    if not present[name] then
      cell:Hide()
      cell.botName = nil
      cell.rec = nil
      cells[name] = nil
      free[#free + 1] = cell
    end
  end

  panel.totalText:SetText(total .. " BOTS")

  -- Size the panel to its content (kills the empty dead space), but never
  -- narrower than the roster bar along the bottom.
  panel:SetWidth(math.max(240, (NS.RosterBar and NS.RosterBar.width or 0) + PANEL_PAD,
    x0 - GAP + PANEL_PAD))
  panel:SetHeight(TITLE_H + PANEL_PAD + maxColH + PANEL_PAD + BAR_ROOM)
end

-- Cell click router (set by BotGrid.lua to also open the menu on right-click).
function Grid.OnCellClick(name, button)
  if NS.OnCellClick then NS.OnCellClick(name, button) end
end

function Grid.Toggle()
  Grid.Ensure()
  if panel:IsShown() then panel:Hide() else panel:Show() end
end

function Grid.Panel() return panel end
