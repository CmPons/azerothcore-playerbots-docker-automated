-- Raid combat API2. C'Thun: eye-beam spacing OR glare escape/tentacles; no formation slots.
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
-- Fifteen-yard minimum, with a17-yard warning buffer; settle at18.
-- Route following only brings clear bots into the room; attack range never gates it.
local MIN, CLEAR = 15*15, 18*18
local separating = {}
local function neighbors(s,index)
    local m=s.members[index]
    local list,cost,nearest,otherIndex={},0,1e9,nil
    for j,o in ipairs(s.members) do
        if j~=index and o.alive and not aura(o,26476) then
            local x,y,z=m.x-o.x,m.y-o.y,m.z-o.z
            local d=x*x+y*y+z*z
            if d<nearest then nearest,otherIndex=d,j end
            if d<21*21 then
                list[#list+1]={x=x,y=y,z=z,d=d}
                cost=cost+math.max(0,CLEAR-d)
            end
        end
    end
    return list,cost,nearest,otherIndex
end
local function measure(list,dx,dy,dz)
    local cost,nearest=0,1e9
    local length=dx*dx+dy*dy+dz*dz
    for _,o in ipairs(list) do
        local dot=o.x*dx+o.y*dy+o.z*dz
        -- Check the whole proposed segment against already-clear neighbours.
        if o.d>=MIN and length>0 then
            local t=math.max(0,math.min(1,-dot/length))
            if o.d+2*t*dot+t*t*length<MIN then return nil end
        end
        local d=o.d+2*dot+length
        cost=cost+math.max(0,CLEAR-d)
        if d<nearest then nearest=d end
    end
    return cost,nearest
end

local state,glare="eye_beam_avoidance",nil
local function resetGlare()
    state,glare="eye_beam_avoidance",nil
end
local function observeState(s)
    if s.map~=531 or not s.combat then resetGlare(); return end
    local eye
    for _,e in ipairs(s.entities) do if e.entry==15589 then eye=e; break end end
    if eye and (not eye.alive or eye.health<=0) then resetGlare(); return end
    local red=eye and (aura(eye,22518) or (eye.casting and eye.cast_spell==26029))
    if red then
        if not glare or glare.guid~=eye.guid then
            glare={guid=eye.guid,facing=eye.facing,at=s.sampled_at,direction=0,running={}}
            separating={}
        else
            local delta=math.atan(math.sin(eye.facing-glare.facing),math.cos(eye.facing-glare.facing))
            local elapsed=s.sampled_at-glare.at
            if elapsed<=0 or elapsed>1500 or math.abs(delta)>0.3 then glare.direction=0
            elseif math.abs(delta)>0.01 then glare.direction=delta<0 and -1 or 1 end
        end
        state="glare_avoidance"
        glare.eye,glare.facing,glare.at=eye,eye.facing,s.sampled_at
    elseif eye and not eye.aura_gap then
        -- Complete observed aura list + no glare cast confirms the return to green beams.
        if glare then separating={} end
        resetGlare()
    end
    -- Missing/truncated observations are not evidence the glare ended. Never steer from
    -- a stale facing indefinitely; retain the state but hold until fresh evidence returns.
end
local function glareGoal(m,eye,index)
    local radius=distance(m,eye)
    local bearing=math.atan(m.y-eye.y,m.x-eye.x)
    local relative=math.atan(math.sin(bearing-eye.facing),math.cos(bearing-eye.facing))
    local ahead=glare.direction~=0 and relative*glare.direction>0
    local trigger=ahead and 1.3 or (glare.direction==0 and 1.15 or 0.5)
    local clearance=ahead and 1.8 or (glare.direction==0 and 1.65 or 0.9)
    if math.abs(relative)>=clearance then glare.running[m.guid]=nil end
    if radius>=8 and math.abs(relative)>=trigger and not glare.running[m.guid] then return end
    glare.running[m.guid]=true
    local side=relative<0 and -1 or 1
    if radius<8 then
        local angle=eye.facing+(index%2==0 and 1 or -1)*math.pi/2
        return {m.x+3*math.cos(angle),m.y+3*math.sin(angle),m.z}
    end
    local angle=bearing+side*math.min(0.3,math.max(0,clearance-math.abs(relative)))
    return {eye.x+radius*math.cos(angle),eye.y+radius*math.sin(angle),m.z}
end
local function tentacle(s,m,index)
    local range=m.melee and 4.5 or 28
    local mask=1 << (index-1)
    local best,bestPriority,bestDistance=0,0,range*range
    for i,e in ipairs(s.entities) do
        local priority=(e.entry==15726 or e.entry==15334) and 2 or
                       ((e.entry==15725 or e.entry==15728) and 1 or 0)
        if priority>0 and e.alive and e.health>0 and e.attackable and e.engaged and
           (e.visible_to & mask)~=0 and ((e.los_known & mask)==0 or (e.los_to & mask)~=0) and
           math.abs(e.z-m.z)<5 then
            local d=(m.x-e.x)^2+(m.y-e.y)^2
            if d<=range*range and (priority>bestPriority or (priority==bestPriority and d<bestDistance)) then
                best,bestPriority,bestDistance=i,priority,d
            end
        end
    end
    return best
end
local function glarePlan(s)
    local out,running={},{}
    for _,m in ipairs(s.members) do running[m.guid]=glare.running[m.guid] end
    glare.running=running -- Bounded to the current roster.
    local eye=(s.sampled_at>=glare.at and s.sampled_at-glare.at<=1500) and glare.eye or nil
    for i,m in ipairs(s.members) do
        local intent=release(); out[i]=intent
        if m.eligible and m.alive and not m.human and not aura(m,26476) and
           m.z>98 and m.z<112 and (m.x+8578.79)^2+(m.y-1986.18)^2<120*120 then
            intent.movement=1
            -- Do not run the spacing or entry policy during glare, even in a clump.
            if m.z<104 and (m.x+8578.79)^2+(m.y-1986.18)^2<55*55 then
                if eye then
                    local p=glareGoal(m,eye,i)
                    if p then goal(intent,p[1],p[2],p[3]) end
                end
                if not m.healer then intent.target=tentacle(s,m,i) end
            end
        end
    end
    return out
end
local function eyeBeamPlan(s)
    local out,nextSeparating,paths={}, {}, {}
    local humanInRoom,humanDeep=false,false
    for i,m in ipairs(s.members) do
        local point,progress=entry(m)
        paths[i]={point=point,progress=progress}
        if m.human and m.alive and m.z>98 and m.z<104 and
           m.x>-8638 and m.x<-8530 and m.y>1964 and m.y<2032 then
            humanInRoom=true
            if (m.x+8578.79)^2+(m.y-1986.18)^2<30*30 then humanDeep=true end
        end
    end
    for i,m in ipairs(s.members) do
        local intent=release(); out[i]=intent
        if s.map==531 and m.eligible and m.alive and not m.human and not aura(m,26476) and
           m.z>98 and m.z<112 and (m.x+8578.79)^2+(m.y-1986.18)^2<120*120 then
            intent.movement=1 -- Own the hold: ordinary follow must not undo spacing.
            local nearby,cost,near,j=neighbors(s,i)
            if near<17*17 or (separating[m.guid] and near<CLEAR) then
                nextSeparating[m.guid]=true
                local o=s.members[j]
                local dx,dy=m.x-o.x,m.y-o.y
                local length=math.sqrt(dx*dx+dy*dy)
                if length<0.01 then
                    local angle=2*math.pi*(i-1)/#s.members
                    dx,dy=math.cos(angle),math.sin(angle)
                else dx,dy=dx/length,dy/length end
                local best,bestNear,bestCost=nil,near,cost
                for k=1,2 do
                    local vx,vy=dx*3,dy*3
                    if k==2 then local side=i%2==0 and 1 or -1; vx,vy=-dy*3*side,dx*3*side end
                    local score,gap=measure(nearby,vx,vy,0)
                    if score and (score<bestCost-0.01 or (math.abs(score-bestCost)<0.01 and gap>bestNear)) then
                        best,bestCost,bestNear={m.x+vx,m.y+vy,m.z},score,gap
                    end
                end
                if best then goal(intent,best[1],best[2],best[3]) end
            else
                local p,progress=paths[i].point,paths[i].progress
                local advance=humanInRoom
                if m.x>=-8620 and m.z<=104 then
                    -- Finish running INTO the room, not merely into attack range.
                    p=nil
                    if humanDeep and (m.x+8578.79)^2+(m.y-1986.18)^2>22*22 then
                        p={-8578.79,1986.18,m.z}
                    end
                end
                for h,human in ipairs(s.members) do
                    if human.human and human.alive and progress and paths[h].progress and
                       paths[h].progress>progress+15 then advance=true end
                end
                if p and advance then
                    local dx,dy,dz=p[1]-m.x,p[2]-m.y,p[3]-m.z
                    local step=near<21*21 and 1 or 3
                    local scale=math.min(1,step/math.max(0.01,math.sqrt(dx*dx+dy*dy+dz*dz)))
                    dx,dy,dz=dx*scale,dy*scale,dz*scale
                    if measure(nearby,dx,dy,dz) then
                        goal(intent,m.x+dx,m.y+dy,m.z+dz)
                    end
                end
            end
        end
    end
    separating=nextSeparating
    return out
end
return {api=2, plan=function(s)
    local melee=viscidus(s)
    if melee then resetGlare(); return melee end
    observeState(s)
    if state=="glare_avoidance" then return glarePlan(s) end
    return eyeBeamPlan(s)
end}
