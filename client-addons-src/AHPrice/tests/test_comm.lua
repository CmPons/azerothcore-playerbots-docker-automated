return function(H)
  local NS = H.load("Comm.lua")
  local C = NS.Comm

  -- nextEcho: exactly 4 chars, changes each call
  local e1 = C.nextEcho()
  H.eq(#e1, 4, "echo is 4 chars")
  local e2 = C.nextEcho()
  H.truthy(e1 ~= e2, "echo advances")

  -- buildPayload: "i" .. echo4 .. command
  H.eq(C.buildPayload("abcd", "ahprice item 2770"), "iabcdahprice item 2770", "payload = i+echo+cmd")

  -- parseReply: opcode(1) + echo(4) + body
  local r = C.parseReply("mWXYZR\t2770\t1\tCopper Ore")
  H.eq(r.opcode, "m", "reply opcode")
  H.eq(r.echo, "WXYZ", "reply echo")
  H.eq(r.body, "R\t2770\t1\tCopper Ore", "reply body")

  local ack = C.parseReply("aWXYZ")
  H.eq(ack.opcode, "a", "ack opcode")
  H.eq(ack.echo, "WXYZ", "ack echo")
  H.eq(ack.body, "", "ack empty body")

  H.eq(C.parseReply("xy"), nil, "too-short reply rejected")

  -- dispatch: collect 'm' bodies by echo, resolve on 'o'
  local lines, done = {}, false
  C._pending = {}
  C.register("WXYZ", function(body) lines[#lines+1] = body end, function(ok) done = ok end)
  C.dispatch("AzerothCore", "aWXYZ")             -- ack, ignored
  C.dispatch("AzerothCore", "mWXYZfirst")
  C.dispatch("AzerothCore", "mWXYZsecond")
  C.dispatch("AzerothCore", "oWXYZ")             -- done OK
  H.eq(#lines, 2, "collected two lines")
  H.eq(lines[1], "first", "first line")
  H.eq(done, true, "done resolved true")

  -- wrong prefix ignored
  local touched = false
  C.register("QQQQ", function() touched = true end, function() end)
  C.dispatch("SomethingElse", "mQQQQx")
  H.eq(touched, false, "non-AzerothCore prefix ignored")

  -- failure path resolves false
  local failed = nil
  C.register("FAIL", function() end, function(ok) failed = ok end)
  C.dispatch("AzerothCore", "fFAIL")
  H.eq(failed, false, "failure resolves false")
end
