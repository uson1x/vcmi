/*
 * WikiTownContent.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "WikiTownContent.h"

#include "../../widgets/CViewport.h"
#include "../../widgets/Images.h"
#include "../../widgets/TextControls.h"
#include "../../widgets/GraphicalPrimitiveCanvas.h"
#include "../../render/Colors.h"
#include "../../render/IImage.h"

#include "../../../lib/GameLibrary.h"
#include "../../../lib/entities/faction/CFaction.h"
#include "../../../lib/entities/faction/CTown.h"
#include "../../../lib/entities/building/CBuilding.h"
#include "../../../lib/CCreatureHandler.h"
#include "../../../lib/ResourceSet.h"
#include "../../../lib/texts/CGeneralTextHandler.h"

// ============================================================================
// Layout constants
// ============================================================================

static constexpr int SECTION_GAP   = 14;  ///< vertical gap between major sections
static constexpr int ROW_H         = 20;  ///< row height for table entries
static constexpr int ICON_SIZE     = 16;  ///< building / creature icon square
static constexpr int RES_ICON_W    = 14;  ///< small resource icon width
static constexpr int RES_GAP       = 4;   ///< gap between resource icons
static constexpr int TEXT_LEFT     = 6;   ///< left margin for text
static constexpr int COST_RIGHT_M  = 6;   ///< right margin before cost column

// ============================================================================
// Helper: add resource cost icons + labels for a TResources
// ============================================================================

/// Lays out resource icons + amounts horizontally from `startX` rightwards at `y`.
/// Returns widgets created (appended to `out`).
static void addResourceCost(
	std::vector<std::shared_ptr<CIntObject>> & out,
	const TResources & cost,
	int startX,
	int y,
	int maxWidth)
{
	// Collect non-zero resources
	struct ResInfo { GameResID id; int amount; };
	std::vector<ResInfo> entries;

	TResources::nziterator it(cost);
	while(it.valid())
	{
		entries.push_back({ it->resType, static_cast<int>(it->resVal) });
		++it;
	}

	if(entries.empty())
		return;

	// Layout: [icon 14px][amount text ~30px][gap 4px] per resource
	const int entryW = RES_ICON_W + 30 + RES_GAP;
	int x = startX;

	for(const auto & e : entries)
	{
		if(x + RES_ICON_W + 20 > startX + maxWidth)
			break; // don't overflow

		out.push_back(std::make_shared<CAnimImage>(
			AnimationPath::builtin("SMALRES"),
			e.id.getNum(),
			0, x, y));

		out.push_back(std::make_shared<CLabel>(
			x + RES_ICON_W + 1, y + 1,
			FONT_TINY, ETextAlignment::TOPLEFT, Colors::WHITE,
			std::to_string(e.amount)));

		x += entryW;
	}
}

// ============================================================================
// buildTownContent
// ============================================================================

std::vector<std::shared_ptr<CIntObject>> buildTownContent(
	CViewport & viewport,
	const CFaction * faction,
	int viewportWidth)
{
	std::vector<std::shared_ptr<CIntObject>> widgets;

	if(!faction || !faction->hasTown())
		return widgets;

	const CTown * town = faction->town.get();
	if(!town)
		return widgets;

	OBJECT_CONSTRUCTION_TARGETED(viewport.content());

	int curY = 4;
	const int W = viewportWidth;

	// ------------------------------------------------------------------
	// 1. Town title
	// ------------------------------------------------------------------
	widgets.push_back(std::make_shared<CLabel>(
		W / 2, curY,
		FONT_BIG, ETextAlignment::CENTER, Colors::YELLOW,
		faction->getNameTranslated()));
	curY += 22;

	// ------------------------------------------------------------------
	// 2. Town background (scaled inside viewport width)
	// ------------------------------------------------------------------
	{
		auto bg = std::make_shared<CPicture>(town->clientInfo.townBackground, Point(0, curY), EImageBlitMode::OPAQUE);
		// The engine renders CPicture at native size. For the wiki we just
		// place it; the viewport clips horizontally and scrolls as needed.
		widgets.push_back(bg);

		// Overlay structure sprites on top of the background
		for(const auto & structure : town->clientInfo.structures)
		{
			if(!structure || structure->defName.empty())
				continue;

			auto sprite = std::make_shared<CAnimImage>(
				structure->defName,
				0,  // first (idle) frame
				0,
				structure->pos.x,
				curY + structure->pos.y);
			widgets.push_back(sprite);
		}

		// Advance past the background (town screens are 800×374 typically)
		curY += 374;
	}

	curY += SECTION_GAP;

	// ------------------------------------------------------------------
	// 3. Buildings table
	// ------------------------------------------------------------------
	{
		widgets.push_back(std::make_shared<CLabel>(
			W / 2, curY,
			FONT_MEDIUM, ETextAlignment::CENTER, Colors::YELLOW,
			LIBRARY->generaltexth->translate("vcmi.wiki.town.buildings")));
		curY += 20;

		// Thin separator line
		widgets.push_back(std::make_shared<TransparentFilledRectangle>(
			Rect(TEXT_LEFT, curY, W - TEXT_LEFT * 2, 1),
			ColorRGBA(180, 160, 100, 200)));
		curY += 4;

		// Column positions
		const int iconX   = TEXT_LEFT;
		const int nameX   = TEXT_LEFT + ICON_SIZE + 6;
		// Cost starts at ~55% of total width
		const int costX   = static_cast<int>(W * 0.55);
		const int costW   = W - costX - COST_RIGHT_M;

		// Header row
		widgets.push_back(std::make_shared<CLabel>(
			nameX, curY, FONT_TINY, ETextAlignment::TOPLEFT, Colors::YELLOW,
			LIBRARY->generaltexth->translate("vcmi.wiki.town.buildingName")));
		widgets.push_back(std::make_shared<CLabel>(
			costX, curY, FONT_TINY, ETextAlignment::TOPLEFT, Colors::YELLOW,
			LIBRARY->generaltexth->translate("vcmi.wiki.town.cost")));
		curY += ROW_H - 4;

		// Sort buildings by ID for stable display order
		std::vector<const CBuilding *> sortedBuildings;
		for(const auto & [bid, bld] : town->buildings)
		{
			if(!bld)
				continue;
			// Skip auto/special buildings — they aren't player-buildable
			if(bld->mode == CBuilding::BUILD_AUTO || bld->mode == CBuilding::BUILD_SPECIAL)
				continue;
			sortedBuildings.push_back(bld.get());
		}
		std::sort(sortedBuildings.begin(), sortedBuildings.end(),
			[](const CBuilding * a, const CBuilding * b) { return a->bid < b->bid; });

		for(const CBuilding * bld : sortedBuildings)
		{
			// Building icon from the town's buildingsIcons atlas
			if(!town->clientInfo.buildingsIcons.empty())
			{
				widgets.push_back(std::make_shared<CAnimImage>(
					town->clientInfo.buildingsIcons,
					bld->bid.getNum(),
					0, iconX, curY));
			}

			// Building name
			widgets.push_back(std::make_shared<CLabel>(
				nameX, curY + 1,
				FONT_SMALL, ETextAlignment::TOPLEFT, Colors::WHITE,
				bld->getNameTranslated()));

			// Resource cost
			addResourceCost(widgets, bld->resources, costX, curY, costW);

			curY += ROW_H;
		}
	}

	curY += SECTION_GAP;

	// ------------------------------------------------------------------
	// 4. Creatures table (one row per tier)
	// ------------------------------------------------------------------
	{
		widgets.push_back(std::make_shared<CLabel>(
			W / 2, curY,
			FONT_MEDIUM, ETextAlignment::CENTER, Colors::YELLOW,
			LIBRARY->generaltexth->translate("vcmi.wiki.town.creatures")));
		curY += 20;

		widgets.push_back(std::make_shared<TransparentFilledRectangle>(
			Rect(TEXT_LEFT, curY, W - TEXT_LEFT * 2, 1),
			ColorRGBA(180, 160, 100, 200)));
		curY += 4;

		const int iconX = TEXT_LEFT;
		const int nameX = TEXT_LEFT + ICON_SIZE + 6;
		const int costX = static_cast<int>(W * 0.55);
		const int costW = W - costX - COST_RIGHT_M;

		// Header
		widgets.push_back(std::make_shared<CLabel>(
			nameX, curY, FONT_TINY, ETextAlignment::TOPLEFT, Colors::YELLOW,
			LIBRARY->generaltexth->translate("vcmi.wiki.town.creatureName")));
		widgets.push_back(std::make_shared<CLabel>(
			costX, curY, FONT_TINY, ETextAlignment::TOPLEFT, Colors::YELLOW,
			LIBRARY->generaltexth->translate("vcmi.wiki.town.cost")));
		curY += ROW_H - 4;

		for(size_t tier = 0; tier < town->creatures.size(); ++tier)
		{
			if(town->creatures[tier].empty())
				continue;

			// Show all creatures in this tier (base + upgrades)
			for(size_t ci = 0; ci < town->creatures[tier].size(); ++ci)
			{
				const CreatureID creatureId = town->creatures[tier][ci];
				const CCreature * creature = LIBRARY->creh->objects[creatureId].get();
				if(!creature)
					continue;

				// Small creature portrait
				widgets.push_back(std::make_shared<CAnimImage>(
					AnimationPath::builtin("CPRSMALL"),
					creature->getIconIndex(),
					0, iconX, curY));

				// Tier + name
				std::string prefix = (ci == 0)
					? ("T" + std::to_string(tier + 1) + ": ")
					: std::string("       ");  // indentation for upgrade
				widgets.push_back(std::make_shared<CLabel>(
					nameX, curY + 1,
					FONT_SMALL, ETextAlignment::TOPLEFT, Colors::WHITE,
					prefix + creature->getNameSingularTranslated()));

				// Recruit cost
				addResourceCost(widgets, creature->getFullRecruitCost(), costX, curY, costW);

				curY += ROW_H;
			}
		}
	}

	return widgets;
}
