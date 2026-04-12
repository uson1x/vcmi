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
#include "../../widgets/MiscWidgets.h"
#include "../../widgets/TextControls.h"
#include "../../widgets/GraphicalPrimitiveCanvas.h"
#include "../../render/Canvas.h"
#include "../../render/Colors.h"
#include "../../GameEngine.h"
#include "../../gui/WindowHandler.h"
#include "../CCreatureWindow.h"
#include "WikiTownContent.h"   // reuse ClickableTableRow / makeTableGrid / addResourceCost via inline helpers

#include "../../../lib/CCreatureHandler.h"
#include "../../../lib/CBonusTypeHandler.h"
#include "../../../lib/GameLibrary.h"
#include "../../../lib/ResourceSet.h"
#include "../../../lib/texts/CGeneralTextHandler.h"
#include "../InfoWindows.h"

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

	// ── 2. Creature animation (CCreaturePic handles footset + faction background) ──
	{
		// Create at x=0, then use moveBy() to center based on actual bg width
		auto pic = std::make_shared<CCreaturePic>(0, curY, creature, true, true);
		const int picW = pic->pos.w;
		const int picH = pic->pos.h;
		const int centerX = (W - picW) / 2;
		pic->moveBy(Point(centerX, 0));
		widgets.push_back(pic);

		// Right-click overlay → CStackWindow popup (sized to actual CCreaturePic dimensions)
		const CreatureID crId(creature->getIndex());
		widgets.push_back(std::make_shared<ClickablePortrait>(
			Point(centerX, curY), picW, picH,
			nullptr,
			[crId](){
				ENGINE->windows().createAndPushWindow<CStackWindow>(
					LIBRARY->creh->objects[crId.getNum()].get(), true);
			},
			blueStyle));

		curY += picH + GAP;
	}

	// ── 2.5. Description text ────────────────────────────────────────────
	{
		const std::string desc = creature->getDescriptionTranslated();
		if(!desc.empty())
		{
			auto label = std::make_shared<CMultiLineLabel>(
				Rect(MARGIN, curY, W - MARGIN * 2, 4000),
				FONT_SMALL, ETextAlignment::TOPLEFT, Colors::WHITE, desc);
			label->pos.h = label->textSize.y;
			curY += label->textSize.y + GAP;
			widgets.push_back(std::move(label));
		}
	}

	// ── 3. Stats table ──────────────────────────────────────────────────
	{
		curY += 16;
		widgets.push_back(std::make_shared<CLabel>(
			W / 2, curY,
			FONT_MEDIUM, ETextAlignment::CENTER, Colors::YELLOW,
			LIBRARY->generaltexth->translate("vcmi.wiki.creature.section.stats")));
		curY += 20;
		// Collect stat rows
		auto tr = [](const char * key){ return LIBRARY->generaltexth->translate(key); };
		struct StatRow { std::string label; std::string value; };
		std::vector<StatRow> stats;

		stats.push_back({tr("vcmi.wiki.creature.stat.level"),   std::to_string(creature->getLevel())});
		stats.push_back({tr("vcmi.wiki.creature.stat.attack"),  std::to_string(creature->getBaseAttack())});
		stats.push_back({tr("vcmi.wiki.creature.stat.defense"), std::to_string(creature->getBaseDefense())});

		const int dmgMin = creature->getBaseDamageMin();
		const int dmgMax = creature->getBaseDamageMax();
		if(dmgMin == dmgMax)
			stats.push_back({tr("vcmi.wiki.creature.stat.damage"), std::to_string(dmgMin)});
		else
			stats.push_back({tr("vcmi.wiki.creature.stat.damage"), std::to_string(dmgMin) + " \xe2\x80\x93 " + std::to_string(dmgMax)});

		stats.push_back({tr("vcmi.wiki.creature.stat.speed"),     std::to_string(creature->getBaseSpeed())});
		stats.push_back({tr("vcmi.wiki.creature.stat.hitpoints"), std::to_string(creature->getBaseHitPoints())});
		stats.push_back({tr("vcmi.wiki.creature.stat.growth"),    std::to_string(creature->getGrowth())});

		if(creature->getBaseShots() > 0)
			stats.push_back({tr("vcmi.wiki.creature.stat.shots"),      std::to_string(creature->getBaseShots())});
		if(creature->getBaseSpellPoints() > 0)
			stats.push_back({tr("vcmi.wiki.creature.stat.spellpoints"), std::to_string(creature->getBaseSpellPoints())});
		if(creature->getHorde() > 0)
			stats.push_back({tr("vcmi.wiki.creature.stat.hordegrowth"), std::to_string(creature->getHorde())});
		if(creature->isDoubleWide())
			stats.push_back({tr("vcmi.wiki.creature.stat.doublewide"), "Yes"});

		const int mapMin = creature->getAdvMapAmountMin();
		const int mapMax = creature->getAdvMapAmountMax();
		if(mapMin > 0 || mapMax > 0)
		{
			const std::string mapAmt = (mapMin == mapMax)
				? std::to_string(mapMin)
				: std::to_string(mapMin) + " – " + std::to_string(mapMax);
			stats.push_back({tr("vcmi.wiki.creature.stat.mapamount"), mapAmt});
		}

		stats.push_back({tr("vcmi.wiki.creature.stat.aivalue"),    std::to_string(creature->getAIValue())});
		stats.push_back({tr("vcmi.wiki.creature.stat.fightvalue"), std::to_string(creature->getFightValue())});

		const int tableW  = W - MARGIN * 2;
		const int rowH    = 18;
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
			curY += 16;
			widgets.push_back(std::make_shared<CLabel>(
				W / 2, curY,
				FONT_MEDIUM, ETextAlignment::CENTER, Colors::YELLOW,
				LIBRARY->generaltexth->translate("vcmi.wiki.creature.section.cost")));
			curY += 20;
			addResCost(widgets, cost, MARGIN, curY, W - MARGIN * 2);
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
		std::vector<const CCreature *> bases;
		for(const auto & cr : LIBRARY->creh->objects)
		{
			if(cr && cr.get() != creature
				&& cr->upgrades.count(CreatureID(creature->getIndex())) > 0)
			{
				bases.push_back(cr.get());
			}
		}

		// Helper: render a 2-column table of related creatures
		auto addRelatedTable = [&](const std::string & sectionLabel,
		                           const std::vector<const CCreature *> & list)
		{
			if(list.empty()) return;
			curY += 16;
			// Centered section label
			widgets.push_back(std::make_shared<CLabel>(
				W / 2, curY,
				FONT_MEDIUM, ETextAlignment::CENTER, Colors::YELLOW, sectionLabel));
			curY += 20;

			const int iconW  = 36; // CPRSMALL (32px) + 4 padding
			const int rowH   = 36;
			const int tableW = W - MARGIN * 2;

			for(const CCreature * rel : list)
			{
				if(!rel) continue;

				// Row border (no fill)
				const ColorRGBA & bdr = blueStyle ? COL_BDR_BLUE : COL_BDR_BROWN;
				auto rowBg = std::make_shared<GraphicalPrimitiveCanvas>(
					Rect(MARGIN, curY, tableW, rowH));
				rowBg->addRectangle(Point(0,0), Point(tableW, rowH), bdr);
				widgets.push_back(rowBg);

				// Icon column
				widgets.push_back(std::make_shared<CAnimImage>(
					AnimationPath::builtin("CPRSMALL"),
					rel->getIconIndex(), 0,
					MARGIN + 2, curY + 2));

				// Name column
				widgets.push_back(std::make_shared<CLabel>(
					MARGIN + iconW + CELL_L, curY + rowH / 2 - 5,
					FONT_SMALL, ETextAlignment::TOPLEFT, Colors::WHITE,
					rel->getNameSingularTranslated()));

				// Clickable overlay: left-click → navigate, right-click → CStackWindow
				const std::string relName = rel->getNameSingularTranslated();
				std::function<void()> lclick;
				if(navigateCallback)
					lclick = [navigateCallback, relName](){ navigateCallback(relName); };
				const CreatureID relId(rel->getIndex());
				widgets.push_back(std::make_shared<ClickablePortrait>(
					Point(MARGIN, curY), tableW, rowH,
					std::move(lclick),
					[relId](){
						ENGINE->windows().createAndPushWindow<CStackWindow>(
							LIBRARY->creh->objects[relId.getNum()].get(), true);
					},
					blueStyle));

				curY += rowH + 2;
			}
			curY += GAP - 2;
		};

		// Show both base creatures AND upgrades if applicable
		addRelatedTable(
			LIBRARY->generaltexth->translate("vcmi.wiki.creature.upgradeOf"),
			bases);
		addRelatedTable(
			LIBRARY->generaltexth->translate("vcmi.wiki.creature.upgradeTo"),
			upgrades);
	}

	// ── 7. Abilities (creature bonuses with icons) ───────────────────────
	{
		// Collect visible non-hidden CREATURE_ABILITY bonuses, deduplicated
		struct AbilityRow
		{
			std::string text;
			ImagePath   icon;
		};
		std::vector<AbilityRow> abilityRows;
		std::set<std::pair<int,int>> seen; // (type, subtype) deduplication

		auto bonusList = creature->getBonuses(Selector::all);
		for(const auto & b : *bonusList)
		{
			if(b->hidden) continue;
			const std::string text = LIBRARY->bth->bonusToString(b);
			if(text.empty()) continue;
			const auto key = std::make_pair((int)b->type, b->subtype.getNum());
			if(!seen.insert(key).second) continue;
			abilityRows.push_back(AbilityRow{text, LIBRARY->bth->bonusToGraphics(b)});
		}

		if(!abilityRows.empty())
		{
			curY += 16;
			widgets.push_back(std::make_shared<CLabel>(
				W / 2, curY,
				FONT_MEDIUM, ETextAlignment::CENTER, Colors::YELLOW,
				LIBRARY->generaltexth->translate("vcmi.wiki.creature.section.abilities")));
			curY += 20;

			static constexpr int ICON_SIZE = 51;
			static constexpr int ICON_COL  = ICON_SIZE + 4; // column width when icon present
			const int tableW = W - MARGIN * 2;

			for(const auto & ab : abilityRows)
			{
				const bool hasIcon = !ab.icon.empty();
				const int  textX   = MARGIN + (hasIcon ? ICON_COL : 0) + CELL_L;
				const int  textW   = tableW - (hasIcon ? ICON_COL : 0) - CELL_L * 2;

				// Build the real label – measure its height to compute adaptive row size
				auto textLabel = std::make_shared<CMultiLineLabel>(
					Rect(textX, curY + CELL_T, textW, 4000),
					FONT_SMALL, ETextAlignment::TOPLEFT, Colors::WHITE, ab.text);
				const int textH = textLabel->textSize.y;

				// Row is at least tall enough to fit the icon (if any), otherwise just text
				const int minH = hasIcon ? (ICON_SIZE + CELL_T * 2) : (CELL_T * 2 + 12);
				const int rowH = std::max(minH, textH + CELL_T * 2);

				// Clamp label height to the computed row
				textLabel->pos.h = rowH - CELL_T * 2;

				// Row border (no fill)
				const ColorRGBA & bdr = blueStyle ? COL_BDR_BLUE : COL_BDR_BROWN;
				auto rowBorder = std::make_shared<GraphicalPrimitiveCanvas>(
					Rect(MARGIN, curY, tableW, rowH));
				rowBorder->addRectangle(Point(0, 0), Point(tableW, rowH), bdr);
				widgets.push_back(rowBorder);

				if(hasIcon)
					widgets.push_back(std::make_shared<CPicture>(
						ab.icon,
						MARGIN + 2,
						curY + (rowH - ICON_SIZE) / 2));

				widgets.push_back(textLabel);

				// Right-click popup with ability text
				const std::string popupText = ab.text;
				widgets.push_back(std::make_shared<ClickablePortrait>(
					Point(MARGIN, curY), tableW, rowH,
					nullptr,
					[popupText](){ CRClickPopup::createAndPush(popupText); },
					blueStyle));

				curY += rowH + 2;
			}
			curY += GAP - 2;
		}
	}

	return widgets;
}
