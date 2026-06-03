/*
 * Registry.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"

#include "Registry.h"

#include "battle/Unit.h"
#include "battle/UnitState.h"
#include "battle/BattleHex.h"
#include "battle/BattleHexArray.h"
#include "battle/Obstacle.h"
#include "events/BattleEvents.h"
#include "events/EventBus.h"
#include "events/GenericEvents.h"
#include "events/SubscriptionRegistry.h"
#include "spells/Mechanics.h"
#include "spells/Problem.h"
#include "library/Artifact.h"
#include "library/Bonus.h"
#include "callback/IBattleInfoCallback.h"
#include "library/Creature.h"
#include "library/Faction.h"
#include "callback/IGameInfoCallback.h"
#include "library/HeroClass.h"
#include "adventure/HeroInstance.h"
#include "library/HeroType.h"
#include "Registry.h"
#include "callback/ServerCallback.h"
#include "library/Services.h"
#include "library/Skill.h"
#include "library/Spell.h"
#include "adventure/StackInstance.h"

VCMI_LIB_NAMESPACE_BEGIN

namespace scripting::api
{

Registry::Registry()
{
	registerPrivate<ServicesProxy>("library.Services");
	registerPrivate<ArtifactProxy>("library.Artifact");
	registerPrivate<BonusProxy>("library.Bonus");
	registerPrivate<BonusListProxy>("library.BonusList");
	registerPrivate<CreatureProxy>("library.Creature");
	registerPrivate<FactionProxy>("library.Faction");
	registerPrivate<HeroClassProxy>("library.HeroClass");
	registerPrivate<HeroTypeProxy>("library.HeroType");
	registerPrivate<SkillProxy>("library.Skill");
	registerPrivate<SpellProxy>("library.Spell");

	registerPrivate<HeroInstanceProxy>("adventure.HeroInstance");
	registerPrivate<StackInstanceProxy>("adventure.StackInstance");

	registerPrivate<BattleHexProxy>("battle.BattleHex");
	registerPrivate<BattleHexArrayProxy>("battle.BattleHexArray");
	registerPrivate<UnitProxy>("battle.Unit");
	registerPrivate<LuaUnitStateProxy>("battle.UnitState");
	registerPrivate<ObstacleProxy>("battle.Obstacle");
	registerPrivate<ProblemProxy>("battle.SpellProblem");
	registerPrivate<MechanicsProxy>("battle.SpellMechanics");

	registerPrivate<ApplyDamageProxy>("events.ApplyDamage");
	registerPrivate<GameResumedProxy>("events.GameResumed");
	registerPrivate<PlayerGotTurnProxy>("events.PlayerGotTurn");
	registerPrivate<TurnStartedProxy>("events.TurnStarted");

	registerPrivate<IBattleInfoCallbackProxy>("game.Battle");
	registerPrivate<IGameInfoCallbackProxy>("game.Game");
	registerPrivate<ServerCallbackProxy>("game.Server");
	registerPrivate<EventBusProxy>("game.EventBus");
	registerPrivate<EventSubscriptionProxy>("game.EventSubscription");
}

const Registry * Registry::get()
{
	static const Registry instance;
	return &instance;
}

void Registry::addPrivate(const std::string & name, const std::shared_ptr<Registar> & item)
{
	privateData[name] = item;
}

void Registry::addPublic(const std::string & name, const std::shared_ptr<Registar> & item)
{
	privateData[name] = item;
	publicData[name] = item;
}

const Registar * Registry::find(const std::string & name) const
{
	auto iter = publicData.find(name);
	if(iter == publicData.end())
		return nullptr;
	else
		return iter->second.get();
}

}

VCMI_LIB_NAMESPACE_END
