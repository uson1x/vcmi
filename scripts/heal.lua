local Base = require("unitEffect")
local Script = setmetatable({}, {__index = Base})
Script.__index = Script

local HEAL_LEVEL_FROM_STRING = {
	heal      = ENUM.HealLevel.heal,
	resurrect = ENUM.HealLevel.resurrect,
	overHeal  = ENUM.HealLevel.overheal,
}
local HEAL_POWER_FROM_STRING = {
	oneBattle = ENUM.HealPower.oneBattle,
	permanent = ENUM.HealPower.permanent,
}

local function healLevel(self)
	return HEAL_LEVEL_FROM_STRING[self.healLevel] or ENUM.HealLevel.heal
end
local function healPower(self)
	return HEAL_POWER_FROM_STRING[self.healPower] or ENUM.HealPower.permanent
end
local function minFullUnits(self)
	return self.minFullUnits or 0
end

--- Only injured units are valid; resurrect/overheal levels also accept dead units.
function Script:isValidTarget(mechanics, unit)
	local level = healLevel(self)
	local allowDead = level ~= ENUM.HealLevel.heal
	if not unit:isValidTarget(allowDead) then return false end

	local injuries = unit:getTotalHealth() - unit:getAvailableHealth()
	if injuries <= 0 then return false end

	local mfu = minFullUnits(self)
	if mfu > 0 then
		local hpGained = math.min(mechanics:getEffectValue(), injuries)
		if hpGained < mfu * unit:getMaxHealth() then return false end
	end

	if unit:isDead() then
		local hexes = unit:getHexes()
		for i = 1, hexes:size() do
			local hex = hexes:at(i)
			local blockers = mechanics:getBattle():getUnitsIf(function(other)
				return other ~= unit and other:isValidTarget(false) and other:coversPos(hex)
			end)
			if #blockers > 0 then return false end
		end
	end

	return true
end

--- Returns HP and unit count change for hover tooltip.
function Script:getHealthChange(mechanics, spellTarget)
	local result = { hpDelta = 0, unitsDelta = 0, unitType = -1 }
	for _, dest in ipairs(spellTarget) do
		local unit = dest.unit
		if unit then
			local copy = unit:copy()
			local healedHP, resurrected = copy:heal(mechanics:getEffectValue(), healLevel(self), healPower(self))
			result.hpDelta   = result.hpDelta   + healedHP
			result.unitsDelta = result.unitsDelta + resurrected
			result.unitType  = unit:getCreature():getIndex()
		end
	end
	return result
end

--- Heals each target unit and emits battle log messages.
function Script:apply(mechanics, server, target)
	local battleID    = mechanics:getBattleID()
	local isUnitCaster = mechanics:getHeroCaster() == nil

	for _, dest in ipairs(target) do
		local unit = dest.unit
		if unit then
			local healedHP, resurrected = server:healUnit(
				battleID, unit, mechanics:getEffectValue(), healLevel(self), healPower(self))

			if resurrected > 0 then
				local textID = resurrected == 1 and "core.genrltxt.117" or "core.genrltxt.116"
				local nameTextID = unit:getCreature():getNameTextID(resurrected)
				server:appendLog(battleID, {
					append  = { textID },
					replace = { resurrected, nameTextID }
				})
			elseif healedHP > 0 and isUnitCaster then
				local casterUnit = mechanics:getUnitCaster()
				server:appendLog(battleID, {
					append  = { "core.genrltxt.414" },
					replace = {
						casterUnit:getCreature():getNameSingularTextID(),
						unit:getCreature():getNameSingularTextID(),
						healedHP
					}
				})
			end
		end
	end
end

return Script
