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

#include "battle/UnitProxy.h"
#include "battle/BattleHexProxy.h"
#include "events/BattleEvents.h"
#include "events/EventBusProxy.h"
#include "events/GenericEvents.h"
#include "events/SubscriptionRegistryProxy.h"
#include "netpacks/BattleLogMessage.h"
#include "netpacks/BattleStackMoved.h"
#include "netpacks/BattleUnitsChanged.h"
#include "netpacks/EntitiesChanged.h"
#include "netpacks/InfoWindow.h"
#include "netpacks/SetResources.h"
#include "spells/Mechanics.h"
#include "spells/Problem.h"
#include "texts/MetaString.h"
#include "Artifact.h"
#include "BattleCb.h"
#include "Creature.h"
#include "Faction.h"
#include "GameCb.h"
#include "HeroClass.h"
#include "HeroInstance.h"
#include "HeroType.h"
#include "Registry.h"
#include "ServerCb.h"
#include "Services.h"
#include "Skill.h"
#include "Spell.h"
#include "StackInstance.h"

VCMI_LIB_NAMESPACE_BEGIN

namespace scripting::api
{

Registry::Registry()
{
	registerPrivate<ServicesProxy>("library.Services");

	registerPrivate<ArtifactProxy>("entity.Artifact");
	registerPrivate<CreatureProxy>("entity.Creature");
	registerPrivate<FactionProxy>("entity.Faction");
	registerPrivate<HeroClassProxy>("entity.HeroClass");
	registerPrivate<HeroTypeProxy>("entity.HeroType");
	registerPrivate<SkillProxy>("entity.Skill");
	registerPrivate<SpellProxy>("entity.Spell");

//	registerPrivate<BonusProxy>("Bonus");
//	registerPrivate<BonusListProxy>("BonusList");
//	registerPrivate<BonusBearerProxy>("BonusBearer");

	registerPrivate<HeroInstanceProxy>("adventure.HeroInstance");
	registerPrivate<StackInstanceProxy>("adventure.StackInstance");

	registerPrivate<battle::BattleHexProxy>("battle.BattleHex");
	registerPrivate<battle::UnitProxy>("battle.Unit");
	registerPrivate<SpellProblemProxy>("battle.SpellProblem");
	registerPrivate<SpellsMechanicsProxy>("battle.SpellMechanics");

	registerPrivate<events::ApplyDamageProxy>("events.ApplyDamage");
	registerPrivate<events::GameResumedProxy>("events.GameResumed");
	registerPrivate<events::PlayerGotTurnProxy>("events.PlayerGotTurn");
	registerPrivate<events::TurnStartedProxy>("events.TurnStarted");

	registerPrivate<netpacks::BattleLogMessageProxy>("netpacks.BattleLogMessage");
	registerPrivate<netpacks::BattleStackMovedProxy>("netpacks.BattleStackMoved");
	registerPrivate<netpacks::SetResourcesProxy>("netpacks.SetResources");
	registerPrivate<netpacks::InfoWindowProxy>("netpacks.InfoWindow");
	registerPrivate<netpacks::EntitiesChangedProxy>("netpacks.EntitiesChanged");
	registerPrivate<netpacks::BattleUnitsChangedProxy>("netpacks.BattleUnitsChanged");

	registerPrivate<BattleCbProxy>("game.Battle");
	registerPrivate<GameCbProxy>("game.Game");
	registerPrivate<ServerCbProxy>("game.Server");
	registerPrivate<events::EventBusProxy>("game.EventBus");
	registerPrivate<events::EventSubscriptionProxy>("game.EventSubscription");

	registerPrivate<MetaStringProxy>("texts.MetaString");
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
