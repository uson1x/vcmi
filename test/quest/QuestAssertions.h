/*
 * QuestAssertions.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "StdInc.h"

#include "../../lib/mapObjects/CQuest.h"
#include "../../lib/rewardable/Limiter.h"

/// What a test expects a loaded CQuest to look like. Populate only the
/// limiter fields the mission actually uses — the assertion helper ignores
/// fields not associated with `kind`.
struct ExpectedMission
{
	EQuestMission       kind     = EQuestMission::NONE;
	Rewardable::Limiter limiter;
	int                 lastDay  = -1;

	/// Per-visit text checks. Empty string means "don't check this slot".
	std::string firstVisitText;
	std::string nextVisitText;
	std::string completedText;
};

namespace quest_test
{

/// Assert a quest matches `expected`. On mismatch emits a non-fatal failure
/// pinned to `file:line` with a per-field diff naming the limiter slot that
/// disagreed.
void expectQuestMission(const CQuest & actual, const ExpectedMission & expected,
                        const char * file, int line);

} // namespace quest_test

/// Usage: EXPECT_QUEST_MISSION(seer->getQuest(), expected);
#define EXPECT_QUEST_MISSION(quest, expected) \
	::quest_test::expectQuestMission((quest), (expected), __FILE__, __LINE__)
