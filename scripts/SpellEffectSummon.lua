meta = {
	type = spellEffect,
	spellEffectSchema = {
		required = { "id" },
		properties = {
			type = {},
			indirect = {},
			optional = {},
			id = { type = "string" }, -- type of creature that can be summoned by the spell
			permanent = { type = "boolean" }, -- if true, summoned units persists after combat
			exclusive =  { type = "boolean" }, -- if true, spell can only be used if there are no summoned units of another typee
			summonByHealth = { type = "boolean" }, -- if true, spell effect power represents summoned health, if not - summoned amount
			summonSameUnit = { type = "boolean" } -- if true, spell will increase stack size of existing summon, if available, instead of summoning a separate stack 
		},
		additionalProperties = false
	}
}

local function summonedEffectValue(parameters, mechanics)
	local effectPower = mechanics:getEffectPower()
	local rawEffectPower = mechanics:calculateRawEffectValue(0, effectPower)
	local finalEffectPower = mechanics:applySpecificSpellBonus(rawEffectPower)
	
	return finalEffectPower
end

local function summonedCreatureHealth(parameters, mechanics)
	local valueWithBonus = summonedEffectValue(parameters, mechanics)
	local creature = LIBRARY:getCreatureByName(parameters.id)
	if parameters.summonByHealth then
		return valueWithBonus
	else
		return valueWithBonus * creature:getMaxHealth()
	end
end

local function summonedCreatureAmount(parameters, mechanics)
	local valueWithBonus = summonedEffectValue(parameters, mechanics)
	local creature = LIBRARY:getCreatureByName(parameters.id)
	if parameters.summonByHealth then
		return math.floor(valueWithBonus / creature:getMaxHealth())
	else
		return valueWithBonus
	end
end

-- TODO
-- initializes parameters of the script using spell effect json
-- returns converted parameters that contain resolved identifiers
initialize = function(parameters)
	parameters.creature = LIBRARY:getCreatureByName(parameters.id)
	return parameters
end

--- Returns true if specified target can be affected by the spell
--- if target can not be affected, script needs to call `problem:add`
--- to explain the reason to the player
applicableTarget = function(parameters, mechanics, problem, target)
    return true
end

--- Returns true if spell can be casted in general
--- if no valid targets exist, script needs to call `problem:add`
--- to explain the reason to the player
applicable = function(parameters, mechanics, problem)
	local creature = LIBRARY:getCreatureByName(parameters.id)

	if summonedCreatureAmount(parameters, mechanics) == 0 then
		return problem:addGeneric(mechanics:getCasterNameTextID())
	end

	-- check if there are summoned creatures of other type
	if parameters.exclusive then
		local elemental = mechanics:getBattle():getAnyUnitIf(function(unit)
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

--- Actually casts the spells and applies all changes caused by spell
--- use `server` parameter to apply changes on specified target(s)
apply = function(parameters, mechanics, server, target)
	local creature = LIBRARY:getCreatureByName(parameters.id)

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

--- converts specified range into actual list of affected targets
--- for example, area damage spells should locate all units on affected hexes
--- and return list of affected units
transformTarget = function(parameters, mechanics, aimPoint, spellTarget)
	local creature = LIBRARY:getCreatureByName(parameters.id)
	local sameSummoned = mechanics:getBattle():getAnyUnitIf(function(unit)
		return (unit:getOwner() == mechanics:getCasterColor())
			and (unit:isSummoned())
			and (not unit:isClone())
			and (unit:getCreature() == creature)
			and (unit:isAlive())
	end)

	if sameSummoned == nil or not parameters.summonSameUnit then
		local hex = mechanics:getBattle():getAvailableHex(creature, mechanics:getCasterSide())
		if not hex.isValid() then
			return {} -- no free space. FIXME: should be in isApplicable
		else
			return {
				{
					hex = hex, 
					unit = nil
				}
			}
		end
	else
		return {
			{ 
				hex = sameSummoned[1]:getPosition(), 
				unit = sameSummoned[1]
			}
		}
	end
end


