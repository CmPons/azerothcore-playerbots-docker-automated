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
    local best, point, progress, travelled = 12, nil, nil, 0
    for i=2,#route do
        local a,b = route[i-1],route[i]
        local dx,dy,dz = b[1]-a[1],b[2]-a[2],b[3]-a[3]
        local length = math.sqrt(dx*dx+dy*dy+dz*dz)
        local t = math.max(0,math.min(1,((member.x-a[1])*dx+(member.y-a[2])*dy+(member.z-a[3])*dz)/(length*length)))
        local gap = math.sqrt((member.x-a[1]-dx*t)^2+(member.y-a[2]-dy*t)^2+(member.z-a[3]-dz*t)^2)
        if gap < best and math.abs(member.z-a[3]-dz*t) < 4 then
            best,point = gap,(t*length > length-4 and route[i+1]) or b
            progress = travelled+t*length
        end
        travelled = travelled+length
    end
    return point,progress
end
-- A tighter entry gap, with one-yard approach goals near the preceding member.
local function entryClear(s, paths, index)
    local member, progress, nearest = s.members[index],paths[index].progress,1e9
    for j,other in ipairs(s.members) do
        if j ~= index and other.alive and math.abs(other.z-member.z) < 12 then
            local ahead = paths[j].progress
            if other.human or (other.x >= -8620 and other.z > 98 and other.z < 104) or
               (ahead and (ahead > progress+0.75 or (math.abs(ahead-progress) <= 0.75 and j < index))) then
                nearest=math.min(nearest,distance(member,other))
            end
        end
    end
    return nearest >= 17,nearest
end
-- Reserve a human sector and alternate healers/melee on the inner ring. Spread ranged
-- around the outer ring. Keep dead/CC members' places and freeze the anchor during combat.
local formationEye,formationAngle
local function formationSlots(s, eye)
    local humans,healers,melee,ranged={},{},{},{}
    for i,m in ipairs(s.members) do
        local list=m.human and humans or (m.healer and healers or (m.melee and melee or ranged))
        list[#list+1]=i
    end
    if formationEye ~= eye.guid or not s.combat then
        formationEye,formationAngle=eye.guid,math.pi
        for _,i in ipairs(humans) do
            local m=s.members[i]
            if m.alive then formationAngle=math.atan(m.y-eye.y,m.x-eye.x); break end
        end
    end
    local inner,slots={},{}
    for _,i in ipairs(humans) do inner[#inner+1]=i end
    for j=1,math.max(#healers,#melee) do
        if healers[j] then inner[#inner+1]=healers[j] end
        if melee[j] then inner[#inner+1]=melee[j] end
    end
    for j,i in ipairs(inner) do
        local angle=formationAngle+2*math.pi*(j-1)/#inner
        slots[i]={angle=angle,radius=22,x=eye.x+22*math.cos(angle),y=eye.y+22*math.sin(angle)}
    end
    for j,i in ipairs(ranged) do
        local angle=formationAngle+2*math.pi*(j-0.5)/#ranged
        slots[i]={angle=angle,radius=40,x=eye.x+40*math.cos(angle),y=eye.y+40*math.sin(angle)}
    end
    return slots
end
-- Score the next short step, not merely the final station. Spacing wins over travel;
-- living-healer proximity also constrains travel. Distances include nearby stair occupants.
local function roomScore(s, index, x, y, gx, gy)
    local m=s.members[index]
    local crowd,heal,fx,fy,coincident,hx,hy=0,1e9,0,0,false,nil,nil
    for j,o in ipairs(s.members) do
        if j ~= index and o.alive and math.abs(o.z-m.z) < 17 then
            local dx,dy,dz=x-o.x,y-o.y,m.z-o.z
            local d2=dx*dx+dy*dy+dz*dz
            if d2 < 17*17 then
                crowd=crowd+17*17-d2
                coincident=coincident or d2 < 0.25
                fx,fy=fx+dx/math.max(0.25,d2),fy+dy/math.max(0.25,d2)
            end
            if o.healer and d2 < heal then heal,hx,hy=d2,o.x,o.y end
        end
    end
    local uncovered=heal < 1e9 and math.max(0,heal-36*36) or 0
    return crowd*10000+uncovered*100+(x-gx)^2+(y-gy)^2,crowd,uncovered,fx,fy,coincident,hx,hy
end
local function roomGoal(s, index, eye, station)
    local m=s.members[index]
    local gx,gy=station.x,station.y
    local stationGap=(m.x-gx)^2+(m.y-gy)^2
    local bearing=math.atan(m.y-eye.y,m.x-eye.x)
    local delta=math.atan(math.sin(station.angle-bearing),math.cos(station.angle-bearing))
    if stationGap > 6*6 and math.abs(delta) > 0.22 then
        -- Travel around the outside before peeling into an inner station; don't cut through the raid.
        local angle=bearing+math.max(-0.25,math.min(0.25,delta))
        gx,gy=eye.x+40*math.cos(angle),eye.y+40*math.sin(angle)
    end
    local score,crowd,uncovered,fx,fy,coincident,hx,hy=roomScore(s,index,m.x,m.y,gx,gy)
    if stationGap <= 0.5*0.5 and crowd==0 and uncovered==0 then return nil end
    local dx,dy=gx-m.x,gy-m.y
    if crowd > 0 then
        dx,dy=fx,fy
        if coincident or dx*dx+dy*dy < 0.0001 then dx,dy=math.cos(station.angle),math.sin(station.angle) end
    elseif uncovered > 0 and hx then
        dx,dy=hx-m.x,hy-m.y
    end
    local length=math.sqrt(dx*dx+dy*dy)
    if length < 0.01 then return nil end
    local step=math.min(2.5,length)
    dx,dy=dx/length,dy/length
    local best
    for k=1,2 do
        local vx,vy=dx,dy
        if k==2 then
            -- Consistent detour side avoids alternating left/right behind a settled neighbour.
            local side=index%2==0 and 1 or -1
            vx,vy=-dy*side,dx*side
        end
        local x,y=m.x+step*vx,m.y+step*vy
        local ex,ey=x-eye.x,y-eye.y
        local radius=math.sqrt(ex*ex+ey*ey)
        if radius > 0.01 then
            local bounded=math.max(18,math.min(40,radius))
            x,y=eye.x+ex*bounded/radius,eye.y+ey*bounded/radius
            local candidate=roomScore(s,index,x,y,gx,gy)-8
            if candidate < score then best,score={x,y,100.446},candidate end
        end
    end
    return best
end
-- Infer sweep direction only from observed facing changes, never from a hidden boss timer.
local glare
local function observeGlare(s, eye)
    if not eye or not aura(eye,22518) then glare=nil; return false end
    if not glare or glare.guid ~= eye.guid then
        glare={guid=eye.guid,facing=eye.facing,at=s.sampled_at,direction=0,running={},radii={}}
    else
        local delta=math.atan(math.sin(eye.facing-glare.facing),math.cos(eye.facing-glare.facing))
        local elapsed=s.sampled_at-glare.at
        if elapsed <= 0 or elapsed > 2000 or math.abs(delta) > 0.3 then glare.direction=0
        elseif math.abs(delta) > 0.01 then glare.direction=delta < 0 and -1 or 1 end
        glare.facing,glare.at=eye.facing,s.sampled_at
    end
    -- Retain only this copied roster's values, bounded to at most40 members.
    local running,radii={},{}
    for _,m in ipairs(s.members) do
        running[m.guid],radii[m.guid]=glare.running[m.guid],glare.radii[m.guid]
    end
    glare.running,glare.radii=running,radii
    return true
end
local function glareGoal(member, eye)
    local bearing=math.atan(member.y-eye.y,member.x-eye.x)
    local relative=math.atan(math.sin(bearing-eye.facing),math.cos(bearing-eye.facing))
    local ahead=glare.direction ~= 0 and relative*glare.direction > 0
    local trigger=ahead and 1.3 or (glare.direction==0 and 1.15 or 0.5)
    local clearance=ahead and 1.8 or (glare.direction==0 and 1.65 or 0.9)
    if math.abs(relative) >= clearance then glare.running[member.guid]=nil end
    if math.abs(relative) >= trigger and not glare.running[member.guid] then return nil end
    glare.running[member.guid]=true
    -- Always escape away from the current beam, even if it has overtaken a delayed runner.
    local side=relative < 0 and -1 or 1
    local angle=bearing+side*math.min(0.3,clearance-math.abs(relative))
    local radius=glare.radii[member.guid] or math.max(20,math.min(40,distance(member,eye)))
    glare.radii[member.guid]=radius
    return {eye.x+radius*math.cos(angle),eye.y+radius*math.sin(angle),100.446}
end
-- Kill nearby eye stalks without undoing the working entrance/spacing movement.
-- Only select visible, already-engaged enemies. Unknown LOS is left to native cast checks.
local function eyeTentacle(s, member, memberIndex)
    local range=(member.melee and not member.healer) and 4.5 or 28
    local best,limit=0,range*range
    local mask=1 << (memberIndex-1)
    for i,e in ipairs(s.entities) do
        if (e.entry==15726 or e.entry==15334) and e.alive and e.health > 0 and
           e.attackable and e.engaged and (e.visible_to & mask) ~= 0 and
           ((e.los_known & mask)==0 or (e.los_to & mask) ~= 0) and math.abs(e.z-member.z) < 5 then
            local dx,dy=member.x-e.x,member.y-e.y
            local d2=dx*dx+dy*dy
            if d2 <= limit then best,limit=i,d2 end
        end
    end
    return best
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
    local out, eye, eyeIndex, committed, approaching = {},nil,0,false,false
    local paths = {}
    for i,e in ipairs(s.entities) do
        if e.entry == 15589 and e.alive and e.health > 0 then eye,eyeIndex=e,i end
    end
    local red=observeGlare(s,eye)
    local stations=eye and formationSlots(s,eye) or {}
    -- No entity observation is a whole-map absence claim. Outside this encounter leave native AI alone.
    for i,m in ipairs(s.members) do
        local point,progress = entry(m)
        paths[i] = {point=point,progress=progress}
        if m.human and m.alive and point then approaching=true end
        if m.human and m.alive and m.z > 98 and m.z < 104 and
           m.x > -8638 and m.x < -8530 and m.y > 1964 and m.y < 2032 then committed=true end
    end
    for i,m in ipairs(s.members) do
        local intent = release()
        out[i] = intent
        if s.map == 531 and eye and m.eligible and (s.combat or committed or approaching) and
           not aura(m,26476) and distance(m,eye) < 160 then
            if m.x < -8620 or m.z > 104 then
                local p=paths[i].point
                if p then
                    intent.movement=1
                    local clear,gap=entryClear(s,paths,i)
                    if (s.combat or committed) and clear then
                        if gap < 21 then
                            local dx,dy,dz=p[1]-m.x,p[2]-m.y,p[3]-m.z
                            local length=math.sqrt(dx*dx+dy*dy+dz*dz)
                            local step=1/math.max(1,length)
                            goal(intent,m.x+dx*step,m.y+dy*step,m.z+dz*step)
                        else goal(intent,p[1],p[2],p[3]) end
                    end
                end
            elseif (s.combat or committed) and m.z > 98 and m.z < 104 then
                intent.movement=1
                if red then
                    -- Short arc waypoints avoid large chords through the boss/beam. Keep escaping
                    -- until well clear, rather than stopping at the edge of the old narrow trigger.
                    local p=glareGoal(m,eye)
                    if p then goal(intent,p[1],p[2],p[3]) end
                else
                    local p=roomGoal(s,i,eye,stations[i])
                    if p then goal(intent,p[1],p[2],p[3]) end
                end
            end
            if eye.attackable and eye.engaged and not m.healer then intent.target=eyeIndex end
        end
        -- Surface-room support also works when the central Eye is absent during body phase.
        -- No stomach, entrance, human or healer retargeting; movement remains exactly as above.
        if s.map==531 and s.combat and m.eligible and not m.healer and
           m.z > 98 and m.z < 104 and m.x > -8620 and not aura(m,26476) and
           (m.x+8578.79)^2+(m.y-1986.18)^2 < 55*55 then
            local tentacle=eyeTentacle(s,m,i)
            if tentacle ~= 0 then intent.target=tentacle end
        end
    end
    return out
end}
