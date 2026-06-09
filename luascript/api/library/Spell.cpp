/*
 * api/Spell.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"

#include "Spell.h"

#include "EntityBindings.h"
#include "../Registry.h"
#include "../../../lib/constants/EntityIdentifiers.h"
#include "../../../lib/bonuses/BonusCustomTypes.h"

VCMI_LIB_NAMESPACE_BEGIN

namespace scripting::api
{

using ::spells::Spell;

void SpellProxy::registerMethods(MethodRegistrar & R)
{
	EntityBindings<Spell>::registerMethods(R);
	R.method<&Entity::getNameTextID, Spell>("getNameTextID",
		"Returns the text ID of the spell name.");
	R.method<&Spell::isAdventure>("isAdventure",
		"True if the spell can only be cast on the adventure map.");
	R.method<&Spell::isCombat>("isCombat",
		"True if the spell can only be cast during combat.");
	R.method<&Spell::isCreatureAbility>("isCreatureAbility",
		"True if the spell is a creature's innate ability rather than a learnable spell.");
	R.method<&Spell::isPositive>("isPositive",
		"True if the spell is classified as beneficial to its target.");
	R.method<&Spell::isNegative>("isNegative",
		"True if the spell is classified as harmful to its target.");
	R.method<&Spell::isNeutral>("isNeutral",
		"True if the spell is classified as neutral (neither positive nor negative).");
	R.method<&Spell::isDamage>("isDamage",
		"True if the spell deals direct damage.");
	R.method<&Spell::isOffensive>("isOffensive",
		"True if the spell is offensive (damage / curse-type effects).");
	R.method<&Spell::isSpecial>("isSpecial",
		"True if the spell is marked as special (e.g. cannot be obtained from common sources).");
	R.method<&Spell::isPersistent>("isPersistent",
		"True if the spell's effect persists and can't be dispelled normally.");
	R.method<&Spell::isMagical>("isMagical",
		"True if the spell is magical in nature (as opposed to a non-magical ability).");

	R.method<&Spell::getCost>("getCost",
		"Returns the mana cost of the spell at the requested skill level.");
	R.method<&Spell::getBasePower>("getBasePower",
		"Returns the spell's base power before caster bonuses.");
	R.method<&Spell::getLevelPower>("getLevelPower",
		"Returns the spell's per-level power bonus.");
	R.method<&Spell::getDescriptionTranslated>("getLevelDescription",
		"DEPRECATED API Returns the translated description text for the spell at the requested skill level.");

	R.function<&SpellProxy::getSchools>("getSchools",
		"DEPRECATED API Returns the list of magic schools the spell belongs to as JSON keys.");
}

std::vector<std::string> SpellProxy::getSchools(const Spell & spell)
{
	std::vector<std::string> result;
	spell.forEachSchool([&result](const SpellSchool & school, bool & stop)
	{
		result.push_back(BonusSubtypeID(school).toString());
	});
	return result;
}

}

VCMI_LIB_NAMESPACE_END
