/*
 * Obstacle.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include <vcmi/scripting/Service.h>

#include "../../LuaWrapper.h"
#include "../../../lib/battle/CObstacleInstance.h"

VCMI_LIB_NAMESPACE_BEGIN

namespace scripting::api
{

class ObstacleProxy : public SharedPointerWrapper<const CObstacleInstance, ObstacleProxy>
{
public:
	using Wrapper = SharedPointerWrapper<const CObstacleInstance, ObstacleProxy>;
	static const std::vector<typename Wrapper::CustomRegType> REGISTER_CUSTOM;

	static CObstacleInstance::EObstacleType getObstacleType(std::shared_ptr<const CObstacleInstance> obstacle);
	static BattleHex getPosition(std::shared_ptr<const CObstacleInstance> obstacle);
	static std::string getSpellKey(std::shared_ptr<const CObstacleInstance> obstacle);
};

}

VCMI_LIB_NAMESPACE_END
