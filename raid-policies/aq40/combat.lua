-- Raid combat API2. Self-contained helpers: edit this file, check, publish-default once.
-- Movement: 0 release, 1 hold, 2 ground room goal. Native code chooses/checks short steps.
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
-- Landmarks from the existing entry evidence. Lua owns the encounter and geometry decisions.
local route = {
    {-8634.93,1913.87,108.979}, {-8641.33,1914.40,108.979},
    {-8652.53,1921.60,108.979}, {-8661.33,1941.33,108.979},
    {-8663.47,1950.40,108.979}, {-8640,1962.67,100.713},
    {-8625,1974,100.713}, {-8612,1980,100.446}
}
local function entry(member)
    local best, point = 12, nil
    for i=2,#route do
        local a,b = route[i-1],route[i]
        local dx,dy,dz = b[1]-a[1],b[2]-a[2],b[3]-a[3]
        local length = math.sqrt(dx*dx+dy*dy+dz*dz)
        local t = math.max(0,math.min(1,((member.x-a[1])*dx+(member.y-a[2])*dy+(member.z-a[3])*dz)/(length*length)))
        local gap = math.sqrt((member.x-a[1]-dx*t)^2+(member.y-a[2]-dy*t)^2+(member.z-a[3]-dz*t)^2)
        if gap < best and math.abs(member.z-a[3]-dz*t) < 4 then
            best,point = gap,(t*length > length-4 and route[i+1]) or b
        end
    end
    return point
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
return {api=2, plan=function(s)
    local melee = viscidus(s)
    if melee then return melee end
    local out, eye, eyeIndex, committed, count = {},nil,0,false,0
    for i,e in ipairs(s.entities) do
        if e.entry == 15589 and e.alive and e.health > 0 then eye,eyeIndex=e,i end
    end
    -- No entity observation is a whole-map absence claim. Outside this encounter leave native AI alone.
    for _,m in ipairs(s.members) do
        if m.eligible then count=count+1 end
        if m.human and m.z > 98 and m.z < 104 and
           m.x > -8638 and m.x < -8530 and m.y > 1964 and m.y < 2032 then committed=true end
    end
    local slot = 0
    for i,m in ipairs(s.members) do
        local intent = release()
        out[i] = intent
        if s.map == 531 and eye and m.eligible and (s.combat or committed) and
           not aura(m,26476) and distance(m,eye) < 160 then
            slot=slot+1
            if m.x < -8620 or m.z > 104 then
                local p=entry(m)
                if p then goal(intent,p[1],p[2],p[3]) end
            elseif m.z > 98 and m.z < 104 then
                local radius = m.melee and not m.healer and 22 or 35
                local angle = 2*math.pi*(slot-1)/math.max(1,count)
                -- Red facing is observed, not a guessed native sweep direction/timer.
                if aura(eye,22518) then
                    local relative = math.atan(m.y-eye.y,m.x-eye.x)-eye.facing
                    relative = math.atan(math.sin(relative),math.cos(relative))
                    if math.abs(relative) < 0.7 then
                        angle = eye.facing+(relative < 0 and -1 or 1)*1.1
                    else
                        angle = math.atan(m.y-eye.y,m.x-eye.x)
                        radius = distance(m,eye)
                    end
                end
                local x,y = eye.x+radius*math.cos(angle),eye.y+radius*math.sin(angle)
                intent.movement=1
                if math.sqrt((m.x-x)^2+(m.y-y)^2) > 2 then goal(intent,x,y,100.446) end
            end
            if eye.attackable and eye.engaged and not m.healer then intent.target=eyeIndex end
        end
    end
    return out
end}
