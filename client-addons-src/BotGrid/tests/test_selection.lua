return function(H)
  local NS = H.load("Selection.lua")
  local Sel = NS.Selection
  Sel.clear()

  -- plain click selects exactly one
  Sel.click("Boband", false, false, {"Boband","Cira","Dax"})
  local s = Sel.list()
  H.eq(#s, 1, "single select count")
  H.eq(s[1], "Boband", "single select name")
  H.truthy(Sel.has("Boband"), "has Boband")

  -- ctrl-click toggles add
  Sel.click("Cira", true, false, {"Boband","Cira","Dax"})
  H.eq(#Sel.list(), 2, "ctrl add -> 2")
  Sel.click("Cira", true, false, {"Boband","Cira","Dax"})
  H.eq(#Sel.list(), 1, "ctrl toggle off -> 1")

  -- shift-click range selects from last anchor to target (in display order)
  Sel.clear()
  Sel.click("Boband", false, false, {"Boband","Cira","Dax"})
  Sel.click("Dax", false, true, {"Boband","Cira","Dax"})
  H.eq(#Sel.list(), 3, "shift range -> 3")

  -- selectNames replaces selection (column header / select-all)
  Sel.selectNames({"Cira","Dax"})
  H.eq(#Sel.list(), 2, "selectNames replaces")
  H.truthy(not Sel.has("Boband"), "boband cleared")

  -- scopeOrClicked: returns selection if any, else just the clicked name
  H.eq(#Sel.scopeOrClicked("Boband"), 2, "scope uses selection when present")
  Sel.clear()
  local sc = Sel.scopeOrClicked("Boband")
  H.eq(sc[1], "Boband", "scope falls back to clicked")
end
