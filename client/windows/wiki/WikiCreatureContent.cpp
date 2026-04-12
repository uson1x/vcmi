/*
 * WikiCreatureContent.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "WikiCreatureContent.h"

#include "../../widgets/CViewport.h"
#include "../../widgets/Images.h"
#include "../../widgets/TextControls.h"
#include "../../widgets/GraphicalPrimitiveCanvas.h"
#include "../../render/Canvas.h"
#include "../../render/Colors.h"
#include "../../render/IRenderHandler.h"
#include "../../render/IImage.h"
#include "../../GameEngine.h"
#include "../../gui/WindowHandler.h"
#include "../CCreatureWindow.h"
#include "WikiTownContent.h"   // reuse ClickableTableRow / makeTableGrid / addResourceCost via inline helpers

#include "../../../lib/CCreatureHandler.h"
#include "../../../lib/GameLibrary.h"
#include "../../../lib/ResourceSet.h"
#include "../../../lib/texts/CGeneralTextHandler.h"

// ─────────────────────────────────────────────────────────────────────────────
// Colour palette (mirrors WikiTownContent)
// ─────────────────────────────────────────────────────────────────────────────

static const ColorRGBA COL_BDR_BROWN {200, 180, 120, 220};
static const ColorRGBA COL_INN_BROWN  { 90,  80,  50, 160};
static const ColorRGBA COL_HDR_BROWN  { 60,  50,  30, 140};

static const ColorRGBA COL_BDR_BLUE { 80, 140, 220, 220};
static const ColorRGBA COL_INN_BLUE  { 30,  60, 120, 160};
static const ColorRGBA COL_HDR_BLUE  { 20,  40,  80, 140};

// ─────────────────────────────────────────────────────────────────────────────
// Layout constants
// ─────────────────────────────────────────────────────────────────────────────

static constexpr int MARGIN   =  4;
static constexpr int GAP      = 10;
static constexpr int CELL_L   =  4;
static constexpr int CELL_T   =  2;
static constexpr int RES_W    = 14;
static constexpr int RES_GAP  =  3;

// ─────────────────────────────────────────────────────────────────────────────
// Local helpers
// ─────────────────────────────────────────────────────────────────────────────

static Point creImageDims(const AnimationPath & path, int maxW = 160, int maxH = 140)
{
	if(path.getName().empty()) return {64, 64};
	auto img = ENGINE->renderHandler().loadImage(path, 0, 0, EImageBlitMode::COLORKEY);
	if(!img) return {64, 64};
	Point d = img->dimensions();
	// Fit inside maxW x maxH preserving aspect
	if(d.x > maxW || d.y > maxH)
	{
		const float sx = static_cast<float>(maxW) / d.x;
		const float sy = static_cast<float>(maxH) / d.y;
		const float s  = std::min(sx, sy);
		d.x = static_cast<int>(d.x * s);
		d.y = static_cast<int>(d.y * s);
	}
	return {std::max(d.x, 1), std::max(d.y, 1)};
}

/// Small 2-column stats grid: [label | value]
static std::shared_ptr<GraphicalPrimitiveCanvas> makeStatsGrid(
	int x, int y, int totalW,
	int colLabel, int rowH, int rowCount, bool blue)
{
	const ColorRGBA & bdr = blue ? COL_BDR_BLUE : COL_BDR_BROWN;
	const ColorRGBA & inn = blue ? COL_INN_BLUE : COL_INN_BROWN;
	const int totalH = rowH * rowCount;

	auto g = std::make_shared<GraphicalPrimitiveCanvas>(Rect(x, y, totalW, totalH));
	g->addRectangle(Point(0, 0), Point(totalW, totalH), bdr);
	for(int r = 1; r < rowCount; ++r)
		g->addLine(Point(1, r * rowH), Point(totalW - 2, r * rowH), inn);
	g->addLine(Point(colLabel, 1), Point(colLabel, totalH - 2), inn);
	return g;
}

/// Resource cost helper (reimplemented locally so no dependency on WikiTownContent internals)
static void addResCost(
	std::vector<std::shared_ptr<CIntObject>> & out,
	const TResources & cost,
	int startX, int y, int maxW)
{
	struct R { GameResID id; int amount; };
	std::vector<R> entries;
	TResources::nziterator it(cost);
	while(it.valid()) { entries.push_back({it->resType, static_cast<int>(it->resVal)}); ++it; }
	if(entries.empty()) return;

	// Gold first
	std::stable_sort(entries.begin(), entries.end(), [](const R & a, const R & b){
		const bool ag = (a.id == GameResID::GOLD);
		const bool bg = (b.id == GameResID::GOLD);
		if(ag != bg) return ag;
		return a.id < b.id;
	});

	const int entW = RES_W + 26 + RES_GAP;
	int x = startX;
	for(const auto & e : entries)
	{
		if(x + entW > startX + maxW) break;
		out.push_back(std::make_shared<CAnimImage>(
			AnimationPath::builtin("SMALRES"), e.id.getNum(), 0, x, y));
		out.push_back(std::make_shared<CLabel>(
			x + RES_W + 7, y + 1,
			FONT_TINY, ETextAlignment::TOPLEFT, Colors::WHITE,
			std::to_string(e.amount)));
		x += entW;
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// ClickablePortrait – portrait icon that triggers left/right click callbacks
// ─────────────────────────────────────────────────────────────────────────────

class ClickablePortrait : public CIntObject
{
	std::function<void()> onLClick;
	std::function<void()> onRClick;
	bool hovered = false;
	bool blueTheme = false;

public:
	ClickablePortrait(Point pos_, int w, int h,
	                  std::function<void()> lclick,
	                  std::function<void()> rclick,
	                  bool blue)
		: CIntObject(LCLICK | SHOW_POPUP | HOVER, pos_)
		, onLClick(std::move(lclick))
		, onRClick(std::move(rclick))
		, blueTheme(blue)
	{
		pos.w = w;
		pos.h = h;
	}

	void clickPressed(const Point &) override { if(onLClick) onLClick(); }
	void showPopupWindow(const Point &) override { if(onRClick) onRClick(); }
	void hover(bool on) override { if(hovered != on) { hovered = on; redraw(); } }
	void showAll(Canvas & to) override
	{
		const Point cur = ENGINE->getCursorPosition();
		hovered = pos.isInside(cur);
		if(hovered)
		{
			const ColorRGBA hc = blueTheme
				? ColorRGBA(60, 100, 180, 60)
				: ColorRGBA(180, 160, 100, 60);
			to.drawColorBlended(pos, hc);
		}
		CIntObject::showAll(to);
	}
};

// ─────────────────────────────────────────────────────────────────────────────
// buildCreatureContent – main entry point
// ─────────────────────────────────────────────────────────────────────────────

std::vector<std::shared_ptr<CIntObject>> buildCreatureContent(
	CViewport & viewport,
	const CCreature * creature,
	int viewportWidth,
	bool blueStyle,
	WikiCreatureNavigateCallback navigateCallback)
{
	std::vector<std::shared_ptr<CIntObject>> widgets;
	if(!creature) return widgets;

	OBJECT_CONSTRUCTION_TARGETED(viewport.content());

	const int W   = viewportWidth;
	int       curY = 12;

	// ── 1. Title ────────────────────────────────────────────────────────
	widgets.push_back(std::make_shared<CLabel>(
		W / 2, curY,
		FONT_BIG, ETextAlignment::CENTER, Colors::YELLOW,
		creature->getNameSingularTranslated()));
	curY += 28;

	// ── 2. Animation + portraits (side by side) ──────────────────────────
	{
		// Determine sizes
		const Point animSz = creImageDims(creature->animDefName, W / 2 - MARGIN * 2, 140);
		const int animAreaW = std::max(animSz.x + MARGIN * 2, 80);

		// Battle animation (HOLDING state)
		widgets.push_back(std::make_shared<CCreatureAnim>(
			MARGIN + (animAreaW - MARGIN * 2 - animSz.x) / 2,  // centre in area
			curY,
			creature->animDefName,
			0,    // flags
			ECreatureAnimType::HOLDING));

		// Right-side: CPRSMALL portrait + plural name
		const int rightX = animAreaW + MARGIN;
		{
			// Creature portrait (CPRSMALL)
			widgets.push_back(std::make_shared<CAnimImage>(
				AnimationPath::builtin("CPRSMALL"),
				creature->getIconIndex(), 0,
				rightX, curY));
		}
		curY += std::max(animSz.y, 32) + GAP;
	}

	// ── 3. Stats table ──────────────────────────────────────────────────
	{
		curY += 8;
		// Collect stat rows
		struct StatRow { std::string label; std::string value; };
		std::vector<StatRow> stats;

		stats.push_back({"Level",       std::to_string(creature->getLevel())});
		stats.push_back({"Attack",      std::to_string(creature->getBaseAttack())});
		stats.push_back({"Defense",     std::to_string(creature->getBaseDefense())});

		const int dmgMin = creature->getBaseDamageMin();
		const int dmgMax = creature->getBaseDamageMax();
		if(dmgMin == dmgMax)
			stats.push_back({"Damage",   std::to_string(dmgMin)});
		else
			stats.push_back({"Damage",   std::to_string(dmgMin) + " \xe2\x80\x93 " + std::to_string(dmgMax)});

		stats.push_back({"Speed",       std::to_string(creature->getBaseSpeed())});
		stats.push_back({"Hit Points",  std::to_string(creature->getBaseHitPoints())});
		stats.push_back({"Growth/week", std::to_string(creature->getGrowth())});

		if(creature->getBaseShots() > 0)
			stats.push_back({"Shots",   std::to_string(creature->getBaseShots())});
		if(creature->getBaseSpellPoints() > 0)
			stats.push_back({"Spell Points", std::to_string(creature->getBaseSpellPoints())});

		stats.push_back({"AI Value",    std::to_string(creature->getAIValue())});
		stats.push_back({"Fight Value", std::to_string(creature->getFightValue())});

		const int tableW  = W - MARGIN * 2;
		const int rowH    = 16;
		const int colLbl  = tableW / 2;
		const int rowCnt  = static_cast<int>(stats.size());

		widgets.push_back(makeStatsGrid(MARGIN, curY, tableW, colLbl, rowH, rowCnt, blueStyle));

		for(int i = 0; i < rowCnt; ++i)
		{
			const int ry = curY + i * rowH;
			widgets.push_back(std::make_shared<CLabel>(
				MARGIN + CELL_L, ry + CELL_T,
				FONT_SMALL, ETextAlignment::TOPLEFT, Colors::WHITE,
				stats[i].label));
			widgets.push_back(std::make_shared<CLabel>(
				MARGIN + colLbl + CELL_L, ry + CELL_T,
				FONT_SMALL, ETextAlignment::TOPLEFT,
				blueStyle ? ColorRGBA(160, 210, 255, 255) : Colors::YELLOW,
				stats[i].value));
		}
		curY += rowCnt * rowH + GAP;
	}

	// ── 4. Cost row ─────────────────────────────────────────────────────
	{
		const TResources & cost = creature->getFullRecruitCost();
		const bool hasCost = cost.nonZero();
		if(hasCost)
		{
			curY += 4;
			widgets.push_back(std::make_shared<CLabel>(
				MARGIN, curY,
				FONT_SMALL, ETextAlignment::TOPLEFT, Colors::YELLOW,
				LIBRARY->generaltexth->translate("vcmi.wiki.creature.cost") + ":"));
			addResCost(widgets, cost, MARGIN + 60, curY, W - MARGIN * 2 - 60);
			curY += 18 + GAP;
		}
	}

	// ── 5. Upgrades / related creatures ─────────────────────────────────
	{
		// Collect upgrade targets
		std::vector<const CCreature *> upgrades;
		for(const CreatureID uid : creature->upgrades)
		{
			if(uid.getNum() >= 0 && uid.getNum() < (int)LIBRARY->creh->objects.size())
			{
				if(const auto & up = LIBRARY->creh->objects[uid.getNum()])
					upgrades.push_back(up.get());
			}
		}

		// Find base creature (any creature that has this creature in its upgrades)
		const CCreature * base = nullptr;
		for(const auto & cr : LIBRARY->creh->objects)
		{
			if(cr && cr.get() != creature
				&& cr->upgrades.count(CreatureID(creature->getIndex())) > 0)
			{
				base = cr.get();
				break;
			}
		}

		if(!upgrades.empty() || base)
		{
			curY += 4;
			widgets.push_back(std::make_shared<CLabel>(
				MARGIN, curY,
				FONT_SMALL, ETextAlignment::TOPLEFT, Colors::YELLOW,
				upgrades.empty() ? "Upgrade of:" : "Upgrades to:"));
			curY += 18;

			// Show base (if this is an upgrade) OR show upgrades
			const auto & relatedList = upgrades.empty()
				? std::vector<const CCreature *>{base}
				: upgrades;

			const int portW = 36; // CPRSMALL width + padding
			const int portH = 26;
			int px = MARGIN;

			for(const CCreature * rel : relatedList)
			{
				if(!rel) continue;

				widgets.push_back(std::make_shared<CAnimImage>(
					AnimationPath::builtin("CPRSMALL"),
					rel->getIconIndex(), 0,
					px + 2, curY + 2));

				widgets.push_back(std::make_shared<CLabel>(
					px + portW + CELL_L, curY + 4,
					FONT_TINY, ETextAlignment::TOPLEFT, Colors::WHITE,
					rel->getNameSingularTranslated()));

				// Clickable overlay
				const std::string relName = rel->getNameSingularTranslated();
				std::function<void()> lclick;
				if(navigateCallback)
					lclick = [navigateCallback, relName](){ navigateCallback(relName); };
				const CreatureID relId(rel->getIndex());
				std::function<void()> rclick = [relId](){
					ENGINE->windows().createAndPushWindow<CStackWindow>(
						LIBRARY->creh->objects[relId.getNum()].get(), true);
				};
				widgets.push_back(std::make_shared<ClickablePortrait>(
					Point(px, curY), portW + 120, portH,
					std::move(lclick), std::move(rclick), blueStyle));

				px += portW + 130;
				if(px + portW > W - MARGIN)
				{
					px   = MARGIN;
					curY += portH + 4;
				}
			}
			curY += portH + GAP;
		}
	}

	// ── 6. Description text ─────────────────────────────────────────────
	{
		const std::string desc = creature->getDescriptionTranslated();
		if(!desc.empty())
		{
			curY += 4;
			auto label = std::make_shared<CMultiLineLabel>(
				Rect(MARGIN, curY, W - MARGIN * 2, 4000),
				FONT_SMALL, ETextAlignment::TOPLEFT, Colors::WHITE, desc);
			label->pos.h = label->textSize.y;
			curY += label->textSize.y + GAP;
			widgets.push_back(std::move(label));
		}
	}

	return widgets;
}
