/*
 * AIFactory.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "AIFactory.h"

#include "CGlobalAI.h"

#ifdef ENABLE_NULLKILLER2_AI
#  include "../../AI/Nullkiller2/AIGateway.h"
#endif
#ifdef ENABLE_BATTLE_AI
#  include "../../AI/BattleAI/BattleAI.h"
#endif
#ifdef ENABLE_STUPID_AI
#  include "../../AI/StupidAI/StupidAI.h"
#endif
#ifdef ENABLE_MMAI
#  include "../../AI/MMAI/MMAI.h"
#endif
#include "../../AI/EmptyAI/CEmptyAI.h"

VCMI_LIB_NAMESPACE_BEGIN

std::shared_ptr<CGlobalAI> AIFactory::createAdventureAI(const std::string & name)
{
	logGlobal->info("Creating adventure AI %s", name);

#ifdef ENABLE_NULLKILLER2_AI
	if(name == "Nullkiller2")
	{
		auto ret = std::make_shared<NK2AI::AIGateway>();
		ret->dllName = name;
		return ret;
	}
#endif

	auto ret = std::make_shared<CEmptyAI>();
	ret->dllName = name;
	return ret;
}

std::shared_ptr<CBattleGameInterface> AIFactory::createBattleAI(const std::string & name)
{
	logGlobal->info("Creating battle AI %s", name);

#ifdef ENABLE_BATTLE_AI
	if(name == "BattleAI")
		return std::make_shared<CBattleAI>();
#endif

#ifdef ENABLE_STUPID_AI
	if(name == "StupidAI")
		return std::make_shared<CStupidAI>();
#endif

#ifdef ENABLE_MMAI
	if(name == "MMAI")
		return std::make_shared<MMAI::BAI::Router>();
#endif

	return std::make_shared<CEmptyAI>();
}

bool AIFactory::isAvailableAdventureAI(const std::string & name)
{
	if(name == "EmptyAI")
		return true;
#ifdef ENABLE_NULLKILLER2_AI
	if(name == "Nullkiller2")
		return true;
#endif
	return false;
}

VCMI_LIB_NAMESPACE_END
