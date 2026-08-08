-- Menu.Open behaviour. ToggleDropDownMenu (which EasyMenu calls) HIDES the list
-- when it is already open for the same frame, so an abandoned menu turns the
-- next right-click into a silent no-op. Open must always close first.

return function(H)
  local mock = dofile("client-addons-src/BotGrid/tests/framemock.lua").install()

  local NS
  for _, f in ipairs({ "Style.lua", "Selection.lua", "Comm.lua", "Model.lua",
                       "Spells.lua", "Cell.lua", "Grid.lua", "RosterBar.lua",
                       "Menu.lua" }) do
    NS = H.load(f)
  end

  local recs = {
    Ayla = { name = "Ayla", classToken = "WARRIOR", role = "tank", spec = "Protection",
             alive = true, hpPct = 100, mpPct = 0, score = 264 },
    Bex  = { name = "Bex", classToken = "PALADIN", role = "heal", spec = "Holy",
             alive = true, hpPct = 100, mpPct = 80, score = 260 },
  }

  NS.Selection.clear()
  NS.Menu.Open("Ayla", recs)

  H.truthy(mock.easyMenuCalls > 0, "Menu.Open shows a menu")
  H.truthy(mock.closeDropDownCalls > 0, "Menu.Open closes any stale dropdown first")
  H.eq(mock.callLog[1], "Close", "the close happens BEFORE the open")
  H.eq(mock.callLog[2], "EasyMenu", "...and the open follows it")

  -- Opening twice in a row must produce two real opens, not an open then a
  -- toggle-close (the 'have to right-click 3-4 times' bug).
  NS.Menu.Open("Bex", recs)
  H.eq(mock.easyMenuCalls, 2, "a second Open re-opens the menu for the new bot")
  H.eq(mock.closeDropDownCalls, 2, "each Open closes the previous list first")

  -- Sanity: a bot with no record still yields a menu rather than erroring.
  local ok = pcall(NS.Menu.Open, "Ghost", recs)
  H.truthy(ok, "Menu.Open tolerates an unknown bot name")
end
