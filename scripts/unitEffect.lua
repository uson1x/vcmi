local Script = {}
Script.__index = Script
Script.type = "unitEffect"

--- Actually casts the spell and applies all changes caused by it.
--- Use `server` parameter to apply changes on specified target(s).
--- target contains units pre-filtered by C++ (applicableUnit / filterTarget),
--- so every dest.unit is guaranteed alive and receptive.
function Script:apply(mechanics, server, target)
	return
end

--- Returns true if `unit` can be affected by this spell effect.
--- Called for every candidate unit during target selection.
--- If not defined, C++ default applies: unit must be a valid, non-dead target.
--- function Script:isValidTarget(mechanics, unit)
---     return true
--- end

--- Returns true if `unit` is receptive to this spell effect (not immune).
--- Called for every candidate unit during target selection and filtering.
--- If not defined, C++ default applies: checks spell immunity bonuses on the unit.
--- function Script:isReceptive(mechanics, unit)
---     return true
--- end

return Script
