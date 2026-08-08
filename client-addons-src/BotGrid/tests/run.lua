-- Runs every tests/test_*.lua. Exits nonzero if any assertion failed.
local H = dofile("client-addons-src/BotGrid/tests/init.lua")

local files = {
  "test_comm.lua",
  "test_model.lua",
  "test_spells.lua",
  "test_selection.lua",
  "test_style.lua",
  "test_grid.lua",
  "test_menu.lua",
}

for _, f in ipairs(files) do
  local path = "client-addons-src/BotGrid/tests/" .. f
  local chunk = loadfile(path)
  if chunk then
    print("== " .. f)
    local testfn = chunk()   -- the file returns its test function
    testfn(H)
  end
end

print(string.format("\n%d checks, %d failures", H.count, H.failures))
if H.failures > 0 then os.exit(1) end
print("OK")
