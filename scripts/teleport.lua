local Base = require("unitEffect")
local Script = setmetatable({}, {__index = Base})
Script.__index = Script

--- Enforce that target types are exactly [creature, location].
--- The spell config declares "targetType": "CREATURE"; this adds the destination hex.
function Script:adjustTargetTypes(mechanics, types)
	if #types == 0 then return types end
	if types[1] ~= ENUM.AimType.creature then return {} end
	if #types == 1 then return { ENUM.AimType.creature, ENUM.AimType.location } end
	if types[2] ~= ENUM.AimType.location then return {} end
	return types
end

--- Validate the destination hex (two-target phase only).
--- Single-unit validity and immunity are handled by the C++ UnitEffect layer.
function Script:applicableTarget(mechanics, problem, target)
	local unit    = target[1].unit
	local fromHex = target[1].hex
	local toHex   = target[2].hex

	if not toHex:isValid() or not mechanics:getBattle():isAccessibleForUnit(unit, toHex) then
		problem:addStandard(mechanics, ENUM.SpellCastProblem.wrongSpellTarget)
		return false
	end

	if mechanics:getBattle():hasPenaltyOnLine(fromHex, toHex, not self.isWallPassable, not self.isMoatPassable) then
		problem:addStandard(mechanics, ENUM.SpellCastProblem.wrongSpellTarget)
		return false
	end

	return true
end

--- C++ has already applied immunity filtering to the unit target.
--- Re-attach the destination hex from the original aimPoint.
function Script:transformTarget(mechanics, filteredTarget, aimPoint)
	if #filteredTarget == 0 then return {} end
	if #aimPoint >= 2 then
		filteredTarget[2] = aimPoint[2]
	end
	return filteredTarget
end

--- Move the unit to the destination hex via a teleport (no path, no distance cost).
function Script:apply(mechanics, server, target)
	local unit    = target[1].unit
	local destHex = target[2].hex
	server:moveUnit(mechanics:getBattleID(), unit, destHex, true)
end

return Script
