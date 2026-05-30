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
#include "../../../lib/bonuses/Bonus.h"
#include "../../../lib/bonuses/BonusList.h"
#include "../../../lib/bonuses/BonusEnum.h"

VCMI_LIB_NAMESPACE_BEGIN

namespace scripting::api::library
{

class BonusProxy : public CopyableWrapper<Bonus, BonusProxy>
{
public:
	using Wrapper = CopyableWrapper<Bonus, BonusProxy>;
	static const std::vector<typename Wrapper::CustomRegType> REGISTER_CUSTOM;

	static std::string getType(Bonus b);
	static si32 getVal(Bonus b);
	static std::string getSubtype(Bonus b);
	static std::string getSourceID(Bonus b);
	static si32 getSource(Bonus b);
	static std::vector<BonusDuration::BonusDuration> getDuration(Bonus b);
	static si32 getValType(Bonus b);
	static std::string getStacking(Bonus b);
	static si16 getTurnsRemain(Bonus b);
	static bool isHidden(Bonus b);
	static si32 getParametersAsNumber(Bonus b);
};

class BonusListProxy : public CopyableWrapper<BonusList, BonusListProxy>
{
public:
	using Wrapper = CopyableWrapper<BonusList, BonusListProxy>;
	static const std::vector<typename Wrapper::CustomRegType> REGISTER_CUSTOM;

	static int32_t size(BonusList list);
	static Bonus getBonus(BonusList list, int32_t index);
};

}

VCMI_LIB_NAMESPACE_END
