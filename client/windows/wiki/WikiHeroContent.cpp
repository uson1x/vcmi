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
#include "WikiCommon.h"

#include "../../widgets/CViewport.h"
#include "../../widgets/Images.h"
#include "../../widgets/TextControls.h"
#include "../../widgets/GraphicalPrimitiveCanvas.h"
#include "../../render/Canvas.h"
#include "../../render/Colors.h"
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
#include "WikiWindow.h"
#include "../InfoWindows.h"

// ─────────────────────────────────────────────────────────────────────────────
// Layout constants
// ─────────────────────────────────────────────────────────────────────────────

static constexpr int MARGIN = 4;
static constexpr int GAP    = 10;
static constexpr int CELL_L = 4;
static constexpr int CELL_T = 2;

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
		widgets.push_back(std::make_shared<CAnimImage>(
			AnimationPath::builtin("PortraitsLarge"),
			hero->imageIndex, 0,
			MARGIN, curY));

		const int textX = MARGIN + 62 + 6;
		const std::string genderStr = (hero->gender == EHeroGender::FEMALE)
			? LIBRARY->generaltexth->translate("vcmi.wiki.hero.gender.female")
			: LIBRARY->generaltexth->translate("vcmi.wiki.hero.gender.male");
		widgets.push_back(std::make_shared<CLabel>(
			textX, curY + 8,
			FONT_MEDIUM, ETextAlignment::TOPLEFT, Colors::YELLOW,
			hero->heroClass ? hero->heroClass->getNameTranslated() : ""));
		widgets.push_back(std::make_shared<CLabel>(
			textX, curY + 28,
			FONT_SMALL, ETextAlignment::TOPLEFT, Colors::WHITE,
			genderStr));

		const HeroTypeID hId = hero->getId();
		widgets.push_back(std::make_shared<WikiClickable>(
			Rect(MARGIN, curY, 62, 68),
			nullptr,
			[hId](){ ENGINE->windows().createAndPushWindow<CHeroOverview>(hId); },
			blueStyle));

		curY += 74 + GAP;
	}

	// ── 2.5. Biography ────────────────────────────────────────────────────
	{
		const std::string bio = hero->getBiographyTranslated();
		if(!bio.empty())
		{
			auto label = std::make_shared<CMultiLineLabel>(
				Rect(MARGIN, curY, W - MARGIN * 2, 4000),
				FONT_SMALL, ETextAlignment::TOPLEFT, Colors::WHITE, bio);
			label->pos.h = label->textSize.y;
			curY += label->textSize.y + GAP;
			widgets.push_back(std::move(label));
		}
	}

	// ── 3. Primary skills table ─────────────────────────────────────────
	if(hero->heroClass)
	{
		curY += 16;
		const std::vector<std::string> psNames = {
			LIBRARY->generaltexth->jktexts[1],
			LIBRARY->generaltexth->jktexts[2],
			LIBRARY->generaltexth->jktexts[3],
			LIBRARY->generaltexth->jktexts[4],
		};
		const int nStats = static_cast<int>(psNames.size());
		const int tableW = W - MARGIN * 2;
		const int rowH   = 18;
		const int colLbl = tableW / 2;

		widgets.push_back(wikiMakeTableGrid(MARGIN, curY, tableW, {colLbl, tableW - colLbl}, 0, rowH, nStats, blueStyle));

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
			curY += 16;
			widgets.push_back(std::make_shared<CLabel>(
				W / 2, curY,
				FONT_MEDIUM, ETextAlignment::CENTER, Colors::YELLOW,
				LIBRARY->generaltexth->translate("vcmi.wiki.hero.specialty")));
			curY += 20;

			const int iconCol = 48;
			const int tableW  = W - MARGIN * 2;
			const ColorRGBA bdr = wikiBorderColor(blueStyle);
			auto rowBorder = std::make_shared<GraphicalPrimitiveCanvas>(
				Rect(MARGIN, curY, tableW, 50));
			rowBorder->addRectangle(Point(0, 0), Point(tableW, 50), bdr);
			widgets.push_back(rowBorder);

			widgets.push_back(std::make_shared<CAnimImage>(
				AnimationPath::builtin("UN44"),
				hero->imageIndex, 0,
				MARGIN + 2, curY + 2));
			widgets.push_back(std::make_shared<CLabel>(
				MARGIN + iconCol + CELL_L, curY + CELL_T,
				FONT_MEDIUM, ETextAlignment::TOPLEFT, Colors::YELLOW,
				specName));
			curY += 50;

			if(!specDesc.empty())
			{
				auto label = std::make_shared<CMultiLineLabel>(
					Rect(MARGIN, curY, W - MARGIN * 2, 4000),
					FONT_SMALL, ETextAlignment::TOPLEFT, Colors::WHITE, specDesc);
				label->pos.h = label->textSize.y;
				curY += label->textSize.y + GAP;
				widgets.push_back(std::move(label));
			}
			else
			{
				curY += GAP;
			}
		}
	}

	// ── 5. Starting army table ───────────────────────────────────────────
	if(!hero->initialArmy.empty())
	{
		curY += 16;
		widgets.push_back(std::make_shared<CLabel>(
			W / 2, curY,
			FONT_MEDIUM, ETextAlignment::CENTER, Colors::YELLOW,
			LIBRARY->generaltexth->translate("vcmi.heroOverview.startingArmy")));
		curY += 20;

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
			const int iconW   = 36;
			const int tableW  = W - MARGIN * 2;
			const int nameW   = tableW - iconW - 60;
			const std::vector<int> cols = { iconW, nameW, 60 };
			const int headerH = 16;
			const int rowH    = 36;

			widgets.push_back(wikiMakeTableGrid(MARGIN, curY, tableW, cols,
				headerH, rowH, static_cast<int>(armyRows.size()), blueStyle));

			widgets.push_back(std::make_shared<CLabel>(
				MARGIN + iconW + CELL_L, curY + CELL_T,
				FONT_TINY, ETextAlignment::TOPLEFT, Colors::YELLOW,
				LIBRARY->generaltexth->translate("core.genrltxt.42")));
			widgets.push_back(std::make_shared<CLabel>(
				MARGIN + iconW + nameW + CELL_L, curY + CELL_T,
				FONT_TINY, ETextAlignment::TOPLEFT, Colors::YELLOW,
				LIBRARY->generaltexth->translate("vcmi.wiki.hero.column.amount")));
			curY += headerH;

			for(const auto & ar : armyRows)
			{
				widgets.push_back(std::make_shared<CAnimImage>(
					AnimationPath::builtin("CPRSMALL"),
					ar.cr->getIconIndex(), 0,
					MARGIN + 2, curY + 2));
				widgets.push_back(std::make_shared<CLabel>(
					MARGIN + iconW + CELL_L, curY + rowH / 2 - 5,
					FONT_SMALL, ETextAlignment::TOPLEFT, Colors::WHITE,
					ar.cr->getNamePluralTranslated()));
				const std::string amt = (ar.minAmt == ar.maxAmt)
					? std::to_string(ar.minAmt)
					: std::to_string(ar.minAmt) + "-" + std::to_string(ar.maxAmt);
				widgets.push_back(std::make_shared<CLabel>(
					MARGIN + iconW + nameW + CELL_L, curY + rowH / 2 - 5,
					FONT_SMALL, ETextAlignment::TOPLEFT, valCol, amt));

				const CreatureID crId(ar.cr->getIndex());
				std::function<void()> lclick;
				if(navigateCallback)
				{
					const std::string crKey = ar.cr->getJsonKey();
					lclick = [navigateCallback, crKey](){ navigateCallback(WikiCategory::CREATURE, crKey); };
				}
				widgets.push_back(std::make_shared<WikiClickable>(
					Rect(MARGIN, curY, tableW, rowH),
					std::move(lclick),
					[crId](){
						ENGINE->windows().createAndPushWindow<CStackWindow>(
							LIBRARY->creh->objects[crId.getNum()].get(), true);
					},
					blueStyle));
				curY += rowH;
			}
			curY += GAP;
		}
	}

	// ── 6. Secondary skills ──────────────────────────────────────────────
	if(!hero->secSkillsInit.empty())
	{
		curY += 16;
		widgets.push_back(std::make_shared<CLabel>(
			W / 2, curY,
			FONT_MEDIUM, ETextAlignment::CENTER, Colors::YELLOW,
			LIBRARY->generaltexth->translate("vcmi.heroOverview.secondarySkills")));
		curY += 20;

		const int iconW  = 36;
		const int tableW = W - MARGIN * 2;
		const int nameW  = tableW - iconW - 80;
		const std::vector<int> cols = { iconW, nameW, 80 };
		const int headerH = 16;
		const int rowH    = 36;
		const int n       = static_cast<int>(hero->secSkillsInit.size());

		widgets.push_back(wikiMakeTableGrid(MARGIN, curY, tableW, cols,
			headerH, rowH, n, blueStyle));
		widgets.push_back(std::make_shared<CLabel>(
			MARGIN + iconW + CELL_L, curY + CELL_T,
			FONT_TINY, ETextAlignment::TOPLEFT, Colors::YELLOW,
			LIBRARY->generaltexth->translate("vcmi.wiki.hero.column.skill")));
		widgets.push_back(std::make_shared<CLabel>(
			MARGIN + iconW + nameW + CELL_L, curY + CELL_T,
			FONT_TINY, ETextAlignment::TOPLEFT, Colors::YELLOW,
			LIBRARY->generaltexth->translate("vcmi.wiki.hero.column.level")));
		curY += headerH;

		for(const auto & [skillId, level] : hero->secSkillsInit)
		{
			if(skillId.getNum() < 0 || skillId.getNum() >= (int)LIBRARY->skillh->objects.size())
				{ curY += rowH; continue; }
			const auto & sk = LIBRARY->skillh->objects[skillId.getNum()];
			if(!sk) { curY += rowH; continue; }

			const size_t frame = static_cast<size_t>(sk->getIconIndex(static_cast<uint8_t>(level - 1)));
			widgets.push_back(std::make_shared<CAnimImage>(
				AnimationPath::builtin("SECSK32"), frame, 0,
				MARGIN + 2, curY + 2));
			widgets.push_back(std::make_shared<CLabel>(
				MARGIN + iconW + CELL_L, curY + rowH / 2 - 5,
				FONT_SMALL, ETextAlignment::TOPLEFT, Colors::WHITE,
				sk->getNameTranslated()));
			const std::string lvlStr = (level >= 1 && level <= 3)
				? LIBRARY->generaltexth->levels[level - 1]
				: std::to_string(level);
			widgets.push_back(std::make_shared<CLabel>(
				MARGIN + iconW + nameW + CELL_L, curY + rowH / 2 - 5,
				FONT_SMALL, ETextAlignment::TOPLEFT, valCol, lvlStr));

			const CSkill * skPtr = sk.get();
			const int skLvl = level;
			widgets.push_back(std::make_shared<WikiClickable>(
				Rect(MARGIN, curY, tableW, rowH),
				nullptr,
				[skPtr, skLvl](){
					CRClickPopup::createAndPush(skPtr->getDescriptionTranslated(skLvl));
				},
				blueStyle));
			curY += rowH;
		}
		curY += GAP;
	}

	// ── 7. Starting spells ───────────────────────────────────────────────
	if(!hero->spells.empty())
	{
		curY += 16;
		widgets.push_back(std::make_shared<CLabel>(
			W / 2, curY,
			FONT_MEDIUM, ETextAlignment::CENTER, Colors::YELLOW,
			LIBRARY->generaltexth->translate("vcmi.heroOverview.spells")));
		curY += 20;

		std::vector<const CSpell *> validSpells;
		for(const SpellID & sid : hero->spells)
		{
			if(sid.getNum() < 0 || sid.getNum() >= (int)LIBRARY->spellh->objects.size()) continue;
			const auto & sp = LIBRARY->spellh->objects[sid.getNum()];
			if(sp) validSpells.push_back(sp.get());
		}

		if(!validSpells.empty())
		{
			const int iconW   = 36;
			const int tableW  = W - MARGIN * 2;
			const int nameW   = tableW - iconW;
			const std::vector<int> cols = { iconW, nameW };
			const int headerH = 16;
			const int rowH    = 36;

			widgets.push_back(wikiMakeTableGrid(MARGIN, curY, tableW, cols,
				headerH, rowH, static_cast<int>(validSpells.size()), blueStyle));
			widgets.push_back(std::make_shared<CLabel>(
				MARGIN + iconW + CELL_L, curY + CELL_T,
				FONT_TINY, ETextAlignment::TOPLEFT, Colors::YELLOW,
				LIBRARY->generaltexth->translate("vcmi.wiki.hero.column.spell")));
			curY += headerH;

			for(const CSpell * sp : validSpells)
			{
				widgets.push_back(std::make_shared<CAnimImage>(
					AnimationPath::builtin("SPELLBON"),
					static_cast<size_t>(sp->getIconIndex()),
					Rect(MARGIN + 2, curY + 2, 32, 32), 0));
				widgets.push_back(std::make_shared<CLabel>(
					MARGIN + iconW + CELL_L, curY + rowH / 2 - 5,
					FONT_SMALL, ETextAlignment::TOPLEFT, Colors::WHITE,
					sp->getNameTranslated()));

				std::function<void()> lclick;
				if(navigateCallback)
				{
					const std::string spId = sp->getJsonKey();
					lclick = [navigateCallback, spId](){ navigateCallback(WikiCategory::SPELL, spId); };
				}
				const CSpell * spPtr = sp;
				widgets.push_back(std::make_shared<WikiClickable>(
					Rect(MARGIN, curY, tableW, rowH),
					std::move(lclick),
					[spPtr](){
						CRClickPopup::createAndPush(spPtr->getDescriptionTranslated(0));
					},
					blueStyle));
				curY += rowH;
			}
			curY += GAP;
		}
	}

	return widgets;
}
