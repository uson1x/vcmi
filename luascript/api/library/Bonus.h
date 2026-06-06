/*
 * Bonus.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include "../../LuaWrapper.h"
#include "../MethodRegistrar.h"
#include "../../../lib/bonuses/Bonus.h"
#include "../../../lib/bonuses/BonusList.h"
#include "../../../lib/bonuses/BonusEnum.h"

VCMI_LIB_NAMESPACE_BEGIN

namespace scripting::api
{

class BonusProxy : public CopyableWrapper<Bonus, BonusProxy>
{
public:
	static constexpr std::string_view luaName = "Bonus";

	using Wrapper = CopyableWrapper<Bonus, BonusProxy>;
	static void registerMethods(MethodRegistrar & R);

	static std::string getType(const Bonus & b);
	static si32 getVal(const Bonus & b);
	static std::string getSubtype(const Bonus & b);
	static std::string getSourceID(const Bonus & b);
	static BonusSource getSource(const Bonus & b);
	static std::vector<BonusDuration::BonusDuration> getDuration(const Bonus & b);
	static BonusValueType getValType(const Bonus & b);
	static std::string getStacking(const Bonus & b);
	static si16 getTurnsRemain(const Bonus & b);
	static bool isHidden(const Bonus & b);
	static si32 getParametersAsNumber(const Bonus & b);
};

class BonusListProxy : public CopyableWrapper<BonusList, BonusListProxy>
{
public:
	static constexpr std::string_view luaName = "BonusList";

	using Wrapper = CopyableWrapper<BonusList, BonusListProxy>;
	static void registerMethods(MethodRegistrar & R);

	static int32_t size(const BonusList & list);
	static Bonus getBonus(const BonusList & list, int32_t index);
};

inline std::string luaTypeNameOf(LuaTypeNameTag<Bonus>)
{
	return std::string(BonusProxy::luaName);
}

inline std::string luaTypeNameOf(LuaTypeNameTag<BonusList>)
{
	return std::string(BonusListProxy::luaName);
}

}

VCMI_LIB_NAMESPACE_END
