local Base = require("spellEffect")
local Script = setmetatable({}, {__index = Base})
Script.__index = Script

-- Raw EWallState values from lib/constants/Enumerations.h
local WALL_DESTROYED  = 0
local WALL_DAMAGED    = 1
local WALL_INTACT     = 2
local WALL_REINFORCED = 3

local WALLS  = { ENUM.WallPart.bottomWall, ENUM.WallPart.belowGate,
                 ENUM.WallPart.overGate,   ENUM.WallPart.upperWall }
local TOWERS = { ENUM.WallPart.bottomTower, ENUM.WallPart.keep,
                 ENUM.WallPart.upperTower }
local GATE   = ENUM.WallPart.gate

local function isTower(part)
	return part == ENUM.WallPart.keep
	    or part == ENUM.WallPart.bottomTower
	    or part == ENUM.WallPart.upperTower
end

function Script:hitChance(part)
	if part == GATE                  then return self.chanceToHitGate  or 0 end
	if part == ENUM.WallPart.keep    then return self.chanceToHitKeep  or 0 end
	if part == ENUM.WallPart.bottomTower
	   or part == ENUM.WallPart.upperTower then return self.chanceToHitTower or 0 end
	return self.chanceToHitWall or 0
end

function Script:randomDamage(server)
	local crit  = math.min(100, math.max(0, self.chanceToCrit       or 0))
	local hit   = math.min(100 - crit, math.max(0, self.chanceToNormalHit or 0))
	local noDmg = 100 - hit - crit
	local chances = { noDmg, hit, crit }
	local r, acc = server:rngInt(0, 99), 0
	for dmg = 0, 2 do
		acc = acc + chances[dmg + 1]
		if r < acc then return dmg end
	end
	return 0
end

-- Mirror of C++ SiegeInfo::applyDamage: each damage point lowers the wall state by one level.
function Script:applyWallDamage(state, damage)
	while damage > 0 do
		if state == WALL_REINFORCED then
			state = WALL_INTACT
		elseif state == WALL_INTACT then
			state = WALL_DAMAGED
		elseif state == WALL_DAMAGED then
			state = WALL_DESTROYED
		end
		damage = damage - 1
	end
	return state
end

function Script:potentialTargets(battle, bypassGate, bypassTower)
	local list = {}
	for _, p in ipairs(WALLS) do
		if battle:isWallPartAttackable(p) then list[#list+1] = p end
	end
	if (#list == 0 or bypassGate) and battle:isWallPartAttackable(GATE) then
		list[#list+1] = GATE
	end
	if #list == 0 or bypassTower then
		for _, p in ipairs(TOWERS) do
			if battle:isWallPartAttackable(p) then list[#list+1] = p end
		end
	end
	return list
end

function Script:applicableGeneral(mechanics, problem)
	local battle = mechanics:getBattle()
	if not battle:hasFortifications()
	   or (mechanics:isSmart() and mechanics:getCasterSide() ~= ENUM.BattleSide.attacker)
	   or #self:potentialTargets(battle, true, true) == 0 then
		problem:addStandard(mechanics, ENUM.SpellCastProblem.noAppropriateTarget)
		return false
	end
	return true
end

-- Look up the unit sitting on a tower; returns its ID or -1.
function Script:towerShooterIdIfDestroyed(battle, part, damage)
	if not isTower(part) then return -1 end
	local stateBefore = battle:getWallState(part)
	if stateBefore == WALL_DESTROYED then return -1 end
	local stateAfter = self:applyWallDamage(stateBefore, damage)
	if stateAfter ~= WALL_DESTROYED then return -1 end

	local towerHex = battle:getTowerShooterHex(part)
	local unit = battle:getUnitByPos(towerHex, false)
	if not unit or unit:isGhost() then return -1 end
	return unit:unitId()
end

function Script:fireAt(server, battle, battleID, attacker, part, damage)
	local killedId = self:towerShooterIdIfDestroyed(battle, part, damage)
	local destinationTile = battle:wallPartToBattleHex(part)
	server:catapultAttack(battleID, attacker, part, destinationTile, damage, killedId)
end

function Script:apply(mechanics, server, target)
	local battle    = mechanics:getBattle()
	local battleID  = mechanics:getBattleID()
	local attacker  = mechanics:getUnitCaster()
	local N         = self.targetsToAttack or 0

	if mechanics:isMassive() then
		local pool = self:potentialTargets(battle, true, true)
		if #pool == 0 then return end
		local damagePerPart, order = {}, {}
		for _ = 1, N do
			local part = pool[server:rngInt(1, #pool)]
			if not damagePerPart[part] then
				order[#order+1] = part
				damagePerPart[part] = 0
			end
			damagePerPart[part] = damagePerPart[part] + self:randomDamage(server)
		end
		for _, part in ipairs(order) do
			self:fireAt(server, battle, battleID, attacker, part, damagePerPart[part])
		end
	else
		local desired = battle:hexToWallPart(target[1].hex)
		for _ = 1, N do
			local actual
			if battle:isWallPartAttackable(desired)
			   and server:rngInt(0, 99) < self:hitChance(desired) then
				actual = desired
			else
				local pool = self:potentialTargets(battle, false, false)
				if #pool == 0 then break end
				actual = pool[server:rngInt(1, #pool)]
			end
			self:fireAt(server, battle, battleID, attacker, actual, self:randomDamage(server))
		end
	end
end

return Script
