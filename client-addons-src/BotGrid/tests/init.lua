-- Headless test harness for BotGrid pure modules (run under lua5.1).
-- Provides a tiny assert API and just enough WoW globals for modules to load.

local H = {}
H.failures = 0
H.count = 0

function H.eq(actual, expected, msg)
  H.count = H.count + 1
  if actual ~= expected then
    H.failures = H.failures + 1
    print(string.format("  FAIL: %s\n    expected: %s\n    actual:   %s",
      tostring(msg or ""), tostring(expected), tostring(actual)))
  end
end

function H.truthy(actual, msg)
  H.count = H.count + 1
  if not actual then
    H.failures = H.failures + 1
    print(string.format("  FAIL: %s (expected truthy, got %s)", tostring(msg or ""), tostring(actual)))
  end
end

-- Minimal WoW globals some pure modules reference at call time (not load time).
_G.GetTime = _G.GetTime or function() return 0 end
_G.IsInRaid = _G.IsInRaid or function() return false end
_G.IsInGroup = _G.IsInGroup or function() return false end
_G.GetNumRaidMembers = _G.GetNumRaidMembers or function() return 0 end
_G.GetNumPartyMembers = _G.GetNumPartyMembers or function() return 0 end

-- Load a BotGrid module file from the addon root and return the shared namespace.
function H.load(file)
  local path = "client-addons-src/BotGrid/" .. file
  local chunk = assert(loadfile(path))
  chunk()
  return _G.BotGridNS
end

return H
