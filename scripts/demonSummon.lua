local Base = require("unitEffect")
local Script = setmetatable({}, {__index = Base})
Script.__index = Script

local function raisedCreatureAmount(parameters, mechanics, unit)
	local creatureType = LIBRARY:getCreatureByName(parameters.id)

	local deadCount         = unit:getBaseAmount()
	local deadTotalHealth   = unit:getTotalHealth()
	local raisedMaxHealth   = creatureType:getMaxHealth()
	local raisedTotalHealth = mechanics:applySpellBonus(mechanics:getEffectValue(), unit)

	local maxFromHealth     = math.floor(deadTotalHealth   / raisedMaxHealth)
	local maxFromAmount     = deadCount
	local maxFromSpellpower = math.floor(raisedTotalHealth / raisedMaxHealth)

	return math.min(maxFromHealth, maxFromAmount, maxFromSpellpower)
end

--- A unit is a valid target if it is a dead, non-ghost corpse whose hexes are
--- not blocked by any alive unit, and our spellpower can raise at least one demon.
function Script.isValidTarget(parameters, mechanics, unit)
	if not unit:isDead() then return false end

	local hexes = unit:getHexes()
	for i = 1, hexes:size() do
		local hex = hexes:at(i)
		local blocker = mechanics:getBattle():getAnyUnitIf(function(other)
			return other:isValidTarget(false) and other:coversPos(hex)
		end)
		if blocker ~= nil then return false end
	end

	if unit:isGhost() then return false end

	if raisedCreatureAmount(parameters, mechanics, unit) == 0 then return false end

	return mechanics:isReceptive(unit)
end

--- Raise demons from each target corpse and remove the corpse.
function Script.apply(parameters, mechanics, server, target)
	local creatureType = LIBRARY:getCreatureByName(parameters.id)

	for _, dest in ipairs(target) do
		local targetStack = dest.unit

		if targetStack == nil or not targetStack:isDead() or targetStack:isGhost() then
			print("DemonSummon: no valid corpse for demonization")
			break
		end

		local hex = mechanics:getBattle():getAvailableHex(
			creatureType,
			mechanics:getCasterSide(),
			targetStack:getPosition():toInteger()
		)

		if not hex:isValid() then
			print("DemonSummon: no available hex for summoned unit")
			break
		end

		local finalAmount = raisedCreatureAmount(parameters, mechanics, targetStack)

		if finalAmount < 1 then
			print("DemonSummon: raised zero creatures")
			break
		end

		server:createUnit(
			mechanics:getBattleID(),
			mechanics:getBattle():getNextUnitId(),
			{
				count    = finalAmount,
				type     = creatureType:getJsonKey(),
				side     = mechanics:getCasterSide(),
				position = hex:toInteger(),
				summoned = not parameters.permanent
			}
		)

		server:removeUnit(mechanics:getBattleID(), targetStack)
	end
end

--- Returns the number of demons that would be raised for the hover preview.
function Script.getHealthChange(parameters, mechanics, spellTarget)
	local creatureType = LIBRARY:getCreatureByName(parameters.id)

	for _, dest in ipairs(spellTarget) do
		if dest.unit ~= nil then
			local amount = raisedCreatureAmount(parameters, mechanics, dest.unit)
			return {
				hpDelta    = 0,
				unitsDelta = amount,
				unitType   = creatureType:getIndex()
			}
		end
	end

	return { hpDelta = 0, unitsDelta = 0, unitType = -1 }
end

return Script
