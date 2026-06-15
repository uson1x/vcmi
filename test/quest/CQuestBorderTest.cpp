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
