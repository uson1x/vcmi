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

/// Pre-canned SOD scenarios for quest tests. Each factory returns a builder
/// that's one .build() away from a playable map plus the positions of the
/// objects it placed, so tests can locate what they want without scanning.
namespace QuestScenarios
{

// Identifiers used by scenario factories and loader tests. Hero/creature ids
// deliberately avoid 0/1 — zero-initialised state has been misread as "Orrin"
// or "pikeman" in the past, masking field-initialisation bugs.
constexpr int kHeroChristian        = 6;   // knight
constexpr int kHeroTyris            = 7;   // second knight for two-hero scenarios
constexpr int kCreatureGriffin      = 4;
constexpr int kCreatureRoyalGriffin = 5;
constexpr int kCreatureImp          = 42;

constexpr int kArtifactCentaurAxe   = 7;
constexpr int kArtifactHelm         = 36;  // helmOfHeavenlyEnlightenment (component of angelic alliance)
constexpr int kArtifactSash         = 68;  // ambassadorsSash
constexpr int kArtifactAngelicAlly  = 129; // angelicAlliance (combination artifact)

/// A scenario builder plus the named positions it placed. Each factory
/// populates only the fields its scenario actually uses — see the per-factory
/// declaration for which.
struct Scenario
{
	TinyH3M::TinyH3MBuilder builder;

	int3 heroPos {};       ///< Visiting hero (red).
	int3 secondHeroPos {}; ///< Optional second hero (target / second visitor).
	int3 questPos {};      ///< Primary quest object (seer hut, quest guard, keymaster, ...).
	int3 questPos2 {};     ///< Optional second quest object (paired keymaster/border-guard, easy/hard seers, ...).
	int3 questPos3 {};     ///< Optional third quest object.

	/// Cross-object reference for kill-quest scenarios. Zero = unused.
	TinyH3M::ObjectHandle target {};
};

// ---- Seer-hut bring-X scenarios -------------------------------------------

Scenario seerArtifact();                          ///< heroPos: visitor with sash in backpack. questPos: seer hut.
Scenario seerArtifactAssembledInBackpack();       ///< heroPos: visitor with Angelic Alliance in backpack. questPos: seer hut asking for the Helm component.
Scenario seerArtifactAssembledEquipped();         ///< heroPos: visitor with Angelic Alliance equipped (right hand). questPos: seer hut asking for the Helm component.
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
// no runtime effect.

Scenario disabledQuestBorderMultiSibling(); ///< heroPos: visitor. questPos: keymaster. questPos2/3: two border guards same colour.

} // namespace QuestScenarios
