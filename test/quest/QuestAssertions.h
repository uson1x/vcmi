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

/// Human-readable description of what each loader/runtime test expects from a
/// CQuest. Set only the limiter fields relevant to the mission; the assertion
/// helper does not check default-valued limiter fields, so an artifact-mission
/// expectation does not need to zero out creatures/resources/etc.
struct ExpectedMission
{
	EQuestMission       kind     = EQuestMission::NONE;
	Rewardable::Limiter limiter;
	int                 lastDay  = -1;

	// Optional textual expectations. Empty string means "any value accepted".
	std::string firstVisitText;
	std::string nextVisitText;
	std::string completedText;
};

namespace quest_test
{

/// Compares `actual` against the relevant fields of `expected`. On mismatch
/// emits a GoogleTest non-fatal failure with `file`:`line` source pin and a
/// field-by-field diff so it is obvious which limiter field disagreed.
void expectQuestMission(const CQuest & actual, const ExpectedMission & expected,
                        const char * file, int line);

} // namespace quest_test

/// Drop-in for typical use:  EXPECT_QUEST_MISSION(seer->getQuest(), expected);
#define EXPECT_QUEST_MISSION(quest, expected) \
	::quest_test::expectQuestMission((quest), (expected), __FILE__, __LINE__)
