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

const std::vector<ObstacleProxy::CustomRegType> ObstacleProxy::REGISTER_CUSTOM =
{
	{"getObstacleType", LuaFunctionWrapper<&ObstacleProxy::getObstacleType>::invoke, false},
	{"getPosition",     LuaFunctionWrapper<&ObstacleProxy::getPosition>::invoke,     false},
	{"getSpellKey",     LuaFunctionWrapper<&ObstacleProxy::getSpellKey>::invoke,     false},
};

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
