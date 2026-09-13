-- API v1: conditional, instance-level tactical choices; C++ remains movement authority.
-- Candidate[1] is the current point; return 0 to hold, -1 to release, index-1 to propose.
return { api = 1, plan = function(s)
    local out, chosen = {}, {}
    for i, member in ipairs(s.members) do
        if not member.eligible or (not s.combat and not s.committed) then
            out[i] = -1 -- human, trash/corridor, stomach, DONE, manual/passive/CC
        else
            local current = member.candidates[1]
            local escape = current.glare < 0
            local function score(c)
                local glare = math.max(0, -c.glare) * 100000
                if member.entering then
                    -- Crossing the real west doorway MUST precede spacing. A hard beam-gap
                    -- constraint here traps the raid outside. Crowding is a soft transit cost;
                    -- this deliberate tradeoff cannot promise zero beam damage in the doorway.
                    return glare + c.goal * 20 + c.crowding * 0.02
                end
                if not c.interior then return math.huge end
                local reservations = 0
                for _, prior in ipairs(chosen) do
                    local gap = 12 + member.reach + prior.reach
                    local distance = math.sqrt((c.x-prior.x)^2 + (c.y-prior.y)^2)
                    reservations = reservations + math.max(0, gap-distance)^2
                end
                -- Current observed red facing works for BOTH possible native sweep directions.
                -- No private timers/direction. Escape outranks spacing when both are impossible.
                return glare + (c.crowding + reservations) * (escape and 0.1 or 10)
                    + c.yield^2 * 3 + c.inside * 100 + c.range * 5 + c.goal * 0.1
            end
            local best, value = 0, score(current)
            -- Lua decides whether positioning is needed, rather than a hidden native trigger.
            -- Once safe/in range, don't march to rigid stations merely to reduce the soft goal cost.
            local needs = member.entering or escape or current.crowding > 0.04
                or current.yield > 0.5 or current.range > 0.5 or current.inside > 0.1
            if needs then
                for j = 2, #member.candidates do
                    local c = member.candidates[j]
                    local nextValue = score(c)
                    if nextValue < value - 0.1 then best, value = j-1, nextValue end
                end
            end
            out[i] = best -- hold is not an action success and never consumes healing ticks
            local c = member.candidates[best+1]
            chosen[#chosen+1] = {x=c.x, y=c.y, reach=member.reach}
        end
    end
    return out
end }
