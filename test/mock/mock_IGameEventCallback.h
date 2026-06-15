/*
 * mock_IGameEventCallback.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include <vcmi/ServerCallback.h>

#include "../../lib/callback/IGameEventCallback.h"
#include "../../lib/int3.h"
#include "../../lib/ResourceSet.h"
#include "../../lib/networkPacks/PacksForClient.h"

class GameEventCallbackMock : public IGameEventCallback
{
public:
	using UpperCallback = ::ServerCallback;

	GameEventCallbackMock(UpperCallback * upperCallback_);
	virtual ~GameEventCallbackMock();

	// Tests set this once gameState->init has populated CMap so the mock can
	// resolve object identifiers without owning the gameState itself.
	void setGameInfoCallback(IGameInfoCallback * cb) { infoCallback = cb; }

	// ---- captured dialog queue (consumed by QuestTest::answerDialog etc.) --
	// showBlockingDialog / showInfoDialog get pushed here instead of being
	// dispatched to a UI. Tests inspect text / components and drive the answer
	// via the returned struct's `answer` field.
	struct CapturedBlockingDialog
	{
		BlockingDialog dialog;
		const IObjectInterface * caller = nullptr;
	};
	std::vector<CapturedBlockingDialog> blockingDialogs;
	std::vector<InfoWindow>             infoWindows;
	std::vector<AddQuest>               addedQuests;

	void setObjPropertyValue(ObjectInstanceID objid, ObjProperty prop, int32_t value) override;
	void setObjPropertyID(ObjectInstanceID objid, ObjProperty prop, ObjPropertyID identifier) override;
	void setRewardableObjectConfiguration(ObjectInstanceID mapObjectID, const Rewardable::Configuration & configuration) override;
	void setRewardableObjectConfiguration(ObjectInstanceID townInstanceID, BuildingID buildingID, const Rewardable::Configuration & configuration) override;

	void showInfoDialog(InfoWindow * iw) override;

	void changeSpells(const CGHeroInstance * hero, bool give, const std::set<SpellID> & spells) override;
	void setResearchedSpells(const CGTownInstance * town, int level, const std::vector<SpellID> & spells, bool accepted) override;
	bool removeObject(const CGObjectInstance * obj, const PlayerColor & initiator) override;
	void createBoat(const int3 & visitablePosition, BoatId type, PlayerColor initiator) override;
	void setOwner(const CGObjectInstance * objid, PlayerColor owner) override;
	void giveExperience(const CGHeroInstance * hero, TExpType val) override;
	void changePrimSkill(const CGHeroInstance * hero, PrimarySkill which, si64 val, ChangeValueMode mode) override;
	void changeSecSkill(const CGHeroInstance * hero, SecondarySkill which, int val, ChangeValueMode mode) override;
	void showBlockingDialog(const IObjectInterface * caller, BlockingDialog * iw) override;
	void showGarrisonDialog(ObjectInstanceID upobj, ObjectInstanceID hid, bool removableUnits, const MetaString & customTitle) override;
	void showTeleportDialog(TeleportDialog * iw) override;
	void showObjectWindow(const CGObjectInstance * object, EOpenWindowMode window, const CGHeroInstance * visitor, bool addQuery) override;
	void giveResource(PlayerColor player, GameResID which, int val) override;
	void giveResources(PlayerColor player, const ResourceSet & resources) override;

	void giveCreatures(const CGHeroInstance * h, const CCreatureSet & creatures) override;
	void giveCreatures(const CArmedInstance * objid, const CGHeroInstance * h, const CCreatureSet & creatures, bool remove) override;
	void takeCreatures(ObjectInstanceID objid, const std::vector<CStackBasicDescriptor> & creatures, bool forceRemoval) override;
	bool changeStackCount(const StackLocation & sl, TQuantity count, ChangeValueMode mode) override;
	bool changeStackType(const StackLocation & sl, const CCreature * c) override;
	bool insertNewStack(const StackLocation & sl, const CCreature * c, TQuantity count) override;
	bool eraseStack(const StackLocation & sl, bool forceRemoval) override;
	bool swapStacks(const StackLocation & sl1, const StackLocation & sl2) override;
	bool addToSlot(const StackLocation & sl, const CCreature * c, TQuantity count) override;
	void tryJoiningArmy(const CArmedInstance * src, const CArmedInstance * dst, bool removeObjWhenFinished, bool allowMerging) override;
	bool moveStack(const StackLocation & src, const StackLocation & dst, TQuantity count) override;

	void removeAfterVisit(const ObjectInstanceID & id) override;

	bool giveHeroNewArtifact(const CGHeroInstance * h, const ArtifactID & artId, const ArtifactPosition & pos) override;
	bool giveHeroNewScroll(const CGHeroInstance * h, const SpellID & spellId, const ArtifactPosition & pos) override;
	bool putArtifact(const ArtifactLocation & al, const ArtifactInstanceID & id, std::optional<bool> askAssemble) override;
	void removeArtifact(const ArtifactLocation & al) override;
	bool moveArtifact(const PlayerColor & player, const ArtifactLocation & al1, const ArtifactLocation & al2) override;

	void heroVisitCastle(const CGTownInstance * obj, const CGHeroInstance * hero) override;
	void stopHeroVisitCastle(const CGTownInstance * obj, const CGHeroInstance * hero) override;
	void visitCastleObjects(const CGTownInstance * obj, const CGHeroInstance * hero) override;
	void startBattle(const CArmedInstance * army1, const CArmedInstance * army2, int3 tile, const CGHeroInstance * hero1, const CGHeroInstance * hero2, const BattleLayout & layout, const CGTownInstance * town) override;
	void startBattle(const CArmedInstance * army1, const CArmedInstance * army2) override;
	bool moveHero(ObjectInstanceID hid, int3 dst, EMovementMode movementMode, bool transit, PlayerColor asker, const EPathfindingLayer & layer) override;
	void giveHeroBonus(GiveBonus * bonus) override;
	void setMovePoints(SetMovePoints * smp) override;
	void setMovePoints(ObjectInstanceID hid, int val) override;
	void setManaPoints(ObjectInstanceID hid, int val) override;
	void giveHero(ObjectInstanceID id, PlayerColor player, ObjectInstanceID boatId) override;
	void changeObjPos(ObjectInstanceID objid, int3 newPos, const PlayerColor & initiator) override;
	void heroExchange(ObjectInstanceID hero1, ObjectInstanceID hero2) override;
	void changeFogOfWar(int3 center, ui32 radius, PlayerColor player, ETileVisibility mode) override;
	void changeFogOfWar(const FowTilesType & tiles, PlayerColor player, ETileVisibility mode) override;
	void castSpell(const spells::Caster * caster, SpellID spellID, const int3 & pos) override;

	void sendAndApply(CPackForClient & pack) override;

	bool isVisitCoveredByAnotherQuery(const CGObjectInstance * obj, const CGHeroInstance * hero) override;

	vstd::RNG & getRandomGenerator() override;

private:
	UpperCallback *      upperCallback = nullptr;
	IGameInfoCallback *  infoCallback  = nullptr;
};
