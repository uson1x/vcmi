/*
 * WikiHeroContent.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "WikiHeroContent.h"

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
#include "../CHeroOverview.h"
#include "../CCreatureWindow.h"

#include "../../../lib/GameLibrary.h"
#include "../../../lib/CCreatureHandler.h"
#include "../../../lib/CSkillHandler.h"
#include "../../../lib/spells/CSpellHandler.h"
#include "../../../lib/entities/hero/CHero.h"
#include "../../../lib/entities/hero/CHeroClass.h"
#include "../../../lib/entities/hero/CHeroHandler.h"
#include "../../../lib/entities/hero/EHeroGender.h"
#include "../../../lib/entities/artifact/CArtifact.h"
#include "../../../lib/entities/artifact/CArtHandler.h"
#include "../../../lib/texts/CGeneralTextHandler.h"

// ─────────────────────────────────────────────────────────────────────────────
// Colour palette (mirrors WikiTownContent)
// ─────────────────────────────────────────────────────────────────────────────

static const ColorRGBA HC_BDR_BROWN {200, 180, 120, 220};
static const ColorRGBA HC_INN_BROWN  { 90,  80,  50, 160};
static const ColorRGBA HC_HDR_BROWN  { 60,  50,  30, 140};

static const ColorRGBA HC_BDR_BLUE { 80, 140, 220, 220};
static const ColorRGBA HC_INN_BLUE  { 30,  60, 120, 160};
static const ColorRGBA HC_HDR_BLUE  { 20,  40,  80, 140};

// ─────────────────────────────────────────────────────────────────────────────
// Layout constants
// ─────────────────────────────────────────────────────────────────────────────

static constexpr int MARGIN = 4;
static constexpr int GAP    = 10;
static constexpr int CELL_L = 4;
static constexpr int CELL_T = 2;

// ─────────────────────────────────────────────────────────────────────────────
// Local helpers
// ─────────────────────────────────────────────────────────────────────────────

/// 2-column label/value table
static std::shared_ptr<GraphicalPrimitiveCanvas> makeKVGrid(
	int x, int y, int totalW,
	int colLabel, int rowH, int rowCount, bool blue)
{
	const ColorRGBA & bdr = blue ? HC_BDR_BLUE : HC_BDR_BROWN;
	const ColorRGBA & inn = blue ? HC_INN_BLUE : HC_INN_BROWN;
	const int totalH = rowH * rowCount;
	auto g = std::make_shared<GraphicalPrimitiveCanvas>(Rect(x, y, totalW, totalH));
	g->addRectangle(Point(0, 0), Point(totalW, totalH), bdr);
	for(int r = 1; r < rowCount; ++r)
		g->addLine(Point(1, r * rowH), Point(totalW - 2, r * rowH), inn);
	g->addLine(Point(colLabel, 1), Point(colLabel, totalH - 2), inn);
	return g;
}

/// Thin separator grid with header row
static std::shared_ptr<GraphicalPrimitiveCanvas> makeSimpleGrid(
	int x, int y, int totalW, const std::vector<int> & colWidths,
	int headerH, int rowH, int rowCount, bool blue)
{
	const ColorRGBA & bdr = blue ? HC_BDR_BLUE : HC_BDR_BROWN;
	const ColorRGBA & inn = blue ? HC_INN_BLUE : HC_INN_BROWN;
	const ColorRGBA & hdr = blue ? HC_HDR_BLUE : HC_HDR_BROWN;
	const int totalH = headerH + rowH * rowCount;
	auto g = std::make_shared<GraphicalPrimitiveCanvas>(Rect(x, y, totalW, totalH));
	g->addBox(Point(0, 0), Point(totalW, headerH), hdr);
	g->addRectangle(Point(0, 0), Point(totalW, totalH), bdr);
	g->addLine(Point(0, headerH), Point(totalW, headerH), bdr);
	for(int r = 1; r < rowCount; ++r)
		g->addLine(Point(1, headerH + r * rowH), Point(totalW - 2, headerH + r * rowH), inn);
	int cx = 0;
	for(int i = 0; i + 1 < (int)colWidths.size(); ++i)
	{
		cx += colWidths[i];
		g->addLine(Point(cx, 1), Point(cx, totalH - 2), inn);
	}
	return g;
}

// ─────────────────────────────────────────────────────────────────────────────
// buildHeroContent
// ─────────────────────────────────────────────────────────────────────────────

std::vector<std::shared_ptr<CIntObject>> buildHeroContent(
	CViewport & viewport,
	const CHero * hero,
	int viewportWidth,
	bool blueStyle,
	WikiHeroNavigateCallback navigateCallback)
{
	std::vector<std::shared_ptr<CIntObject>> widgets;
	if(!hero) return widgets;

	OBJECT_CONSTRUCTION_TARGETED(viewport.content());
	const int W   = viewportWidth;
	int       curY = 12;

	const ColorRGBA valCol = blueStyle
		? ColorRGBA(160, 210, 255, 255)
		: Colors::YELLOW;

	// ── 1. Title ────────────────────────────────────────────────────────
	widgets.push_back(std::make_shared<CLabel>(
		W / 2, curY,
		FONT_BIG, ETextAlignment::CENTER, Colors::YELLOW,
		hero->getNameTranslated()));
	curY += 28;

	// ── 2. Large portrait + class/gender info ────────────────────────────
	{
		// Large portrait (PortraitsLarge frames are 58×64)
		widgets.push_back(std::make_shared<CAnimImage>(
			AnimationPath::builtin("PortraitsLarge"),
			hero->imageIndex, 0,
			MARGIN, curY));

		// Specialty icon (UN44, 44×44)
		widgets.push_back(std::make_shared<CAnimImage>(
			AnimationPath::builtin("UN44"),
			hero->imageIndex, 0,
			MARGIN + 64, curY));

		// Class + gender text to the right of portraits
		const int textX = MARGIN + 64 + 48;
		const std::string genderStr = (hero->gender == EHeroGender::FEMALE) ? "Female" : "Male";
		widgets.push_back(std::make_shared<CLabel>(
			textX, curY + 8,
			FONT_MEDIUM, ETextAlignment::TOPLEFT, Colors::YELLOW,
			hero->heroClass ? hero->heroClass->getNameTranslated() : ""));
		widgets.push_back(std::make_shared<CLabel>(
			textX, curY + 28,
			FONT_SMALL, ETextAlignment::TOPLEFT, Colors::WHITE,
			genderStr));

		// Clickable portrait: right-click opens CHeroOverview
		const HeroTypeID hId = hero->getId();
		auto rclick = [hId](){
			ENGINE->windows().createAndPushWindow<CHeroOverview>(hId);
		};
		// Transparent click overlay covering both portraits
		auto portClick = std::make_shared<CIntObject>(CIntObject::SHOW_POPUP, Point(MARGIN, curY));
		portClick->pos.w = 64 + 48 + 4;
		portClick->pos.h = 64;
		// Use lambda capture via addUsedEvents + override approach indirectly:
		// We add a simple CIntObject that inherits showPopupWindow
		struct PortraitClickable : public CIntObject {
			std::function<void()> cb;
			PortraitClickable(Point p, int w, int h, std::function<void()> c)
				: CIntObject(SHOW_POPUP, p), cb(std::move(c))
			{ pos.w = w; pos.h = h; }
			void showPopupWindow(const Point &) override { if(cb) cb(); }
		};
		widgets.push_back(std::make_shared<PortraitClickable>(
			Point(MARGIN, curY), 64 + 48 + 4, 64,
			std::move(rclick)));

		curY += 70 + GAP;
	}

	// ── 3. Primary skills table ─────────────────────────────────────────
	if(hero->heroClass)
	{
		curY += 4;
		const std::vector<std::string> psNames = {
			LIBRARY->generaltexth->jktexts[1],  // Attack
			LIBRARY->generaltexth->jktexts[2],  // Defense
			LIBRARY->generaltexth->jktexts[3],  // Spell Power
			LIBRARY->generaltexth->jktexts[4],  // Knowledge
		};
		const int nStats = static_cast<int>(psNames.size());
		const int tableW = W - MARGIN * 2;
		const int rowH   = 18;
		const int colLbl = tableW / 2;

		widgets.push_back(makeKVGrid(MARGIN, curY, tableW, colLbl, rowH, nStats, blueStyle));

		for(int i = 0; i < nStats; ++i)
		{
			const int ry = curY + i * rowH;
			widgets.push_back(std::make_shared<CLabel>(
				MARGIN + CELL_L, ry + CELL_T,
				FONT_SMALL, ETextAlignment::TOPLEFT, Colors::WHITE,
				psNames[i]));
			const int val = (i < (int)hero->heroClass->primarySkillInitial.size())
				? hero->heroClass->primarySkillInitial[i] : 0;
			widgets.push_back(std::make_shared<CLabel>(
				MARGIN + colLbl + CELL_L, ry + CELL_T,
				FONT_SMALL, ETextAlignment::TOPLEFT, valCol,
				std::to_string(val)));
		}
		curY += nStats * rowH + GAP;
	}

	// ── 4. Specialty ─────────────────────────────────────────────────────
	{
		const std::string specName = hero->getSpecialtyNameTranslated();
		const std::string specDesc = hero->getSpecialtyDescriptionTranslated();
		if(!specName.empty())
		{
			curY += 4;
			widgets.push_back(std::make_shared<CLabel>(
				MARGIN, curY,
				FONT_SMALL, ETextAlignment::TOPLEFT, Colors::YELLOW,
				"Specialty: " + specName));
			curY += 18;
			if(!specDesc.empty())
			{
				auto label = std::make_shared<CMultiLineLabel>(
					Rect(MARGIN, curY, W - MARGIN * 2, 4000),
					FONT_SMALL, ETextAlignment::TOPLEFT, Colors::WHITE, specDesc);
				label->pos.h = label->textSize.y;
				curY += label->textSize.y + GAP;
				widgets.push_back(std::move(label));
			}
		}
	}

	// ── 5. Starting army table ───────────────────────────────────────────
	if(!hero->initialArmy.empty())
	{
		curY += 4;
		widgets.push_back(std::make_shared<CLabel>(
			W / 2, curY,
			FONT_MEDIUM, ETextAlignment::CENTER, Colors::YELLOW,
			LIBRARY->generaltexth->translate("vcmi.heroOverview.startingArmy")));
		curY += 20;

		// Only non-war-machine entries
		struct ArmyRow { const CCreature * cr; ui32 minAmt; ui32 maxAmt; };
		std::vector<ArmyRow> armyRows;
		for(const auto & stack : hero->initialArmy)
		{
			if(stack.creature.getNum() < 0 || stack.creature.getNum() >= (int)LIBRARY->creh->objects.size())
				continue;
			const auto & cr = LIBRARY->creh->objects[stack.creature.getNum()];
			if(cr && cr->warMachine == ArtifactID::NONE)
				armyRows.push_back({cr.get(), stack.minAmount, stack.maxAmount});
		}

		if(!armyRows.empty())
		{
			const int iconW   = 28;
			const int tableW  = W - MARGIN * 2;
			const int nameW   = tableW - iconW - 60;
			const std::vector<int> cols = { iconW, nameW, 60 };
			const int headerH = 16;
			const int rowH    = 26;

			widgets.push_back(makeSimpleGrid(MARGIN, curY, tableW, cols,
				headerH, rowH, static_cast<int>(armyRows.size()), blueStyle));

			widgets.push_back(std::make_shared<CLabel>(
				MARGIN + iconW + CELL_L, curY + CELL_T,
				FONT_TINY, ETextAlignment::TOPLEFT, Colors::YELLOW, "Creature"));
			widgets.push_back(std::make_shared<CLabel>(
				MARGIN + iconW + nameW + CELL_L, curY + CELL_T,
				FONT_TINY, ETextAlignment::TOPLEFT, Colors::YELLOW, "Amount"));
			curY += headerH;

			for(const auto & ar : armyRows)
			{
				widgets.push_back(std::make_shared<CAnimImage>(
					AnimationPath::builtin("CPRSMALL"),
					ar.cr->getIconIndex(), 0,
					MARGIN + 2, curY + 2));
				widgets.push_back(std::make_shared<CLabel>(
					MARGIN + iconW + CELL_L, curY + CELL_T,
					FONT_SMALL, ETextAlignment::TOPLEFT, Colors::WHITE,
					ar.cr->getNamePluralTranslated()));
				const std::string amt = (ar.minAmt == ar.maxAmt)
					? std::to_string(ar.minAmt)
					: std::to_string(ar.minAmt) + "-" + std::to_string(ar.maxAmt);
				widgets.push_back(std::make_shared<CLabel>(
					MARGIN + iconW + nameW + CELL_L, curY + CELL_T,
					FONT_SMALL, ETextAlignment::TOPLEFT, valCol, amt));
				curY += rowH;
			}
			curY += GAP;
		}
	}

	// ── 6. Secondary skills ──────────────────────────────────────────────
	if(!hero->secSkillsInit.empty())
	{
		curY += 4;
		widgets.push_back(std::make_shared<CLabel>(
			W / 2, curY,
			FONT_MEDIUM, ETextAlignment::CENTER, Colors::YELLOW,
			LIBRARY->generaltexth->translate("vcmi.heroOverview.secondarySkills")));
		curY += 20;

		const int iconW  = 28;
		const int tableW = W - MARGIN * 2;
		const int nameW  = tableW - iconW - 80;
		const std::vector<int> cols = { iconW, nameW, 80 };
		const int headerH = 16;
		const int rowH    = 28;
		const int n       = static_cast<int>(hero->secSkillsInit.size());

		widgets.push_back(makeSimpleGrid(MARGIN, curY, tableW, cols,
			headerH, rowH, n, blueStyle));
		widgets.push_back(std::make_shared<CLabel>(
			MARGIN + iconW + CELL_L, curY + CELL_T,
			FONT_TINY, ETextAlignment::TOPLEFT, Colors::YELLOW, "Skill"));
		widgets.push_back(std::make_shared<CLabel>(
			MARGIN + iconW + nameW + CELL_L, curY + CELL_T,
			FONT_TINY, ETextAlignment::TOPLEFT, Colors::YELLOW, "Level"));
		curY += headerH;

		for(const auto & [skillId, level] : hero->secSkillsInit)
		{
			if(skillId.getNum() < 0 || skillId.getNum() >= (int)LIBRARY->skillh->objects.size())
				{ curY += rowH; continue; }
			const auto & sk = LIBRARY->skillh->objects[skillId.getNum()];
			if(!sk) { curY += rowH; continue; }

			// Skill icon: SECSK32 frame index = skillIndex * 3 + level (1-3)
			const size_t frame = static_cast<size_t>(sk->getIndex() * 3 + level);
			widgets.push_back(std::make_shared<CAnimImage>(
				AnimationPath::builtin("SECSK32"), frame, 0,
				MARGIN + 2, curY + 2));
			widgets.push_back(std::make_shared<CLabel>(
				MARGIN + iconW + CELL_L, curY + CELL_T,
				FONT_SMALL, ETextAlignment::TOPLEFT, Colors::WHITE,
				sk->getNameTranslated()));
			const std::string lvlStr = (level >= 1 && level <= 3)
				? LIBRARY->generaltexth->levels[level - 1]
				: std::to_string(level);
			widgets.push_back(std::make_shared<CLabel>(
				MARGIN + iconW + nameW + CELL_L, curY + CELL_T,
				FONT_SMALL, ETextAlignment::TOPLEFT, valCol, lvlStr));
			curY += rowH;
		}
		curY += GAP;
	}

	// ── 7. Starting spells ───────────────────────────────────────────────
	if(!hero->spells.empty())
	{
		curY += 4;
		widgets.push_back(std::make_shared<CLabel>(
			W / 2, curY,
			FONT_MEDIUM, ETextAlignment::CENTER, Colors::YELLOW,
			LIBRARY->generaltexth->translate("vcmi.heroOverview.spells")));
		curY += 20;

		const int iconW  = 32;
		const int rowH    = 32;
		int px = MARGIN;
		int rowTop = curY;
		int colsInRow = 0;
		const int maxCols = (W - MARGIN * 2) / (iconW + 4);

		for(const SpellID & sid : hero->spells)
		{
			if(sid.getNum() < 0 || sid.getNum() >= (int)LIBRARY->spellh->objects.size())
				continue;
			const auto & sp = LIBRARY->spellh->objects[sid.getNum()];
			if(!sp) continue;

			widgets.push_back(std::make_shared<CAnimImage>(
				AnimationPath::builtin("SPELLBON"), static_cast<size_t>(sp->getIconIndex()),
				Rect(px, rowTop, iconW, iconW), 0));
			colsInRow++;
			px += iconW + 4;

			if(colsInRow >= maxCols)
			{
				px = MARGIN;
				rowTop += iconW + 4;
				colsInRow = 0;
			}
		}
		curY = rowTop + rowH + GAP;
	}

	// ── 8. Biography ─────────────────────────────────────────────────────
	{
		const std::string bio = hero->getBiographyTranslated();
		if(!bio.empty())
		{
			curY += 4;
			auto label = std::make_shared<CMultiLineLabel>(
				Rect(MARGIN, curY, W - MARGIN * 2, 4000),
				FONT_SMALL, ETextAlignment::TOPLEFT, Colors::WHITE, bio);
			label->pos.h = label->textSize.y;
			curY += label->textSize.y + GAP;
			widgets.push_back(std::move(label));
		}
	}

	return widgets;
}
