return function(H)
  local NS = H.load("Spells.lua")
  local S = NS.Spells

  -- forClass returns a flat list of {label, cmd, category} entries.
  -- A "known spell" predicate filters out spells the bot doesn't have.
  local knowAll = function(_) return true end

  local mage = S.forClass("MAGE", "Frost", knowAll)
  local labels = {}
  for _, e in ipairs(mage) do labels[e.label] = e.cmd end
  H.truthy(labels["Conjure Refreshment"], "mage has refreshment")
  H.truthy(labels["Counterspell"], "mage has interrupt")
  H.truthy(labels["Portal: Stormwind"], "mage has SW portal")
  H.truthy(labels["Teleport: Dalaran"], "mage has dalaran teleport")
  H.eq(labels["Counterspell"], "cast Counterspell", "interrupt cmd format")

  local war = S.forClass("WARRIOR", "Fury", knowAll)
  local wl = {}
  for _, e in ipairs(war) do wl[e.label] = true end
  H.truthy(wl["Pummel"], "warrior interrupt")

  -- predicate filters: if the bot doesn't know a spell, it's excluded
  local knowNone = function(_) return false end
  H.eq(#S.forClass("MAGE", "Frost", knowNone), 0, "unknown spells excluded")

  -- unknown class -> empty list, no error
  H.eq(#S.forClass("BOGUS", "X", knowAll), 0, "unknown class empty")
end
