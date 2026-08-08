local NS = _G.BotGridNS or {}
_G.BotGridNS = NS

local RB = {}
NS.RosterBar = RB

local S = NS.Style

-- Fire a server dot-command via the chat edit box (same as RaidRoster addon).
local function Run(cmd)
  local eb = DEFAULT_CHAT_FRAME.editBox
  eb:SetText(cmd)
  ChatEdit_SendText(eb, 0)
end
RB.Run = Run

-- Grouped rather than nine identical buttons: the raid sizes are one segmented
-- control, and Reset (destructive) is pushed to the far right in danger red so
-- it no longer sits shoulder-to-shoulder with Create.
local GROUPS = {
  { { text = "Create", cmd = ".raidroster create", w = 48 } },
  {
    label = "LOGIN",
    segmented = true,
    { text = "5",  cmd = ".raidroster login 5",  w = 22 },
    { text = "10", cmd = ".raidroster login 10", w = 24 },
    { text = "25", cmd = ".raidroster login 25", w = 24 },
    { text = "40", cmd = ".raidroster login 40", w = 24 },
  },
  {
    { text = "Sync",   cmd = ".raidroster sync",   w = 40 },
    { text = "Logout", cmd = ".raidroster logout", w = 48 },
    { text = "Status", cmd = ".raidroster status", w = 46 },
  },
  { { text = "Reset", cmd = ".raidroster reset", w = 44, danger = true } },
}

-- Attach the bar across the bottom of the given parent panel.
function RB.Attach(parent)
  local x = 8
  for gi, group in ipairs(GROUPS) do
    if group.label then
      local lbl = parent:CreateFontString(nil, "OVERLAY")
      S.Font(lbl, "body", 9, "OUTLINE")
      lbl:SetPoint("BOTTOMLEFT", parent, "BOTTOMLEFT", x, 13)
      lbl:SetText(group.label)
      lbl:SetTextColor(S.unpackColor(S.color.textDim))
      x = x + lbl:GetStringWidth() + 5
    end

    for bi, b in ipairs(group) do
      local chip = S.Chip(parent, b.text, b.w,
        b.danger and S.color.danger or S.color.accent)
      chip:SetPoint("BOTTOMLEFT", parent, "BOTTOMLEFT", x, 7)
      chip:SetScript("OnClick", function()
        Run(b.cmd)
        if NS.RequestSoon then NS.RequestSoon() end  -- re-pull grid after sync/login
      end)
      -- Segmented: chips butt together and share one edge.
      x = x + b.w + (group.segmented and bi < #group and -1 or 3)
    end

    -- Hairline divider between groups.
    if gi < #GROUPS then
      local div = S.Solid(parent, "ARTWORK", S.color.borderSoft)
      div:SetPoint("BOTTOMLEFT", parent, "BOTTOMLEFT", x + 2, 9)
      div:SetWidth(1); div:SetHeight(15)
      x = x + 7
    end
  end
  RB.width = x
end
