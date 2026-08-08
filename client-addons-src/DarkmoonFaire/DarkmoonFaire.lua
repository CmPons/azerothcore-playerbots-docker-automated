-- DarkmoonFaire: minimap tracker for the continuous Darkmoon Faire rotation (WotLK 3.3.5a).
-- The Faire is driven server-side by acore_world.game_event rows with holiday=0, so the
-- native Blizzard Calendar never shows it. This addon computes the current/next location
-- and a countdown deterministically from the SAME anchor + cadence configured in setup.sh.
--
-- !!! KEEP THESE CONSTANTS IN SYNC WITH setup.sh !!!
-- They mirror the DARKMOON_CONTINUOUS block in setup.sh (repo root). Server rows:
--   event 4 = Elwynn  start 2024-01-01 (offset 0d)
--   event 5 = Mulgore start 2024-01-08 (offset 7d)
--   event 3 = Terokkar start 2024-01-15 (offset 14d)
--   length = 10080 min (7d), occurence = 30240 min (21d)
-- If you change the rotation there (or set DARKMOON_CONTINUOUS=0), update these or the
-- addon will be wrong.
local ANCHOR = { year = 2024, month = 1, day = 1 }   -- Elwynn's start_time (server-local midnight)
local WINDOW_DAYS = 7
local LOCATIONS = {
    { name = "Elwynn Forest",   hint = "south of Goldshire" },
    { name = "Mulgore",         hint = "north of Thunder Bluff" },
    { name = "Terokkar Forest", hint = "south of Shattrath City" },
}
local CYCLE_DAYS = WINDOW_DAYS * #LOCATIONS

local PREFIX = "|cff33ff99Darkmoon Faire:|r "

-- Julian Day Number for a civil (Gregorian) date. Integer; used only for differencing.
local function julianDay(y, m, d)
    local a  = math.floor((14 - m) / 12)
    local yy = y + 4800 - a
    local mm = m + 12 * a - 3
    return d + math.floor((153 * mm + 2) / 5) + 365 * yy
         + math.floor(yy / 4) - math.floor(yy / 100) + math.floor(yy / 400) - 32045
end

local ANCHOR_JD = julianDay(ANCHOR.year, ANCHOR.month, ANCHOR.day)

-- Current + next + following location and a countdown string, using SERVER date/time so a
-- player's local timezone can't skew it. Returns nil until the calendar system is ready.
local function computeState()
    local _, month, day, year = CalendarGetDate()
    if not (year and month and day) or year == 0 then
        return nil
    end
    local daysElapsed = julianDay(year, month, day) - ANCHOR_JD
    if daysElapsed < 0 then
        return nil
    end
    local cyclePos = daysElapsed % CYCLE_DAYS
    local index    = math.floor(cyclePos / WINDOW_DAYS)   -- 0-based into LOCATIONS
    local daysLeft = WINDOW_DAYS - (cyclePos % WINDOW_DAYS) -- whole days incl. today, 1..WINDOW_DAYS

    local hour, minute = GetGameTime()                    -- server time
    hour = hour or 0; minute = minute or 0
    local totalHours = (daysLeft - 1) * 24 + (24 - (hour + minute / 60))
    local dd = math.floor(totalHours / 24)
    local hh = math.floor(totalHours - dd * 24)
    local countdown
    if dd > 0 then
        countdown = dd .. "d " .. hh .. "h"
    elseif totalHours >= 1 then
        countdown = hh .. "h"
    else
        countdown = "<1h"
    end

    return {
        current   = LOCATIONS[index + 1],
        nextLoc   = LOCATIONS[((index + 1) % #LOCATIONS) + 1],
        following = LOCATIONS[((index + 2) % #LOCATIONS) + 1],
        countdown = countdown,
    }
end

-- Print the summary to the default chat frame (used by /faire and the minimap click).
local function printSummary()
    local st = computeState()
    if not st then
        DEFAULT_CHAT_FRAME:AddMessage(PREFIX .. "location not available yet (calendar still loading).")
        return
    end
    DEFAULT_CHAT_FRAME:AddMessage(PREFIX .. "now in |cff00ff00" .. st.current.name .. "|r (" .. st.current.hint .. ").")
    DEFAULT_CHAT_FRAME:AddMessage("  Moves to " .. st.nextLoc.name .. " in " .. st.countdown .. "; then " .. st.following.name .. ".")
end

-- ---- Minimap button (self-contained; no libraries; transcribed from RaidRoster) ----
local RADIUS = 80

local btn = CreateFrame("Button", "DarkmoonFaireMinimapButton", Minimap)
btn:SetWidth(31); btn:SetHeight(31)
btn:SetFrameStrata("MEDIUM")
btn:SetFrameLevel(8)
btn:RegisterForClicks("LeftButtonUp")
btn:RegisterForDrag("LeftButton")

local icon = btn:CreateTexture(nil, "BACKGROUND")
icon:SetWidth(20); icon:SetHeight(20)
icon:SetPoint("CENTER", 0, 0)
icon:SetTexture("Interface\\Icons\\INV_Misc_Ticket_Tarot_Stack")

local border = btn:CreateTexture(nil, "OVERLAY")
border:SetWidth(53); border:SetHeight(53)
border:SetPoint("TOPLEFT")
border:SetTexture("Interface\\Minimap\\MiniMap-TrackingBorder")

local function UpdatePos()
    local angle = math.rad(DarkmoonFaireDB.pos or 200)
    btn:ClearAllPoints()
    btn:SetPoint("CENTER", Minimap, "CENTER", RADIUS * math.cos(angle), RADIUS * math.sin(angle))
end

btn:SetScript("OnDragStart", function(self)
    self:SetScript("OnUpdate", function()
        local mx, my = Minimap:GetCenter()
        local scale = Minimap:GetEffectiveScale()
        local px, py = GetCursorPosition()
        px, py = px / scale, py / scale
        DarkmoonFaireDB.pos = math.deg(math.atan2(py - my, px - mx))
        UpdatePos()
    end)
end)
btn:SetScript("OnDragStop", function(self) self:SetScript("OnUpdate", nil) end)

btn:SetScript("OnClick", function() printSummary() end)

btn:SetScript("OnEnter", function(self)
    GameTooltip:SetOwner(self, "ANCHOR_LEFT")
    GameTooltip:AddLine("Darkmoon Faire")
    local st = computeState()
    if st then
        GameTooltip:AddLine("Now: " .. st.current.name, 0, 1, 0)
        GameTooltip:AddLine(st.current.hint, 0.7, 0.7, 0.7)
        GameTooltip:AddLine("Moves in " .. st.countdown .. " -> " .. st.nextLoc.name, 1, 1, 1)
        GameTooltip:AddLine("Then: " .. st.following.name, 0.7, 0.7, 0.7)
    else
        GameTooltip:AddLine("Calculating...", 1, 1, 1)
    end
    GameTooltip:AddLine("/faire for details", 0.5, 0.5, 0.5)
    GameTooltip:Show()
end)
btn:SetScript("OnLeave", function() GameTooltip:Hide() end)

-- ---- Slash command ----
SLASH_DARKMOONFAIRE1 = "/faire"
SLASH_DARKMOONFAIRE2 = "/dmf"
SlashCmdList["DARKMOONFAIRE"] = function() printSummary() end

-- ---- Login reminder: once per session, after the calendar is ready ----
-- 3.3.5a has no C_Timer, so poll on OnUpdate until computeState() is ready (or give up).
local reminded = false
local poll = CreateFrame("Frame")
local pollElapsed = 0
local function startReminderPoll()
    if reminded then return end
    pollElapsed = 0
    poll:SetScript("OnUpdate", function(self, elapsed)
        pollElapsed = pollElapsed + elapsed
        local st = computeState()
        if st then
            reminded = true
            self:SetScript("OnUpdate", nil)
            DEFAULT_CHAT_FRAME:AddMessage(PREFIX .. "in |cff00ff00" .. st.current.name
                .. "|r -- moves to " .. st.nextLoc.name .. " in " .. st.countdown .. ".")
        elseif pollElapsed > 15 then
            self:SetScript("OnUpdate", nil)  -- calendar never became ready; stay silent
        end
    end)
end

-- ---- Init: saved vars + minimap position on load; reminder on entering world ----
local loader = CreateFrame("Frame")
loader:RegisterEvent("ADDON_LOADED")
loader:RegisterEvent("PLAYER_ENTERING_WORLD")
loader:SetScript("OnEvent", function(self, event, arg1)
    if event == "ADDON_LOADED" and arg1 == "DarkmoonFaire" then
        DarkmoonFaireDB = DarkmoonFaireDB or {}
        UpdatePos()
        self:UnregisterEvent("ADDON_LOADED")
    elseif event == "PLAYER_ENTERING_WORLD" then
        startReminderPoll()
    end
end)
