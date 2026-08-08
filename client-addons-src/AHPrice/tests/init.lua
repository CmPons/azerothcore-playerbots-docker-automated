-- Headless test harness for AHPrice pure modules (run under lua5.1).
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

-- Minimal WoW globals referenced at call time.
_G.UnitName = _G.UnitName or function() return "Tester" end

function H.load(file)
  local path = "client-addons-src/AHPrice/" .. file
  local chunk = assert(loadfile(path))
  chunk()
  return _G.AHPriceNS
end

return H
