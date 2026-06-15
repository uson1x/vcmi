/*
 * CQuestBorderTest.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"

#include "QuestScenarios.h"
#include "QuestTest.h"

#include "../../lib/CPlayerState.h"
#include "../../lib/gameState/CGameState.h"
#include "../../lib/mapObjects/CGHeroInstance.h"
#include "../../lib/mapObjects/CQuest.h"

// Phase 2.4 — border-guard / border-gate / keymaster runtime tests.
//
// CGKeymasterTent::onHeroVisit sends a ChangeObjectVisitors pack via
// sendAndApply, which (a) flows through GameEventCallbackMock unmodified and
// (b) is handled by GameStatePackVisitor::visitChangeObjectVisitors against
// PlayerState::visitedObjectsGlobal. That means the visit() helper alone is
// enough to drive the "after keymaster" half of these tests — no extra
// packet-forwarder wiring needed beyond what Phase 0 already shipped.
//
// AddQuest-emit / dialog-answer / removeObject tests
// (Keymaster_FirstVisit_doesNotEmitAddQuest, BorderGuard_AfterKeymaster_*)
// land alongside the 0.4 pass that captures AddQuest packs and routes
// removeObject through the gameState apply pipeline.

using namespace QuestScenarios;

class QuestBorderTest : public QuestTest {};

TEST_F(QuestBorderTest, BorderGate_BeforeKeymaster_passableForFalse)
{
	// CGBorderGate::passableFor(color) returns wasMyColorVisited(color),
	// which reads PlayerState::visitedObjectsGlobal. Without a keymaster
	// visit that set is empty, so the gate should refuse every colour.
	auto s = questBorderGate();
	ASSERT_NO_FATAL_FAILURE(startWithMap(std::move(s.builder)));

	const auto * gate = findObjectAt(s.questPos2);
	ASSERT_NE(gate, nullptr);
	EXPECT_FALSE(gate->passableFor(PlayerColor(0)));
}

TEST_F(QuestBorderTest, BorderGate_AfterKeymaster_passableForTrue)
{
	// Drive the keymaster visit, then re-check the gate. CGKeymasterTent
	// sends VISITOR_ADD_PLAYER for its own (id, subID) tuple, which is
	// exactly the slot CGKeys::wasMyColorVisited consults.
	auto s = questBorderGate();
	ASSERT_NO_FATAL_FAILURE(startWithMap(std::move(s.builder)));

	auto * hero       = findHeroAt(s.heroPos);
	auto * keymaster  = findObjectAt(s.questPos);
	auto * gate       = findObjectAt(s.questPos2);
	ASSERT_NE(hero,      nullptr);
	ASSERT_NE(keymaster, nullptr);
	ASSERT_NE(gate,      nullptr);

	ASSERT_FALSE(gate->passableFor(PlayerColor(0)));

	visit(hero, keymaster);

	EXPECT_TRUE(gate->passableFor(PlayerColor(0)))
		<< "after visiting the matching keymaster, the gate should be passable for red";
}

TEST_F(QuestBorderTest, BorderGate_NeverRemoved)
{
	// Border gate is the "pathfinder-passable variant" — visiting the
	// matching keymaster should make it passable, but the gate object itself
	// stays on the map indefinitely (in contrast to border guard, which the
	// hero can choose to demolish on visit).
	auto s = questBorderGate();
	ASSERT_NO_FATAL_FAILURE(startWithMap(std::move(s.builder)));

	auto * hero      = findHeroAt(s.heroPos);
	auto * keymaster = findObjectAt(s.questPos);
	ASSERT_NE(hero,      nullptr);
	ASSERT_NE(keymaster, nullptr);

	visit(hero, keymaster);

	// Gate still exists at its anchor position after the keymaster visit.
	EXPECT_NE(findObjectAt(s.questPos2), nullptr);
}

TEST_F(QuestBorderTest, Keymaster_FirstVisit_marksColorVisited)
{
	// Direct verification of the visitedObjectsGlobal mutation that drives
	// border-gate/border-guard passability. Pre-visit the set lacks the
	// keymaster entry; post-visit it contains it.
	auto s = questBorderGate();
	ASSERT_NO_FATAL_FAILURE(startWithMap(std::move(s.builder)));

	auto * hero      = findHeroAt(s.heroPos);
	auto * keymaster = findObjectAt(s.questPos);
	ASSERT_NE(hero,      nullptr);
	ASSERT_NE(keymaster, nullptr);

	const auto * tent = dynamic_cast<const CGKeymasterTent *>(keymaster);
	ASSERT_NE(tent, nullptr);

	EXPECT_FALSE(tent->wasMyColorVisited(PlayerColor(0)));

	visit(hero, keymaster);

	EXPECT_TRUE(tent->wasMyColorVisited(PlayerColor(0)));
}

TEST_F(QuestBorderTest, Keymaster_SecondVisit_doesNotErrorOut)
{
	// Re-visiting an already-visited keymaster should be a no-op on
	// visitedObjectsGlobal (set semantics) and not crash. Captures the
	// regression "second visit attempts to re-insert and explodes on a
	// state-machine assertion".
	auto s = questBorderGate();
	ASSERT_NO_FATAL_FAILURE(startWithMap(std::move(s.builder)));

	auto * hero      = findHeroAt(s.heroPos);
	auto * keymaster = findObjectAt(s.questPos);
	ASSERT_NE(hero,      nullptr);
	ASSERT_NE(keymaster, nullptr);

	visit(hero, keymaster);
	ASSERT_NO_FATAL_FAILURE(visit(hero, keymaster));

	const auto * tent = dynamic_cast<const CGKeymasterTent *>(keymaster);
	ASSERT_NE(tent, nullptr);
	EXPECT_TRUE(tent->wasMyColorVisited(PlayerColor(0)));
}

TEST_F(QuestBorderTest, Keymaster_FirstVisit_doesNotEmitAddQuest)
{
	// Keymaster tents are not quest objects (no CQuest, no quest-log entry).
	// First visit must emit zero AddQuest packets; only InfoWindow(19) plus
	// the ChangeObjectVisitors mutation handled in earlier tests.
	auto s = questKeymasterTent();
	ASSERT_NO_FATAL_FAILURE(startWithMap(std::move(s.builder)));

	auto * hero      = findHeroAt(s.heroPos);
	auto * keymaster = findObjectAt(s.questPos);
	ASSERT_NE(hero, nullptr);
	ASSERT_NE(keymaster, nullptr);

	ASSERT_TRUE(gameEventCallback->addedQuests.empty()) << "preconditions";

	visit(hero, keymaster);

	EXPECT_TRUE(gameEventCallback->addedQuests.empty())
		<< "keymaster visit unexpectedly emitted " << gameEventCallback->addedQuests.size()
		<< " AddQuest packet(s)";
	// And the visit still rendered the standard first-visit message:
	EXPECT_FALSE(gameEventCallback->infoWindows.empty());
}

TEST_F(QuestBorderTest, Keymaster_SecondVisit_showsAlreadyVisitedText)
{
	// First visit emits the "you found the tent" infoWindow (txt_id=19),
	// second visit emits the "you've already been here" infoWindow (txt_id=20).
	// We assert two InfoWindow captures after two visits — the actual text
	// IDs flow through MetaString resolution which is too brittle for a
	// runtime assertion at this layer.
	auto s = questKeymasterTent();
	ASSERT_NO_FATAL_FAILURE(startWithMap(std::move(s.builder)));

	auto * hero      = findHeroAt(s.heroPos);
	auto * keymaster = findObjectAt(s.questPos);
	ASSERT_NE(hero, nullptr);
	ASSERT_NE(keymaster, nullptr);

	visit(hero, keymaster);
	const size_t windowsAfterFirst = gameEventCallback->infoWindows.size();
	ASSERT_GE(windowsAfterFirst, 1u);

	visit(hero, keymaster);
	EXPECT_GT(gameEventCallback->infoWindows.size(), windowsAfterFirst)
		<< "second visit should produce its own already-visited dialog";
}

TEST_F(QuestBorderTest, BorderGuard_BeforeKeymaster_blocksAndEmitsAddQuest)
{
	// Pre-keymaster border-guard visit emits one InfoWindow (txt 18) plus one
	// AddQuest. No BlockingDialog yet — the prompt only appears once the
	// player has the matching key.
	auto s = questBorderGuard();
	ASSERT_NO_FATAL_FAILURE(startWithMap(std::move(s.builder)));

	auto * hero        = findHeroAt(s.heroPos);
	auto * borderGuard = findObjectAt(s.questPos2);
	ASSERT_NE(hero,        nullptr);
	ASSERT_NE(borderGuard, nullptr);

	visit(hero, borderGuard);

	EXPECT_EQ(gameEventCallback->addedQuests.size(), 1u);
	EXPECT_FALSE(gameEventCallback->infoWindows.empty());
	EXPECT_TRUE(gameEventCallback->blockingDialogs.empty())
		<< "border guard should not prompt for removal before the keymaster has been visited";
}

TEST_F(QuestBorderTest, BorderGuard_AfterKeymaster_promptsRemovalDialog)
{
	// Visit keymaster first, then border guard: the guard sees
	// wasMyColorVisited==true and switches to the BlockingDialog path.
	auto s = questBorderGuard();
	ASSERT_NO_FATAL_FAILURE(startWithMap(std::move(s.builder)));

	auto * hero        = findHeroAt(s.heroPos);
	auto * keymaster   = findObjectAt(s.questPos);
	auto * borderGuard = findObjectAt(s.questPos2);
	ASSERT_NE(hero,        nullptr);
	ASSERT_NE(keymaster,   nullptr);
	ASSERT_NE(borderGuard, nullptr);

	visit(hero, keymaster);
	// Drop everything queued by the keymaster visit so the assertion is
	// scoped to the border-guard interaction.
	gameEventCallback->addedQuests.clear();
	gameEventCallback->infoWindows.clear();

	visit(hero, borderGuard);

	EXPECT_TRUE(gameEventCallback->addedQuests.empty())
		<< "AddQuest must be emitted on the *first* border-guard visit only";
	EXPECT_EQ(gameEventCallback->blockingDialogs.size(), 1u)
		<< "border guard should prompt the player whether to demolish";
}

TEST_F(QuestBorderTest, BorderGuard_AnsweredYes_removesObject)
{
	// Visit keymaster, then visit border guard, then answer "yes" to the
	// removal prompt. CGBorderGuard::blockingDialogAnswered sends RemoveObject
	// — which the mock now routes through gameState->apply. After the apply
	// the slot at the border-guard position is empty.
	auto s = questBorderGuard();
	ASSERT_NO_FATAL_FAILURE(startWithMap(std::move(s.builder)));

	auto * hero        = findHeroAt(s.heroPos);
	auto * keymaster   = findObjectAt(s.questPos);
	auto * borderGuard = findObjectAt(s.questPos2);
	ASSERT_NE(hero,        nullptr);
	ASSERT_NE(keymaster,   nullptr);
	ASSERT_NE(borderGuard, nullptr);

	visit(hero, keymaster);
	visit(hero, borderGuard);

	ASSERT_EQ(gameEventCallback->blockingDialogs.size(), 1u);
	answerDialog(hero, /*yes*/ 1);

	EXPECT_EQ(findObjectAt(s.questPos2), nullptr)
		<< "border guard should be removed from the map after a positive answer";
}
