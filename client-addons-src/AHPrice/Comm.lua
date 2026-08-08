local NS = _G.AHPriceNS or {}
_G.AHPriceNS = NS

local C = {}
NS.Comm = C

C.serverPrefix = "AzerothCore"
C._pending = {}   -- echo -> { onLine=fn, onDone=fn }
C._n = 0
C.debug = false

-- Diagnostic trace to the default chat frame (toggled by /ahprice debug). No-op
-- unless debug is on, so it stays inert in headless tests and in normal play.
function C.log(dir, text)
  if C.debug and DEFAULT_CHAT_FRAME then
    DEFAULT_CHAT_FRAME:AddMessage("|cff88ccffAHPrice " .. dir .. "|r " .. tostring(text))
  end
end

-- 4-char correlation token (digits, wraps at 10000). Always 4 non-nul chars.
function C.nextEcho()
  C._n = (C._n + 1) % 10000
  return string.format("%04d", C._n)
end

-- Payload the client sends (prefix is added by rawSend): opcode 'i' + echo + command.
function C.buildPayload(echo, command)
  return "i" .. echo .. tostring(command)
end

-- Parse a server reply body (arg2 of CHAT_MSG_ADDON): opcode(1) + echo(4) + body.
function C.parseReply(msg)
  msg = msg or ""
  if #msg < 5 then return nil end
  return {
    opcode = string.sub(msg, 1, 1),
    echo   = string.sub(msg, 2, 5),
    body   = string.sub(msg, 6),
  }
end

-- Register callbacks for an in-flight request keyed by echo.
function C.register(echo, onLine, onDone)
  C._pending[echo] = { onLine = onLine, onDone = onDone }
end

-- Route one CHAT_MSG_ADDON event. Collects 'm' bodies; 'o'/'f' finish + clear.
function C.dispatch(prefix, msg)
  C.log("RX", tostring(prefix) .. " | " .. tostring(msg))
  if prefix ~= C.serverPrefix then return end
  local r = C.parseReply(msg)
  if not r then return end
  local p = C._pending[r.echo]
  if not p then return end
  if r.opcode == "m" then
    if p.onLine then p.onLine(r.body) end
  elseif r.opcode == "o" then
    C._pending[r.echo] = nil
    if p.onDone then p.onDone(true) end
  elseif r.opcode == "f" then
    C._pending[r.echo] = nil
    if p.onDone then p.onDone(false) end
  end
  -- opcode 'a' (ack): ignored.
end

-- Live send (wraps SendAddonMessage; no-op-safe headless). Registers callbacks,
-- then sends. Only one request per echo; a new query uses a fresh echo.
function C.request(command, onLine, onDone)
  local echo = C.nextEcho()
  C.register(echo, onLine, onDone)
  if type(SendAddonMessage) ~= "function" then C.log("TX", "ERROR: SendAddonMessage unavailable"); return echo end
  local me = UnitName and UnitName("player")
  if not me then C.log("TX", "ERROR: UnitName('player') is nil"); return echo end
  local payload = C.buildPayload(echo, command)
  C.log("TX", payload .. "   (WHISPER -> " .. me .. ")")
  SendAddonMessage(C.serverPrefix, payload, "WHISPER", me)
  return echo
end
