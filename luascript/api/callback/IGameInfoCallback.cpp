/*
 * IGameInfoCallback.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"

#include "IGameInfoCallback.h"

#include <vcmi/Player.h>

#include "../../LuaCallWrapper.h"

// Proxy header brought in for its luaTypeNameOf ADL overload (used by getHero's return type).
#include "../adventure/HeroInstance.h"

#include "../../../lib/callback/IGameInfoCallback.h"
#include "../../../lib/mapObjects/CGHeroInstance.h"

VCMI_LIB_NAMESPACE_BEGIN

namespace scripting::api
{

void IGameInfoCallbackProxy::registerMethods(MethodRegistrar & R)
{
	R.method<&GameCb::getHero>("getHero",
		"Returns the hero by its object identifier, or nil if not found.");
	R.method<&GameCb::getObj>("getObj",
		"Returns the map object by its identifier, or nil if not found.");
}

}

VCMI_LIB_NAMESPACE_END
