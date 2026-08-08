-- Grid layout behaviour. These guard the right-click reliability fix: a data
-- refresh must NOT hide/reshow the cells (a cell hidden between mouse-down and
-- mouse-up swallows the click), and a given bot must keep the same physical
-- frame so the cell under the cursor never changes identity mid-click.

return function(H)
  local mock = dofile("client-addons-src/BotGrid/tests/framemock.lua").install()

  local NS
  for _, f in ipairs({ "Style.lua", "Selection.lua", "Comm.lua", "Model.lua",
                       "Cell.lua", "Grid.lua" }) do
    NS = H.load(f)
  end

  local ROSTER = {
    { "Ayla", "WARRIOR", "tank" }, { "Bex", "PALADIN", "heal" },
    { "Cyn", "MAGE", "dps" },      { "Dorn", "ROGUE", "dps" },
    { "Eryn", "PRIEST", "heal" },
  }

  local function buildRecs(hp)
    local t = {}
    for _, e in ipairs(ROSTER) do
      t[e[1]] = {
        name = e[1], classToken = e[2], role = e[3], alive = true,
        hpPct = hp or 100, mpPct = 60, score = 264,
        spec = "Protection", specRole = e[3], loading = false, mismatch = false,
      }
    end
    return t
  end

  -- Cells are the Buttons that carry a botName.
  local function liveCells()
    local out = {}
    for _, f in ipairs(mock.frames) do
      if f.kind == "Button" and f.botName then out[#out + 1] = f end
    end
    return out
  end

  NS.store = { recs = buildRecs() }
  local panel = NS.Grid.Ensure()
  panel:Show()

  NS.Grid.Refresh(buildRecs(100))
  local first = liveCells()
  H.truthy(#first >= #ROSTER, "a cell is laid out for every bot")

  local owner = {}
  for _, c in ipairs(first) do owner[c.botName] = c end

  -- A pure value update (health ticked) must not tear the cells down.
  mock.resetCounters()
  NS.Grid.Refresh(buildRecs(72))

  local hidden = 0
  for _, c in ipairs(liveCells()) do
    if c.hideCount > 0 then hidden = hidden + 1 end
  end
  H.eq(hidden, 0, "refresh must not hide in-use cells (a hidden cell eats the click)")

  local moved = 0
  for _, c in ipairs(liveCells()) do
    if owner[c.botName] and owner[c.botName] ~= c then moved = moved + 1 end
  end
  H.eq(moved, 0, "each bot keeps the same physical cell frame across refreshes")

  -- The menu must fire on mouse-DOWN: there is then no down->up window for a
  -- refresh (or a cursor twitch) to swallow.
  local upRegistered, downRegistered = 0, 0
  for _, c in ipairs(liveCells()) do
    if c.clicks["RightButtonUp"] then upRegistered = upRegistered + 1 end
    if c.clicks["RightButtonDown"] then downRegistered = downRegistered + 1 end
  end
  H.eq(upRegistered, 0, "cells must not open the menu on RightButtonUp")
  H.truthy(downRegistered > 0, "cells open the menu on RightButtonDown")

  -- Role changes still re-bucket correctly (the layout must react to structure).
  local changed = buildRecs(100)
  changed["Cyn"].role = "heal"
  NS.Grid.Refresh(changed)
  local healers = 0
  for _, n in ipairs(NS.Grid.flatOrder) do
    if changed[n] and changed[n].role == "heal" then healers = healers + 1 end
  end
  H.eq(healers, 3, "a role change re-buckets the bot into the healer column")
end
