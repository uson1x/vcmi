local MetaString = require("texts.MetaString")

-- expected parameters:
-- - id
-- - permanent
-- - exclusive
-- - summonByHealth
-- - summonSameUnit

local function summonedEffectValue(parameters, mechanics)
	effectPower = mechanics:getEffectPower()
	rawEffectPower = mechanics:calculateRawEffectValue(0, effectPower)
	finaleEffectPower = mechanics:applySpecificSpellBonus(rawEffectPower)
	
	return finaleEffectPower
end

local function summonedCreatureHealth(parameters, mechanics)
	local valueWithBonus = summonedEffectValue(parameters, mechanics)
	local creature = LIBRARY:creatures():getByName(parameters.id)
	if parameters.summonByHealth then
		return valueWithBonus
	else
		return valueWithBonus * creature:getMaxHealth()
	end
end

local function summonedCreatureAmount(parameters, mechanics)
	local valueWithBonus = summonedEffectValue(parameters, mechanics)
	local creature = LIBRARY:creatures():getByName(parameters.id)
	if parameters.summonByHealth then
		return math.floor(valueWithBonus / creature:getMaxHealth())
	else
		return valueWithBonus
	end
end

applicableTarget = function(parameters, mechanics, problem, target)
    return true
end

applicable = function(parameters, mechanics, problem)
	local creature = LIBRARY:creatures():getByName(parameters.id)
	if creature == "nil" then
		return mechanics:adaptGenericProblem(problem)
	end

	if summonedCreatureAmount(parameters, mechanics) == 0 then
		return mechanics:adaptGenericProblem(problem)
	end

	-- check if there are summoned creatures of other type
	if parameters.exclusive then
		local battle = mechanics:getBattle();
		local elemental = battle:getAnyUnitIf(function(unit)
			return (unit:getOwner() == mechanics:getCasterColor())
				and (unit:isSummoned())
				and (not unit:isClone())
				and (unit:getCreature() ~= creature)
		end)

		if elemental ~= nil then
			local hero = mechanics:getHeroCaster()
			local himHer = "core.genrltxt.539"
			if hero ~= nil and hero.isFemale() then
				himHer = "core.genrltxt.540"
			end

			if hero ~= nil then
				problem:add({
					append = "core.genrltxt.538",
					replace = {
						hero:getNameTextID(),
						elemental:getNamePluralTextID(),
						himHer
					}
				})
			else
				problem:add({
					append = "core.genrltxt.538"
				})
			end
			return false
		end
	end
	return true
end

apply = function(parameters, mechanics, server, target)
	local creature = LIBRARY:creatures():getByName(parameters.id)

	for _, dest in ipairs(target) do
		if dest.unitValue then
			local summoned = dest.unitValue
			local state = summoned:acquire()
			local healthValue = summonedCreatureHealth(parameters, mechanics)
			
			state:heal(
				healthValue, 
				EHealLevel.OVERHEAL, 
				(parameters.permanent and EHealPower.PERMANENT or EHealPower.ONE_BATTLE)
			)

			server:updateUnit(
				mechanics:getBattleID(),
				summoned:unitId(),
				state:save(),
				healthValue
			)
		else
			server:createUnit(
				mechanics:getBattleID(),
				{
					count = summonedCreatureAmount(parameters, mechanics),
					type = creature:getJsonKey(),
					side = mechanics:getCasterSide(),
					position = dest.hexValue,
					summoned = not parameters.permanent
				}
			)
		end
	end
end

transformTarget = function(parameters, mechanics, aimPoint, spellTarget)
	local creature = LIBRARY:creatures():getByName(parameters.id)
	local sameSummoned = mechanics:getBattle():getAnyUnitIf(function(unit)
		return (unit:getOwner() == mechanics:getCasterColor())
			and (unit:isSummoned())
			and (not unit:isClone())
			and (unit:getCreature() == creature)
			and (unit:isAlive())
	end)

	if sameSummoned == nil or not parameters.summonSameUnit then
		local hex = mechanics:getBattle():getAvailableHex(creature, mechanics:getCasterSide())
		if hex < 0 then
			return {} -- no free space. FIXME: should be in isApplicable
		else
			return {
				{ hex }
			}
		end
	else
		return {
			{ sameSummoned[1]:getPosition(), sameSummoned[1] }
		}
	end
end


