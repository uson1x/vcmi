/*
 * mock_IGameEventCallback.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"

#include "mock_IGameEventCallback.h"

#include "../../lib/callback/IGameInfoCallback.h"
#include "../../lib/mapObjects/CGHeroInstance.h"
#include "../../lib/mapObjects/CGObjectInstance.h"
#include "../../lib/mapObjects/army/CArmedInstance.h"
#include "../../lib/mapObjects/army/CStackInstance.h"
#include "../../lib/networkPacks/StackLocation.h"

GameEventCallbackMock::GameEventCallbackMock(UpperCallback * upperCallback_)
	: upperCallback(upperCallback_)
{

}

GameEventCallbackMock::~GameEventCallbackMock() = default;

void GameEventCallbackMock::setObjPropertyValue(ObjectInstanceID objid, ObjProperty prop, int32_t value)
{
	SetObjectProperty sob;
	sob.id         = objid;
	sob.what       = prop;
	sob.identifier = NumericID(value);
	sendAndApply(sob);
}

void GameEventCallbackMock::setObjPropertyID(ObjectInstanceID objid, ObjProperty prop, ObjPropertyID identifier)
{
	SetObjectProperty sob;
	sob.id         = objid;
	sob.what       = prop;
	sob.identifier = identifier;
	sendAndApply(sob);
}

void GameEventCallbackMock::setRewardableObjectConfiguration(ObjectInstanceID, const Rewardable::Configuration &) {}
void GameEventCallbackMock::setRewardableObjectConfiguration(ObjectInstanceID, BuildingID, const Rewardable::Configuration &) {}

void GameEventCallbackMock::showInfoDialog(InfoWindow * iw)
{
	assert(iw);
	infoWindows.push_back(*iw);
}

void GameEventCallbackMock::changeSpells(const CGHeroInstance *, bool, const std::set<SpellID> &) {}
void GameEventCallbackMock::setResearchedSpells(const CGTownInstance *, int, const std::vector<SpellID> &, bool) {}

bool GameEventCallbackMock::removeObject(const CGObjectInstance * obj, const PlayerColor & initiator)
{
	if(!obj)
		return false;
	RemoveObject ro(obj->id, initiator);
	sendAndApply(ro);
	return true;
}

void GameEventCallbackMock::createBoat(const int3 &, BoatId, PlayerColor) {}
void GameEventCallbackMock::setOwner(const CGObjectInstance *, PlayerColor) {}

void GameEventCallbackMock::giveExperience(const CGHeroInstance * hero, TExpType val)
{
	// Simplified version of CGameHandler::giveExperience: tests don't care
	// about the level-up dialog flow or the cap-warning info window.
	SetHeroExperience she;
	she.id   = hero->id;
	she.mode = ChangeValueMode::RELATIVE;
	she.val  = val;
	sendAndApply(she);
}

void GameEventCallbackMock::changePrimSkill(const CGHeroInstance *, PrimarySkill, si64, ChangeValueMode) {}
void GameEventCallbackMock::changeSecSkill(const CGHeroInstance *, SecondarySkill, int, ChangeValueMode) {}

void GameEventCallbackMock::showBlockingDialog(const IObjectInterface * caller, BlockingDialog * iw)
{
	assert(iw);
	blockingDialogs.push_back({*iw, caller});
}

void GameEventCallbackMock::showGarrisonDialog(ObjectInstanceID, ObjectInstanceID, bool, const MetaString &) {}
void GameEventCallbackMock::showTeleportDialog(TeleportDialog *) {}
void GameEventCallbackMock::showObjectWindow(const CGObjectInstance *, EOpenWindowMode, const CGHeroInstance *, bool) {}

void GameEventCallbackMock::giveResource(PlayerColor player, GameResID which, int val)
{
	if(!val)
		return;
	TResources resources;
	resources[which] = val;
	giveResources(player, resources);
}

void GameEventCallbackMock::giveResources(PlayerColor player, const ResourceSet & resources)
{
	if(resources.empty())
		return;
	SetResources sr;
	sr.mode   = ChangeValueMode::RELATIVE;
	sr.player = player;
	sr.res    = resources;
	sendAndApply(sr);
}

void GameEventCallbackMock::giveCreatures(const CGHeroInstance *, const CCreatureSet &) {}
void GameEventCallbackMock::giveCreatures(const CArmedInstance *, const CGHeroInstance *, const CCreatureSet &, bool) {}

void GameEventCallbackMock::takeCreatures(ObjectInstanceID objid, const std::vector<CStackBasicDescriptor> & creatures, bool forceRemoval)
{
	// Port of CGameHandler::takeCreatures. Walks every slot in the target
	// army for each (creature, count) demand, calling eraseStack for whole
	// stacks and changeStackCount for partial removals. Requires the test's
	// IGameInfoCallback (EditorCallback for QuestTest) so getObj resolves.
	if(creatures.empty())
		return;

	if(!infoCallback)
		throw std::runtime_error("GameEventCallbackMock::takeCreatures: setGameInfoCallback was not called");

	const auto * armyObj = infoCallback->getObj(objid);
	const auto * army    = dynamic_cast<const CArmedInstance *>(armyObj);
	if(!army)
	{
		// In a well-formed test this should not happen — the scenario placed
		// the army and the test consumed its id. Failing loudly is better than
		// silently dropping the take.
		throw std::runtime_error("GameEventCallbackMock::takeCreatures: object is not a CArmedInstance");
	}

	for(const CStackBasicDescriptor & wanted : creatures)
	{
		TQuantity remaining = wanted.getCount();
		while(remaining > 0)
		{
			bool consumedSlot = false;
			for(const auto & slot : army->Slots())
			{
				if(slot.second->getType() != wanted.getType())
					continue;

				const TQuantity available = slot.second->getCount();
				if(remaining >= available)
				{
					eraseStack(StackLocation(army->id, slot.first), forceRemoval);
					remaining -= available;
				}
				else
				{
					changeStackCount(StackLocation(army->id, slot.first), -remaining, ChangeValueMode::RELATIVE);
					remaining = 0;
				}
				consumedSlot = true;
				break;
			}
			if(!consumedSlot)
				throw std::runtime_error("GameEventCallbackMock::takeCreatures: army did not contain enough of the requested creature");
		}
	}
}

bool GameEventCallbackMock::changeStackCount(const StackLocation & sl, TQuantity count, ChangeValueMode mode)
{
	ChangeStackCount csc;
	csc.army  = sl.army;
	csc.slot  = sl.slot;
	csc.count = count;
	csc.mode  = mode;
	sendAndApply(csc);
	return true;
}

bool GameEventCallbackMock::changeStackType(const StackLocation &, const CCreature *) { return false; }
bool GameEventCallbackMock::insertNewStack(const StackLocation &, const CCreature *, TQuantity) { return false; }

bool GameEventCallbackMock::eraseStack(const StackLocation & sl, bool forceRemoval)
{
	// Port of CGameHandler::eraseStack minus the COMPLAIN_RET guards — tests
	// own the scenario, malformed StackLocations would be a bug to fix at the
	// call site, not a runtime condition to recover from.
	EraseStack es;
	es.army = sl.army;
	es.slot = sl.slot;
	sendAndApply(es);
	return true;
}

bool GameEventCallbackMock::swapStacks(const StackLocation &, const StackLocation &) { return false; }
bool GameEventCallbackMock::addToSlot(const StackLocation &, const CCreature *, TQuantity) { return false; }
void GameEventCallbackMock::tryJoiningArmy(const CArmedInstance *, const CArmedInstance *, bool, bool) {}
bool GameEventCallbackMock::moveStack(const StackLocation &, const StackLocation &, TQuantity) { return false; }

void GameEventCallbackMock::removeAfterVisit(const ObjectInstanceID & id)
{
	// CGameHandler defers the removal until the matching MapObjectVisitQuery
	// ends. The test infrastructure has no query system — visits are
	// synchronous, the interaction is already "over" by the time this
	// override runs — so route straight to RemoveObject.
	RemoveObject ro(id, PlayerColor::UNFLAGGABLE);
	sendAndApply(ro);
}

bool GameEventCallbackMock::giveHeroNewArtifact(const CGHeroInstance *, const ArtifactID &, const ArtifactPosition &) { return false; }
bool GameEventCallbackMock::giveHeroNewScroll(const CGHeroInstance *, const SpellID &, const ArtifactPosition &) { return false; }
bool GameEventCallbackMock::putArtifact(const ArtifactLocation &, const ArtifactInstanceID &, std::optional<bool>) { return false; }

void GameEventCallbackMock::removeArtifact(const ArtifactLocation & al)
{
	BulkEraseArtifacts ea;
	ea.artHolder = al.artHolder;
	ea.posPack.push_back(al.slot);
	sendAndApply(ea);
}

bool GameEventCallbackMock::moveArtifact(const PlayerColor &, const ArtifactLocation &, const ArtifactLocation &) { return false; }

void GameEventCallbackMock::heroVisitCastle(const CGTownInstance *, const CGHeroInstance *) {}
void GameEventCallbackMock::stopHeroVisitCastle(const CGTownInstance *, const CGHeroInstance *) {}
void GameEventCallbackMock::visitCastleObjects(const CGTownInstance *, const CGHeroInstance *) {}
void GameEventCallbackMock::startBattle(const CArmedInstance *, const CArmedInstance *, int3, const CGHeroInstance *, const CGHeroInstance *, const BattleLayout &, const CGTownInstance *) {}
void GameEventCallbackMock::startBattle(const CArmedInstance *, const CArmedInstance *) {}
bool GameEventCallbackMock::moveHero(ObjectInstanceID, int3, EMovementMode, bool, PlayerColor, const EPathfindingLayer &) { return false; }
void GameEventCallbackMock::giveHeroBonus(GiveBonus *) {}
void GameEventCallbackMock::setMovePoints(SetMovePoints *) {}
void GameEventCallbackMock::setMovePoints(ObjectInstanceID, int) {}
void GameEventCallbackMock::setManaPoints(ObjectInstanceID, int) {}
void GameEventCallbackMock::giveHero(ObjectInstanceID, PlayerColor, ObjectInstanceID) {}
void GameEventCallbackMock::changeObjPos(ObjectInstanceID, int3, const PlayerColor &) {}
void GameEventCallbackMock::heroExchange(ObjectInstanceID, ObjectInstanceID) {}
void GameEventCallbackMock::changeFogOfWar(int3, ui32, PlayerColor, ETileVisibility) {}
void GameEventCallbackMock::changeFogOfWar(const FowTilesType &, PlayerColor, ETileVisibility) {}
void GameEventCallbackMock::castSpell(const spells::Caster *, SpellID, const int3 &) {}

void GameEventCallbackMock::sendAndApply(CPackForClient & pack)
{
	// Capture quest-relevant packs into per-type queues so tests can inspect
	// what the engine emitted. AddQuest and InfoWindow flow through here
	// (InfoWindow because IObjectInterface::showInfoDialog constructs one
	// inline and sends it via sendAndApply rather than going through the
	// showInfoDialog(InfoWindow*) override). The pack still proceeds to
	// gameState->apply so the actual state mutation happens.
	if(auto * aq = dynamic_cast<AddQuest *>(&pack))
		addedQuests.push_back(*aq);
	if(auto * iw = dynamic_cast<InfoWindow *>(&pack))
		infoWindows.push_back(*iw);

	upperCallback->apply(pack);
}

bool GameEventCallbackMock::isVisitCoveredByAnotherQuery(const CGObjectInstance *, const CGHeroInstance *) { return false; }

vstd::RNG & GameEventCallbackMock::getRandomGenerator()
{
	throw std::runtime_error("Not implemented!");
}
