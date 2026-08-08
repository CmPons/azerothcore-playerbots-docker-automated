return function(H)
  local NS = H.load("Comm.lua")
  local C = NS.Comm

  -- buildMessage
  H.eq(C.buildMessage("HELLO", "1"), "HELLO~1", "build with payload")
  H.eq(C.buildMessage("GET", "STATES"), "GET~STATES", "build get")
  H.eq(C.buildMessage("PONG", nil), "PONG", "build no payload")

  -- splitOnce
  local a, b = C.splitOnce("STATE~rest~more", "~")
  H.eq(a, "STATE", "splitOnce head")
  H.eq(b, "rest~more", "splitOnce tail keeps separators")

  -- parseDetail
  local d = C.parseDetail("Boband~Orc~0~Warrior~80~3~5~61~4200")
  H.eq(d.name, "Boband", "detail name")
  H.eq(d.classToken, "WARRIOR", "detail class uppercased token")
  H.eq(d.talent1, 3, "detail talent1")
  H.eq(d.talent3, 61, "detail talent3")
  H.eq(d.score, 4200, "detail score")

  -- parseState
  local s = C.parseState("Boband~tank~follow")
  H.eq(s.name, "Boband", "state name")
  H.eq(s.combat, "tank", "state combat")
  H.eq(s.normal, "follow", "state normal")

  -- parseRosterEntry
  local r = C.parseRosterEntry("Boband,1,80,571,1,93,40")
  H.eq(r.name, "Boband", "roster name")
  H.eq(r.classId, 1, "roster classId")
  H.eq(r.alive, true, "roster alive")
  H.eq(r.hpPct, 93, "roster hpPct")

  -- parseList splits on ';'
  local list = C.parseList("a;b;c")
  H.eq(#list, 3, "parseList count")
  H.eq(list[2], "b", "parseList element")

  -- channelFor: pure decision from injected group-state
  H.eq(C.channelFor({raid=true}), "RAID", "raid channel")
  H.eq(C.channelFor({party=true}), "PARTY", "party channel")
  H.eq(C.channelFor({}), "WHISPER", "solo whisper channel")

  -- buildRun: single bot, BOT scope, url-encoded fields
  H.eq(C.buildRun("COMBAT", "Boband", "do attack my target", 7),
    "RUN~COMBAT~BOT~Boband~7~do attack my target", "buildRun single bot BOT scope")
  H.eq(C.urlEncode("a~b%c"), "a%7Eb%25c", "urlEncode escapes ~ and %")

  -- nextToken increments
  C._token = 0
  H.eq(C.nextToken(), 1, "token 1")
  H.eq(C.nextToken(), 2, "token 2")
end
