/*
 * Mechanics.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"

#include "Mechanics.h"

#include "../Registry.h"
#include "../../LuaStack.h"
#include "../../LuaCallWrapper.h"

#include "../../../lib/battle/CBattleInfoCallback.h"
#include "../../../lib/spells/CSpell.h"
#include "../../../lib/battle/Unit.h"
#include "../../../lib/spells/Problem.h"
#include "../../../lib/mapObjects/CGHeroInstance.h"
#include "../../../lib/GameLibrary.h"
#include "../../../lib/texts/CGeneralTextHandler.h"
#include "../../../lib/texts/Languages.h"

#include <vcmi/spells/Service.h>

VCMI_LIB_NAMESPACE_BEGIN

namespace scripting::api
{
		using ::spells::Mechanics;

		bool MechanicsProxy::ownerMatchesUnit(const Mechanics & m, const battle::Unit & unit)
		{
			return m.ownerMatches(&unit);
		}

		std::string MechanicsProxy::getPluralFormTextID(const spells::Mechanics & m, const std::string & baseTextID, int32_t count)
		{
			std::string lang = LIBRARY->generaltexth->getPreferredLanguage();
			return Languages::getPluralFormTextID(lang, count, baseTextID);
		}

		void MechanicsProxy::registerMethods(MethodRegistrar & R)
		{
			R.method<&Mechanics::isPositiveSpell>("isPositive",
				"True if the spell mechanics classify this cast as a positive effect.");
			R.method<&Mechanics::isNegativeSpell>("isNegative",
				"True if the spell mechanics classify this cast as a negative effect.");
			R.method<&Mechanics::isSmart>("isSmart",
				"True if the spell only affects friendly or enemy targets (vs. anyone in range).");
			R.method<&Mechanics::isMassive>("isMassive",
				"True if the spell targets all valid targets simultaneously.");
			R.method<&Mechanics::alwaysHitFirstTarget>("alwaysHitFirstTarget",
				"True if the first selected target is always considered hit, ignoring magic resistance.");
			R.method<&Mechanics::wouldResist>("wouldResist",
				"Returns true if the given target would resist this cast.");

			R.method<&Mechanics::getEffectLevel>("getEffectLevel",
				"Returns the effective mastery level used for the spell's magnitude.");
			R.method<&Mechanics::getRangeLevel>("getRangeLevel",
				"Returns the effective mastery level used for the spell's range.");
			R.method<&Mechanics::getEffectPower>("getEffectPower",
				"Returns the effective spell power applied to the magnitude calculation.");
			R.method<&Mechanics::getEffectDuration>("getEffectDuration",
				"Returns the effect duration in turns.");
			R.method<&Mechanics::getEffectValue>("getEffectValue",
				"Returns the computed effect value (e.g. damage / health amount).");
			R.method<&Mechanics::getCasterColor>("getCasterColor",
				"Returns the player color of the caster.");
			R.method<&Mechanics::getCasterSide>("getCasterSide",
				"Returns the battle side of the caster.");
			R.method<&Mechanics::getHeroCaster>("getHeroCaster",
				"Returns the hero performing the cast, or nil if cast by a unit.");
			R.method<&Mechanics::getUnitCaster>("getUnitCaster",
				"Returns the unit performing the cast, or nil if cast by a hero.");
			R.method<&Mechanics::getCasterNameTextID>("getCasterNameTextID",
				"Returns the JSON text ID of the caster's name.");
			R.method<&Mechanics::battle>("getBattle",
				"Returns the battle callback associated with this cast.");
			R.method<&Mechanics::calculateRawEffectValue>("calculateRawEffectValue",
				"Returns the raw effect value before unit-specific adjustments.");
			R.method<&Mechanics::applySpecificSpellBonus>("applySpecificSpellBonus",
				"Applies any spell-specific bonus modifier and returns the resulting value.");
			R.method<&Mechanics::applySpellBonus>("applySpellBonus",
				"Applies the generic spell-damage bonus and returns the resulting value.");
			R.method<&Mechanics::isReceptive>("isReceptive",
				"True if the target is receptive to the spell (not immune, not absorbed).");
			R.function<&ownerMatchesUnit>("ownerMatches",
				"True if the given unit is owned by the same player as the caster.");
			R.method<&Mechanics::getSpell>("getSpell",
				"Returns the Spell being cast.");
			R.method<&Mechanics::adjustEffectValue>("adjustEffectValue",
				"Applies all per-target adjustments to the raw effect value.");
			R.function<&MechanicsProxy::getPluralFormTextID>("getPluralFormTextID",
				"Picks the appropriate plural-form variant of a text ID for the given count and language.");
		}
}

VCMI_LIB_NAMESPACE_END
