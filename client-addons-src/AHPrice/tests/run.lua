-- Runs every tests/test_*.lua. Exits nonzero if any assertion failed.
local H = dofile("client-addons-src/AHPrice/tests/init.lua")

local files = { "test_comm.lua", "test_model.lua" }

for _, f in ipairs(files) do
  local path = "client-addons-src/AHPrice/tests/" .. f
  local chunk = loadfile(path)
  if chunk then
    print("== " .. f)
    local testfn = chunk()
    testfn(H)
  end
end

print(string.format("\n%d checks, %d failures", H.count, H.failures))
if H.failures > 0 then os.exit(1) end
print("OK")
