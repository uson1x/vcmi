local Base = require("spellEffect")
local Script = setmetatable({}, {__index = Base})
Script.__index = Script

function Script.validationSchema(parameters)
	local schema = Base.validationSchema(parameters)
	return schema
end

-- TODO
-- initializes parameters of the script using spell effect json
-- returns converted parameters that contain resolved identifiers
function Script.initialize(parameters)
	return parameters
end

--- Returns true if specified target can be affected by the spell
--- if target can not be affected, script needs to call `problem:add`
--- to explain the reason to the player
function Script.applicableTarget(parameters, mechanics, problem, target)
    return true
end

--- Returns true if spell can be casted in general
--- if no valid targets exist, script needs to call `problem:add`
--- to explain the reason to the player
function Script.applicable(parameters, mechanics, problem)
	return true
end

--- Actually casts the spells and applies all changes caused by spell
--- use `server` parameter to apply changes on specified target(s)
function Script.apply(parameters, mechanics, server, target)
	return
end

--- converts specified range into actual list of affected targets
--- for example, area damage spells should locate all units on affected hexes
--- and return list of affected units
function Script.transformTarget(parameters, mechanics, aimPoint, spellTarget)
	return spellTarget
end

return Script
