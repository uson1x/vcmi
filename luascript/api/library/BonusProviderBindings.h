/*
 * BonusProviderBindings.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include <vcmi/Entity.h>

#include "../MethodRegistrar.h"
#include "../SignatureOf.h"

VCMI_LIB_NAMESPACE_BEGIN

namespace scripting::api
{

/// Shared bindings for proxies whose underlying C++ type derives from IConstBonusProvider.
/// Exposes the bonus-bearer accessor so scripts can introspect bonuses on the entity.
template<class Leaf>
class BonusProviderBindings
{
public:
	static void registerMethods(MethodRegistrar & R)
	{
		R.template method<&IConstBonusProvider::getBonusBearer, Leaf>("getBonusBearer",
			"Returns the IBonusBearer view that lists every bonus active on this entity.");
	}
};

/// IBonusBearer has no dedicated proxy class — it is the abstract view that
/// `IConstBonusProvider::getBonusBearer()` returns. Expose a stable Lua name so docs
/// don't surface it as `userdata`. Scripts use it via `getBonuses(...)` on the leaf type.
inline std::string luaTypeNameOf(LuaTypeNameTag<IBonusBearer>)
{
	return "BonusBearer";
}

}

VCMI_LIB_NAMESPACE_END
