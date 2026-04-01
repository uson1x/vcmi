local BattleUnitsChanged = require("netpacks.BattleUnitsChanged")
local MetaString = require("texts.MetaString")

-- expected globals:
-- parameters:
-- - creature
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

	local creature = SERVICES:creatures():getByName(parameters.creature)
	if parameters.summonByHealth then
		return valueWithBonus
	else
		return valueWithBonus * creature:getMaxHealth()
	end
end

local function summonedCreatureAmount(parameters, mechanics)
	local valueWithBonus = summonedEffectValue(parameters, mechanics)

	local creature = SERVICES:creatures():getByName(parameters.creature)
	if parameters.summonByHealth then
		return math.floor(valueWithBonus / creature:getMaxHealth())
	else
		return valueWithBonus
	end
end

applicable = function(parameters, mechanics, problem)
	local creature = SERVICES:creatures():getByName(parameters.creature)
	if creature == "nil" then
		return false -- mechanics:adaptGenericProblem(problem)
	end

	if summonedCreatureAmount(parameters, mechanics) == 0 then
		return false -- mechanics:adaptGenericProblem(problem)
	end

	-- check if there are summoned creatures of other type
	if parameters.exclusive then
		local battle = mechanics:getBattle();
		local otherSummoned = battle:getAnyUnitIf(function(unit)
			return (unit:getOwner() == mechanics:getCasterColor())
				and (unit:isSummoned())
				and (not unit:isClone())
				and (unit:getCreature() ~= creature)
		end)

		if otherSummoned ~= nil then
			local elemental = otherSummoned[1]

			local text = MetaString.new()
			text:appendTextID("core.genrltxt.538")

--			local hero = mechanics:caster():getHeroCaster()
--			if caster then
--				text:replaceRawString(caster:getNameTranslated())
--				text:replaceNamePlural(elemental:creatureId())
--
--				if caster.gender == EHeroGender.FEMALE then
--					text:replaceTextID("core.genrltxt.540")
--				else
--					text:replaceTextID("core.genrltxt.539")
--				end
--			end

--			problem:add(text, Problem.NORMAL)
			return false
		end
	end

	return true
end

apply = function(parameters, mechanics, server, target)
	local creature = SERVICES:creatures():getByName(parameters.creature)

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
			
			server:battleUnitChanged(
				mechanics:getBattleID(),
				summoned:unitId(),
				UnitChanges.EOperation.UPDATE,
				state:save()
			)
			
		else
			server:battleUnitChanged(
				mechanics:getBattleID(),
				mechanics:getBattle():getNextUnitId(),
				UnitChanges.EOperation.ADD,
				{
					count = summonedCreatureAmount(parameters, mechanics),
					type = creature,
					side = mechanics:casterSide(),
					position = dest.hexValue,
					summoned = not parameters.permanent
				}
			)
		end
	end

	if #pack.changedStacks > 0 then
		server:apply(pack)
	end
end

transformTarget = function(parameters, mechanics, aimPoint, spellTarget)
	local creature = SERVICES:creatures():getByName(parameters.creature)
	local sameSummoned = mechanics:getBattle():getAnyUnitIf(function(unit)
		return (unit:getOwner() == mechanics:getCasterColor())
			and (unit:isSummoned())
			and (not unit:isClone())
			and (unit:getCreature() == creature)
			and (unit:isAlive())
	end)

	local effectTarget = {}

	if sameSummoned == nil or not parameters.summonSameUnit then
		local hex = mechanics:getBattle():getAvailableHex(creature, mechanics:casterSide())
		if not hex:isValid() then
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


