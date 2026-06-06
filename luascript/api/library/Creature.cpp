/*
 * api/Creature.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"

#include "Creature.h"

#include "BonusProviderBindings.h"
#include "EntityBindings.h"
#include "../Registry.h"
#include "../../../lib/bonuses/IBonusBearer.h"
#include "../../../lib/constants/EntityIdentifiers.h"

VCMI_LIB_NAMESPACE_BEGIN

namespace scripting::api
{

void CreatureProxy::registerMethods(MethodRegistrar & R)
{
	EntityBindings<Creature>::registerMethods(R);
	BonusProviderBindings<Creature>::registerMethods(R);

	R.method<&Creature::getMaxHealth, Creature>("getMaxHealth",
		"Returns the base maximum hit points of a single creature of this type.");
	R.method<&Creature::getNamePluralTranslated>("getPluralName",
		"Returns the translated plural form of the creature's name.");
	R.method<&Creature::getNameSingularTranslated>("getSingularName",
		"Returns the translated singular form of the creature's name.");
	R.method<&Creature::getNamePluralTextID>("getNamePluralTextID",
		"Returns the JSON text ID of the plural name.");
	R.method<&Creature::getNameSingularTextID>("getNameSingularTextID",
		"Returns the JSON text ID of the singular name.");
	R.function<&CreatureProxy::getNameTextID>("getNameTextID",
		"Returns either the singular (amount == 1) or plural text ID for the requested amount.");

	R.method<&Creature::getAdvMapAmountMin>("getMapAmountMin",
		"Returns the minimum stack size for this creature when generated on the adventure map.");
	R.method<&Creature::getAdvMapAmountMax>("getMapAmountMax",
		"Returns the maximum stack size for this creature when generated on the adventure map.");
	R.method<&Creature::getAIValue>("getAIValue",
		"Returns the AI value used for army strength calculations.");
	R.method<&Creature::getFightValue>("getFightValue",
		"Returns the combat power value of one creature of this type.");
	R.method<&Creature::getLevel>("getLevel",
		"Returns the creature level (1..7).");
	R.method<&Creature::getGrowth>("getGrowth",
		"Returns the weekly growth rate for this creature in town dwellings.");
	R.method<&Creature::getHorde>("getHorde",
		"Returns the extra growth granted by the horde building (0 if none).");

	R.method<&Creature::getBaseAttack>("getBaseAttack",
		"Returns the base attack value before bonuses.");
	R.method<&Creature::getBaseDefense>("getBaseDefense",
		"Returns the base defense value before bonuses.");
	R.method<&Creature::getBaseDamageMin>("getBaseDamageMin",
		"Returns the minimum melee damage of one creature, before bonuses.");
	R.method<&Creature::getBaseDamageMax>("getBaseDamageMax",
		"Returns the maximum melee damage of one creature, before bonuses.");
	R.method<&Creature::getBaseHitPoints>("getBaseHitPoints",
		"Returns the base hit points of one creature.");
	R.method<&Creature::getBaseSpellPoints>("getBaseSpellPoints",
		"Returns the base spell points / mana of one creature (for spell-casting creatures).");
	R.method<&Creature::getBaseSpeed>("getBaseSpeed",
		"Returns the base battlefield movement speed in hexes per turn.");
	R.method<&Creature::getBaseShots>("getBaseShots",
		"Returns the base ammunition count for ranged creatures (0 if melee-only).");

	R.method<&Creature::getRecruitCost>("getRecruitCost",
		"Returns the recruitment cost as a resource bundle.");
	R.method<&Creature::isDoubleWide>("isDoubleWide",
		"True if the creature occupies two hexes on the battlefield.");
}

std::string CreatureProxy::getNameTextID(const Creature & creature, int amount)
{
	return amount == 1 ? creature.getNameSingularTextID() : creature.getNamePluralTextID();
}

}

VCMI_LIB_NAMESPACE_END
