/*
 * QuestScenarios.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "StdInc.h"

#include "../mock/TinyH3MBuilder.h"
#include "../../lib/int3.h"

/// Phase 1 scenario factories. Each function returns a ready-to-build
/// TinyH3MBuilder plus the positions of the objects the scenario placed so
/// tests can locate them without scanning the whole map.
///
/// Scope: SOD-only. HOTA-gated scenarios (multi-quest seer, reach-date,
/// difficulty, repeatable) and refactor-only JSON fixtures (Quest Gate,
/// required-keys on Quest Guard, fog visibility, destroyedObjects limiter)
/// land in a later pass.
namespace QuestScenarios
{

/// Generic carrier for "the builder plus a handful of named positions". Not
/// every scenario populates every position field — see per-factory docs for
/// which are meaningful.
struct Scenario
{
	TinyH3M::TinyH3MBuilder builder;

	// Visiting hero (always present, owned by red).
	int3 heroPos {};

	// Secondary hero, if the scenario places one (target / second visitor).
	int3 secondHeroPos {};

	// Primary quest-bearing object (seer hut, quest guard, border guard,
	// keymaster — depending on scenario).
	int3 questPos {};

	// Secondary quest object (for scenarios with two: e.g. easy/hard seer
	// huts, or keymaster + border-guard pair).
	int3 questPos2 {};

	// Third quest object (border-multi-sibling, etc.).
	int3 questPos3 {};

	// Auxiliary object handle when scenarios reference one object from
	// another (e.g. kill-quest targets). 0 means "no handle relevant".
	TinyH3M::ObjectHandle target {};
};

// ---- Seer-hut bring-X scenarios -------------------------------------------

Scenario seerArtifact();                ///< heroPos: visitor with sash in backpack. questPos: seer hut.
Scenario seerArtifactAssembled();       ///< heroPos: visitor with Angelic Alliance equipped. questPos: seer hut.
Scenario seerArmy();                    ///< heroPos: visitor with garrison (10 pikemen + 5 archers). questPos: seer hut.
Scenario seerResources();               ///< heroPos: visitor. questPos: seer hut. Test grants resources post-load.
Scenario seerHero();                    ///< heroPos: visitor. secondHeroPos: target. questPos: seer hut.
Scenario seerPlayer();                  ///< heroPos: red visitor. secondHeroPos: blue hero. questPos: seer hut.
Scenario seerLevel();                   ///< heroPos: visitor (XP for L5). questPos: easy seer (>=3). questPos2: hard seer (>=10).
Scenario seerPrimarySkill();            ///< heroPos: visitor (attack=5). questPos: easy. questPos2: hard.

// ---- Kill-quest scenarios (cross-object handles) --------------------------

Scenario seerKillCreature();            ///< heroPos: visitor. secondHeroPos unused. questPos: seer hut. target: monster handle.
Scenario seerKillHero();                ///< heroPos: red visitor. secondHeroPos: blue target hero. questPos: seer hut. target: target hero handle.

// ---- Timer / payload-shape scenarios --------------------------------------

Scenario seerTimeout();                 ///< heroPos: visitor. questPos: seer hut with lastDay=7, trivial resources limiter.
Scenario seerEmptyArmyToggle();         ///< heroPos: visitor with single 1-pikeman stack. questPos: seer hut (bring 1 pikeman, no reward creatures).

// ---- Quest Guard scenarios ------------------------------------------------

Scenario questGuard();                  ///< heroPos: visitor. questPos: quest guard with bring-1000-wood (resources granted post-load).
Scenario questGuardBlockVisit();        ///< heroPos: visitor placed adjacent to the quest-guard tile.

// ---- Border / Keymaster scenarios -----------------------------------------

Scenario questKeymasterTent();          ///< heroPos: visitor. questPos: keymaster tent of colour 0.
Scenario questBorderGuard();            ///< heroPos: visitor. questPos: keymaster. questPos2: border guard (matching colour).
Scenario questBorderGate();             ///< heroPos: visitor. questPos: keymaster. questPos2: border gate (matching colour).

// ---- Disabled-prefix SOD scenarios (refactor-only behaviour) --------------
// "Disabled" in the name marks them as backing for currently-failing tests;
// no runtime effect. HOTA-prefixed disabled-scenarios live in a later pass.

Scenario disabledQuestBorderMultiSibling(); ///< heroPos: visitor. questPos: keymaster. questPos2/3: two border guards same colour.

} // namespace QuestScenarios
