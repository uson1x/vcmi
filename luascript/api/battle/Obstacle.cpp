/*
 * Obstacle.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "StdInc.h"

#include "Obstacle.h"

#include "../../LuaStack.h"
#include "../../LuaCallWrapper.h"
#include "../Registry.h"

#include "../../../lib/constants/EntityIdentifiers.h"

VCMI_LIB_NAMESPACE_BEGIN

namespace scripting::api
{

void ObstacleProxy::registerMethods(MethodRegistrar & R)
{
	R.function<&ObstacleProxy::getObstacleType>("getObstacleType",
		"Returns the obstacle category (usual, absolute, moat, spell-created, ...).");
	R.function<&ObstacleProxy::getPosition>("getPosition",
		"Returns the primary battle hex occupied by the obstacle.");
	R.function<&ObstacleProxy::getSpellKey>("getSpellKey",
		"For spell-created obstacles, returns the JSON key of the originating spell. Throws otherwise.");
}

CObstacleInstance::EObstacleType ObstacleProxy::getObstacleType(std::shared_ptr<const CObstacleInstance> obstacle)
{
	return obstacle->obstacleType;
}

BattleHex ObstacleProxy::getPosition(std::shared_ptr<const CObstacleInstance> obstacle)
{
	return obstacle->pos;
}

std::string ObstacleProxy::getSpellKey(std::shared_ptr<const CObstacleInstance> obstacle)
{
	if(obstacle->obstacleType != CObstacleInstance::SPELL_CREATED)
		throw LuaApiException("Not a spell created obstacle!");
	return SpellID::encode(obstacle->ID);
}

}

VCMI_LIB_NAMESPACE_END
