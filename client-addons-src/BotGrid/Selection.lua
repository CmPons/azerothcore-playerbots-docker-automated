local NS = _G.BotGridNS or {}
_G.BotGridNS = NS

local Sel = {}
NS.Selection = Sel

local selected = {}   -- set: name -> true
local anchor = nil    -- last single-clicked name (for shift range)

function Sel.clear() selected = {}; anchor = nil end

function Sel.has(name) return selected[name] == true end

function Sel.list()
  local out = {}
  for name in pairs(selected) do out[#out + 1] = name end
  table.sort(out)
  return out
end

local function indexOf(order, name)
  for i, n in ipairs(order) do if n == name then return i end end
  return nil
end

-- ctrl = toggle add/remove; shift = range from anchor; plain = replace with one.
-- order is the current display order (flattened columns) for range math.
function Sel.click(name, ctrl, shift, order)
  if shift and anchor then
    local i, j = indexOf(order, anchor), indexOf(order, name)
    if i and j then
      if i > j then i, j = j, i end
      for k = i, j do selected[order[k]] = true end
      return
    end
  end
  if ctrl then
    selected[name] = (not selected[name]) or nil
    anchor = name
    return
  end
  selected = { [name] = true }
  anchor = name
end

-- Replace the whole selection (column header click, select-all).
function Sel.selectNames(names)
  selected = {}
  for _, n in ipairs(names or {}) do selected[n] = true end
  anchor = names and names[#names] or nil
end

-- Action scope: the selection if non-empty, otherwise just the clicked bot.
function Sel.scopeOrClicked(name)
  local list = Sel.list()
  if #list > 0 then return list end
  return { name }
end
