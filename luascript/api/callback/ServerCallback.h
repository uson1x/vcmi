/*
 * ServerCallback.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include <vcmi/ServerCallback.h>

#include "../../LuaWrapper.h"
#include "../../../lib/battle/BattleHex.h"
#include "../../../lib/json/JsonNode.h"
#include "../../../lib/constants/EntityIdentifiers.h"

VCMI_LIB_NAMESPACE_BEGIN

namespace battle { class Unit; }

namespace scripting
{
namespace api
{

class ServerCallbackProxy : public RawPointerWrapper<ServerCallback, ServerCallbackProxy>
{
public:
	using Wrapper = RawPointerWrapper<ServerCallback, ServerCallbackProxy>;

	static const std::vector<typename Wrapper::CustomRegType> REGISTER_CUSTOM;

	static void createUnit(ServerCallback * object, BattleID battleID, uint32_t id, JsonNode data);
	static void updateUnit(ServerCallback * object, BattleID battleID, uint32_t id, JsonNode data, int64_t healthDelta);
	static void injureUnit();
	static void removeUnit(ServerCallback * object, BattleID battleID, const battle::Unit * unit);
	static void moveUnit(ServerCallback * object, BattleID battleID, const battle::Unit * unit, BattleHex destination, bool isTeleport);
	static void appendLog(ServerCallback * object, BattleID battleID, JsonNode config);
	static int healUnit(lua_State * L);
};

}
}

VCMI_LIB_NAMESPACE_END
