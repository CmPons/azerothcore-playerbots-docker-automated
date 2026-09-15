-- Raid combat API2. C'Thun is temporarily a proximity-movement diagnostic, NOT a boss strategy.
local function release()
    return {movement=0, target=0, operation=0, spell=0, aura=0, action_target=0}
end
local function goal(out, x, y, z)
    out.movement, out.x, out.y, out.z = 2, x, y, z
end
local function aura(unit, id)
    for _, a in ipairs(unit.auras) do if a.spell == id then return true end end
    return false -- aura_gap means not observed, not proof of absence
end
local function distance(a, b)
    return math.sqrt((a.x-b.x)^2 + (a.y-b.y)^2)
end
-- Viscidus: first live iteration deliberately favors sustained melee, not cloud avoidance.
-- Approach on the bot's current side, then hold; boss-facing changes do not reshuffle slots.
local function viscidus(s)
    if s.map ~= 531 or not s.combat then return nil end
    local boss, index
    for i,e in ipairs(s.entities) do
        if e.entry == 15299 and e.alive and e.health > 0 and e.attackable and e.engaged and
           not aura(e,25905) then boss,index=e,i; break end
    end
    if not boss then return nil end
    local out = {}
    for i,m in ipairs(s.members) do
        local intent = release()
        out[i] = intent
        local dx,dy,dz = m.x-boss.x,m.y-boss.y,m.z-boss.z
        local horizontal = math.sqrt(dx*dx+dy*dy)
        if m.eligible and m.alive and m.melee and not m.healer and horizontal < 80 and math.abs(dz) < 6 then
            intent.target = index
            intent.movement = 1
            if dx*dx+dy*dy+dz*dz > 4.5*4.5 then
                local divisor = math.max(horizontal,0.01)
                goal(intent,boss.x+3.5*dx/divisor,boss.y+3.5*dy/divisor,boss.z)
            end
        end
    end
    return out
end
local fleeing = {}
return {api=2, plan=function(s)
    local melee = viscidus(s)
    if melee then return melee end
    local out,nextFleeing = {},{}
    for i,m in ipairs(s.members) do
        local intent = release()
        out[i] = intent
        -- C'Thun room/approach only. No boss observation or combat requirement.
        if s.map == 531 and m.eligible and m.alive and not m.human and
           m.z > 98 and m.z < 112 and (m.x+8578.79)^2+(m.y-1986.18)^2 < 120*120 then
            intent.movement = 1 -- Stay put when clear; don't immediately follow back into the human.
            local human,nearest = nil,1e9
            for _,other in ipairs(s.members) do
                if other.human and other.alive and math.abs(other.z-m.z) < 6 then
                    local d = distance(m,other)
                    if d < nearest then human,nearest = other,d end
                end
            end
            if human and (nearest < 15 or (fleeing[m.guid] and nearest < 20)) then
                nextFleeing[m.guid] = true
                local dx,dy = m.x-human.x,m.y-human.y
                if nearest < 0.01 then
                    local angle = 2*math.pi*(i-1)/#s.members
                    dx,dy = math.cos(angle),math.sin(angle)
                else dx,dy = dx/nearest,dy/nearest end
                goal(intent,m.x+3*dx,m.y+3*dy,m.z)
            end
        end
    end
    fleeing = nextFleeing -- Only current roster state is retained, at most40 entries.
    return out
end}
