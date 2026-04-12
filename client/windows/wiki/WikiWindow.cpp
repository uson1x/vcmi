/*
 * WikiWindow.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "WikiWindow.h"
#include "WikiTownContent.h"

#include "../../gui/Shortcut.h"
#include "../../widgets/Buttons.h"
#include "../../widgets/CViewport.h"
#include "../../widgets/GraphicalPrimitiveCanvas.h"
#include "../../widgets/Images.h"
#include "../../widgets/ObjectLists.h"
#include "../../widgets/Slider.h"
#include "../../widgets/TextControls.h"
#include "../../render/Canvas.h"
#include "../../render/Colors.h"
#include "../../widgets/CTextInput.h"
#include "../InfoWindows.h"

#include "../../../lib/GameLibrary.h"
#include "../../../lib/entities/faction/CFaction.h"
#include "../../../lib/entities/faction/CTownHandler.h"
#include "../../../lib/entities/faction/CTown.h"
#include "../../../lib/CCreatureHandler.h"
#include "../../../lib/entities/hero/CHeroHandler.h"
#include "../../../lib/entities/artifact/CArtHandler.h"
#include "../../../lib/spells/CSpellHandler.h"
#include "../../../lib/CSkillHandler.h"
#include "../../../lib/TerrainHandler.h"
#include "../../../lib/texts/TextOperations.h"
#include "../../../lib/texts/CGeneralTextHandler.h"
#include "../../../lib/json/JsonNode.h"

// ============================================================================
// WikiListItem
// ============================================================================

WikiListItem::WikiListItem(size_t itemIndex, std::string itemText,
	std::function<void(WikiListItem *)> callback,
	std::optional<WikiIconInfo> iconInfo,
	bool blueStyle_)
	: onSelected(std::move(callback))
	, text(std::move(itemText))
	, index(itemIndex)
	, blueStyle(blueStyle_)
{
	OBJECT_CONSTRUCTION;

	addUsedEvents(LCLICK | HOVER | SHOW_POPUP);

	int labelOffsetX = MARGIN_L;

	if(iconInfo)
	{
		if(iconInfo->colorFill)
		{
			// Solid colour square (used for terrain)
			colorFillIcon = iconInfo->colorFill;
			labelOffsetX = MARGIN_L + ICON_SIZE + 5;
		}
		else
		{
			const int iconY = (ITEM_H - ICON_SIZE) / 2;
			icon = std::make_shared<CAnimImage>(
				iconInfo->path,
				iconInfo->frame,
				Rect(MARGIN_L, iconY, ICON_SIZE, ICON_SIZE),
				iconInfo->group);
			labelOffsetX = MARGIN_L + ICON_SIZE + 5;
		}
	}  // end if(iconInfo)

	const int labelY = MARGIN_TOP + 1;
	label = std::make_shared<CLabel>(labelOffsetX, labelY, FONT_SMALL, ETextAlignment::TOPLEFT, Colors::WHITE, text);
}

void WikiListItem::updateLook()
{
	if(selected)
		label->setColor(Colors::YELLOW);
	else
		label->setColor(Colors::WHITE);
	redraw();
}

void WikiListItem::setSelected(bool sel)
{
	selected = sel;
	updateLook();
}

void WikiListItem::clickPressed(const Point & cursorPosition)
{
	if(text.empty())
		return;
	if(onSelected)
		onSelected(this);
}

void WikiListItem::showPopupWindow(const Point & cursorPosition)
{
	if(!text.empty())
		CRClickPopup::createAndPush(text);
}

void WikiListItem::hover(bool on)
{
	if(text.empty())
		return;
	if(!selected)
	{
		label->setColor(on ? ColorRGBA(255, 220, 120, 255) : Colors::WHITE);
		redraw();
	}
}

void WikiListItem::showAll(Canvas & to)
{
	// Clip to parent (CListBox) bounds so items never spill into adjacent columns.
	// Exclude the slider area so text / icons do not overdraw the scrollbar.
	if(parent)
	{
		Rect clipArea = parent->pos;
		auto * lb = dynamic_cast<CListBox *>(parent);
		if(lb && lb->getSlider())
			clipArea.w -= 16; // slider width
		CanvasClipRectGuard guard(to, clipArea);

		// Draw solid-colour terrain icon square (before CIntObject children)
		if(colorFillIcon)
		{
			const int iconY = pos.y + (ITEM_H - ICON_SIZE) / 2;
			to.drawColorBlended(Rect(pos.x + MARGIN_L, iconY, ICON_SIZE, ICON_SIZE), *colorFillIcon);
		}
		// Separator line: full width without slider
		if(pos.y + pos.h < parent->pos.y + parent->pos.h)
		{
			const int sepW = (lb && lb->getSlider()) ? (pos.w - 16) : pos.w;
			to.drawColorBlended(Rect(pos.x, pos.y + pos.h - 1, sepW, 1),
			                    blueStyle ? ColorRGBA(40, 100, 200, 200) : ColorRGBA(100, 80, 55, 255));
		}
		CIntObject::showAll(to);
	}
	else
	{
		CIntObject::showAll(to);
	}
}

// ============================================================================
// WikiWindow – layout constants
// ============================================================================

// Window dimensions
static constexpr int WIN_W = 800;
static constexpr int WIN_H = 600;

// Y coordinates
static constexpr int TITLE_Y       = 14;
static constexpr int HEADER_Y      = 40;
static constexpr int CONTENT_TOP   = 58;
// CONTENT_BOT chosen so CONTENT_H is an exact multiple of ITEM_H (no half-rows)
static constexpr int CONTENT_BOT   = 538;                          // 58 + 480
static constexpr int CONTENT_H     = CONTENT_BOT - CONTENT_TOP;    // 480 = 24 * 20

// Row height – must match WikiListItem::ITEM_H
static_assert(WikiListItem::ITEM_H == 20, "ITEM_H mismatch");
static constexpr int ITEM_H        = WikiListItem::ITEM_H;
static constexpr int VISIBLE_ITEMS = CONTENT_H / ITEM_H; // 24

// Element column uses one row less to make room for the search box
static constexpr int ELEM_VISIBLE_ITEMS = VISIBLE_ITEMS - 1;        // 23
static constexpr int ELEM_LIST_H        = ELEM_VISIBLE_ITEMS * ITEM_H; // 460

// Search box placed directly below the element list
static constexpr int SEARCH_BOX_Y = CONTENT_TOP + ELEM_LIST_H + 2;  // 520
static constexpr int SEARCH_BOX_H = 16;

// Slider width
static constexpr int SLIDER_W      = 16;

// Column 1 – Categories  (x: 10 … 106)  ~60% of original
static constexpr int COL1_X        = 10;
static constexpr int COL1_LIST_W   = 80;   // ~60% of previous 138
static constexpr int COL1_SLIDER_X = COL1_X + COL1_LIST_W + 1; // 91

// Column 2 – Elements + search  (x: 108 … ~269)  reduced to ~65 % of previous
static constexpr int COL2_X        = 108;  // = COL1_X + COL1_LIST_W + SLIDER_W + 2
static constexpr int COL2_LIST_W   = 144;  // ~65 % of previous 222 – gives more room to col3
static constexpr int COL2_SLIDER_X = COL2_X + COL2_LIST_W + 1; // 253

// Column 3 – Content  (x: 270 … 790)
static constexpr int COL3_X        = COL2_X + COL2_LIST_W + SLIDER_W + 2; // 270
static constexpr int COL3_W        = WIN_W - COL3_X - 10; // 520 incl. textbox slider

// Horizontal dividers (drawn as thin filled rects)
// DIV1 sits between col1-slider and col2; DIV2 between col2-slider and col3
static constexpr int DIV1_X        = COL1_X + COL1_LIST_W + SLIDER_W + 1; // 107
static constexpr int DIV2_X        = COL2_X + COL2_LIST_W + SLIDER_W + 1; // 269

// Close button (IOKAY.DEF is ~52 × 28 px)
static constexpr int CLOSE_Y       = 564;

// ============================================================================
// WikiWindow – constructor
// ============================================================================

WikiWindow::WikiWindow(WikiWindow::Style style_, std::optional<WikiEntryKey> initialEntry)
	: CWindowObject(BORDERED)
	, style(style_)
{
	OBJECT_CONSTRUCTION;

	// Resize and centre
	pos = Rect(pos.x, pos.y, WIN_W, WIN_H);
	if(style == Style::BLUE)
	{
		auto blueBg = std::make_shared<FilledTexturePlayerColored>(Rect(0, 0, WIN_W, WIN_H));
		blueBg->setPlayerColor(PlayerColor(1));
		bgTexture = blueBg;
	}
	else
	{
		bgTexture = std::make_shared<CFilledTexture>(ImagePath::builtin("DIBOXBCK"), Rect(0, 0, WIN_W, WIN_H));
	}
	updateShadow();
	center();

	// ---- Column background tints ----------------------------------------
	const ColorRGBA borderColor = (style == Style::BLUE)
	                            ? ColorRGBA(32,  96, 192, 220)   // steel-blue
	                            : ColorRGBA(96,  72,  32, 200);  // gold-brown
	const ColorRGBA fillColor   = (style == Style::BLUE)
	                            ? ColorRGBA( 0,  16,  48,  40)
	                            : ColorRGBA( 0,   0,   0,  40);
	const ColorRGBA fillColor3  = (style == Style::BLUE)
	                            ? ColorRGBA( 0,  16,  48,  60)
	                            : ColorRGBA( 0,   0,   0,  60);

	col1Bg = std::make_shared<TransparentFilledRectangle>(
		Rect(COL1_X, CONTENT_TOP, COL1_LIST_W + SLIDER_W + 1, CONTENT_H),
		fillColor, borderColor);

	col2Bg = std::make_shared<TransparentFilledRectangle>(
		Rect(COL2_X, CONTENT_TOP, COL2_LIST_W + SLIDER_W + 1, CONTENT_H),
		fillColor, borderColor);

	col3Bg = std::make_shared<TransparentFilledRectangle>(
		Rect(COL3_X, CONTENT_TOP, COL3_W, CONTENT_H),
		fillColor3, borderColor);

	// ---- Title --------------------------------------------------------------
	titleLabel = std::make_shared<CLabel>(
		WIN_W / 2, TITLE_Y,
		FONT_BIG, ETextAlignment::CENTER,
		Colors::YELLOW, LIBRARY->generaltexth->translate("vcmi.wiki.title"));

	// ---- Column headers -----------------------------------------------------
	col1Header = std::make_shared<CLabel>(
		COL1_X + (COL1_LIST_W + SLIDER_W) / 2, HEADER_Y,
		FONT_MEDIUM, ETextAlignment::CENTER,
		Colors::YELLOW, LIBRARY->generaltexth->translate("vcmi.wiki.header.category"));

	col2Header = std::make_shared<CLabel>(
		COL2_X + (COL2_LIST_W + SLIDER_W) / 2, HEADER_Y,
		FONT_MEDIUM, ETextAlignment::CENTER,
		Colors::YELLOW, LIBRARY->generaltexth->translate("vcmi.wiki.header.entry"));

	col3Header = std::make_shared<CLabel>(
		COL3_X + COL3_W / 2, HEADER_Y,
		FONT_MEDIUM, ETextAlignment::CENTER,
		Colors::YELLOW, LIBRARY->generaltexth->translate("vcmi.wiki.header.information"));

	// ---- Game data from VCMI library ----------------------------------------
	categoryNames.resize(static_cast<int>(WikiCategory::COUNT));
	categoryNames[static_cast<int>(WikiCategory::GLOSSARY)]  = LIBRARY->generaltexth->translate("vcmi.wiki.category.glossary");
	categoryNames[static_cast<int>(WikiCategory::TOWN)]      = LIBRARY->generaltexth->translate("vcmi.wiki.category.town");
	categoryNames[static_cast<int>(WikiCategory::HERO)]      = LIBRARY->generaltexth->translate("vcmi.wiki.category.hero");
	categoryNames[static_cast<int>(WikiCategory::CREATURE)]  = LIBRARY->generaltexth->translate("vcmi.wiki.category.creature");
	categoryNames[static_cast<int>(WikiCategory::ARTIFACT)]  = LIBRARY->generaltexth->translate("vcmi.wiki.category.artifact");
	categoryNames[static_cast<int>(WikiCategory::SPELL)]     = LIBRARY->generaltexth->translate("vcmi.wiki.category.spell");
	categoryNames[static_cast<int>(WikiCategory::SKILL)]     = LIBRARY->generaltexth->translate("vcmi.wiki.category.skill");
	categoryNames[static_cast<int>(WikiCategory::TERRAIN)]   = LIBRARY->generaltexth->translate("vcmi.wiki.category.terrain");
	categoryEntries.resize(static_cast<int>(WikiCategory::COUNT));

	// Glossary – loaded from a moddable JSON file
	{
		const int iGlossary = static_cast<int>(WikiCategory::GLOSSARY);
		try
		{
			const JsonNode glossaryJson(JsonPath::builtin("config/wikiGlossary.json"));
			for(const auto & e : glossaryJson["entries"].Vector())
			{
				const std::string name = LIBRARY->generaltexth->translate(e["name"].String());
				const std::string desc = LIBRARY->generaltexth->translate(e["description"].String());
				categoryEntries[iGlossary].push_back({ name, desc, std::nullopt });
			}
		}
		catch(const std::exception &) {} // file absent → empty glossary
	}
	// Sort glossary alphabetically
	std::sort(categoryEntries[static_cast<int>(WikiCategory::GLOSSARY)].begin(),
	          categoryEntries[static_cast<int>(WikiCategory::GLOSSARY)].end(),
	          [](const WikiEntry & a, const WikiEntry & b){ return a.name < b.name; });

	// Towns – playable factions that have a town (skip neutral / special)
	{
		const int iTown = static_cast<int>(WikiCategory::TOWN);
		for(const auto & faction : LIBRARY->townh->objects)
			if(faction && faction->hasTown() && !faction->special)
				categoryEntries[iTown].push_back({
					faction->getNameTranslated(), "",
					WikiIconInfo{ AnimationPath::builtin("ITPA"), (size_t)(faction->town->clientInfo.icons[1][0] + 2), 0, std::nullopt }
				});
		std::sort(categoryEntries[iTown].begin(), categoryEntries[iTown].end(),
		          [](const WikiEntry & a, const WikiEntry & b){ return a.name < b.name; });
	}

	// Hero types – skip special (campaign-only) heroes
	{
		const int iHero = static_cast<int>(WikiCategory::HERO);
		for(const auto & hero : LIBRARY->heroh->objects)
			if(hero && !hero->special)
				categoryEntries[iHero].push_back({
					hero->getNameTranslated(), "",
					WikiIconInfo{ AnimationPath::builtin("PortraitsSmall"), (size_t)hero->getIconIndex(), 0, std::nullopt }
				});
		std::sort(categoryEntries[iHero].begin(), categoryEntries[iHero].end(),
		          [](const WikiEntry & a, const WikiEntry & b){ return a.name < b.name; });
	}

	// Build war-machine creature set so we can include them under Creatures
	std::set<CreatureID> warMachineCreatures;
	for(const auto & art : LIBRARY->arth->objects)
		if(art && art->getWarMachine() != CreatureID::NONE)
			warMachineCreatures.insert(art->getWarMachine());

	// Creatures – normal creatures plus war machines (always show war machines)
	{
		const int iCreature = static_cast<int>(WikiCategory::CREATURE);
		for(const auto & creature : LIBRARY->creh->objects)
		{
			if(!creature) continue;
			const bool isWM = warMachineCreatures.count(CreatureID(creature->getIndex())) > 0;
			if(!creature->special || isWM)
				categoryEntries[iCreature].push_back({
					creature->getNameSingularTranslated(), "",
					WikiIconInfo{ AnimationPath::builtin("CPRSMALL"), (size_t)creature->getIconIndex(), 0, std::nullopt }
				});
		}
		std::sort(categoryEntries[iCreature].begin(), categoryEntries[iCreature].end(),
		          [](const WikiEntry & a, const WikiEntry & b){ return a.name < b.name; });
	}

	// Artifacts – exclude "special" class
	{
		const int iArtifact = static_cast<int>(WikiCategory::ARTIFACT);
		for(const auto & artifact : LIBRARY->arth->objects)
			if(artifact && artifact->aClass != EArtifactClass::ART_SPECIAL)
				categoryEntries[iArtifact].push_back({
					artifact->getNameTranslated(), "",
					WikiIconInfo{ AnimationPath::builtin("Artifact"), (size_t)artifact->getIconIndex(), 0, std::nullopt }
				});
		std::sort(categoryEntries[iArtifact].begin(), categoryEntries[iArtifact].end(),
		          [](const WikiEntry & a, const WikiEntry & b){ return a.name < b.name; });
	}

	// Spells – exclude hero specials and creature abilities
	{
		const int iSpell = static_cast<int>(WikiCategory::SPELL);
		for(const auto & spell : LIBRARY->spellh->objects)
			if(spell && !spell->isSpecial() && !spell->isCreatureAbility())
				categoryEntries[iSpell].push_back({
					spell->getNameTranslated(), "",
					WikiIconInfo{ AnimationPath::builtin("SpellInt"), (size_t)spell->getIndex() + 1, 0, std::nullopt }
				});
		std::sort(categoryEntries[iSpell].begin(), categoryEntries[iSpell].end(),
		          [](const WikiEntry & a, const WikiEntry & b){ return a.name < b.name; });
	}

	// Secondary skills
	{
		const int iSkill = static_cast<int>(WikiCategory::SKILL);
		for(const auto & skill : LIBRARY->skillh->objects)
			if(skill)
				categoryEntries[iSkill].push_back({
					skill->getNameTranslated(), "",
					WikiIconInfo{ AnimationPath::builtin("SECSK32"), (size_t)(skill->getIndex() * 3 + 3), 0, std::nullopt }
				});
		std::sort(categoryEntries[iSkill].begin(), categoryEntries[iSkill].end(),
		          [](const WikiEntry & a, const WikiEntry & b){ return a.name < b.name; });
	}

	// Terrain types – minimap colour as icon
	{
		const int iTerrain = static_cast<int>(WikiCategory::TERRAIN);
		for(const auto & terrain : LIBRARY->terrainTypeHandler->objects)
			if(terrain)
			{
				WikiIconInfo colorIcon;
				colorIcon.colorFill = terrain->minimapUnblocked;
				categoryEntries[iTerrain].push_back({ terrain->getNameTranslated(), "", colorIcon });
			}
		std::sort(categoryEntries[iTerrain].begin(), categoryEntries[iTerrain].end(),
		          [](const WikiEntry & a, const WikiEntry & b){ return a.name < b.name; });
	}

	// ---- Category list ------------------------------------------------------
	buildCategoryList();

	// ---- Element list (initially empty) ------------------------------------
	buildElementList(-1);

	// ---- Element search box (below element list) ----------------------------
	{
		const Rect sbRect(COL2_X, SEARCH_BOX_Y, COL2_LIST_W + SLIDER_W, SEARCH_BOX_H);
		const ColorRGBA sbBorderColor = (style == Style::BLUE)
		                              ? ColorRGBA(40, 100, 200, 200)
		                              : ColorRGBA(128, 100,  75, 200);
		const ColorRGBA sbHintColor   = (style == Style::BLUE)
		                              ? ColorRGBA(100, 160, 240, 255)
		                              : ColorRGBA(158, 130, 105, 255);
		searchBoxRect = std::make_shared<TransparentFilledRectangle>(
			sbRect, ColorRGBA(0, 0, 0, 75), sbBorderColor);
		searchBoxHint = std::make_shared<CLabel>(
			sbRect.center().x, sbRect.center().y,
			FONT_SMALL, ETextAlignment::CENTER, sbHintColor,
			LIBRARY->generaltexth->translate("vcmi.wiki.search.hint"));
		searchBox = std::make_shared<CTextInput>(
			sbRect, FONT_SMALL, ETextAlignment::CENTER, false);
		searchBox->setCallback([this](const std::string &) { onSearchInput(); });
	}
	contentBox = std::make_shared<CTextBox>(
		LIBRARY->generaltexth->translate("vcmi.wiki.content.placeholder"),
		Rect(COL3_X + 6, CONTENT_TOP + 3, COL3_W - 12, CONTENT_H - 6),
		(style == Style::BLUE) ? CSlider::BLUE : CSlider::BROWN,
		FONT_SMALL,
		ETextAlignment::TOPLEFT,
		Colors::WHITE);

	// ---- Town viewport -----------------------------------------------
	// Completely fills the content column.  Only visible when category == Town (1).
	// Content is populated dynamically in rebuildTownViewport() when a town is selected.
	{
		static constexpr int VP_W = COL3_W - 6;   // leave 3 px margin each side
		static constexpr int VP_H = CONTENT_H - 6;

		townContentView = std::make_shared<CViewport>(
			Rect(COL3_X + 3, CONTENT_TOP + 3, VP_W, VP_H),
			Point(VP_W, VP_H),  // initial content = viewport size (no scroll)
			(style == Style::BLUE) ? CSlider::BLUE : CSlider::BROWN);
		townContentView->disable(); // shown only for Town category
	}

	// ---- Close button -----------------------------------------------------------
	closeButton = std::make_shared<CButton>(
		Point(WIN_W / 2 - 26, CLOSE_Y),
		AnimationPath::builtin(style == Style::BLUE ? "MuBchck" : "IOKAY"),
		CButton::tooltip("", LIBRARY->generaltexth->translate("vcmi.wiki.button.close")),
		std::bind(&WikiWindow::close, this),
		EShortcut::GLOBAL_RETURN);

	// ---- Back button (left of close) ----------------------------------------
	backButton = std::make_shared<CButton>(
		Point(WIN_W / 2 - 80, CLOSE_Y),
		AnimationPath::builtin(style == Style::BLUE ? "MuBcanc" : "ICANCEL"),
		CButton::tooltip("", LIBRARY->generaltexth->translate("vcmi.wiki.button.back")),
		std::bind(&WikiWindow::navigateBack, this));
	backButton->block(true); // disabled until there is history

	// Apply scroll-wheel bounds after center() so pos is finalised
	applyScrollBounds();

	// Navigate to a specific entry if requested
	if(initialEntry)
		navigateTo(*initialEntry);
}

// ============================================================================
// WikiWindow – helpers
// ============================================================================

void WikiWindow::applyScrollBounds()
{
	// scrollBounds is stored relative to the slider's own pos.
	// When the wheel event fires, the engine calculates: testArea = scrollBounds + slider->pos.
	// So we compute: scrollBounds = desired_column_rect - slider->pos.topLeft().
	if(categoryList)
	{
		auto slider = categoryList->getSlider();
		if(slider)
			slider->setScrollBounds(categoryList->pos - slider->pos.topLeft());
	}
	if(elementList)
	{
		auto slider = elementList->getSlider();
		if(slider)
			slider->setScrollBounds(elementList->pos - slider->pos.topLeft());
	}
}

void WikiWindow::buildCategoryList()
{
	OBJECT_CONSTRUCTION;

	auto createCatItem = [this](size_t idx) -> std::shared_ptr<CIntObject>
	{
		std::string name = (idx < categoryNames.size()) ? categoryNames[idx] : "";
		auto item = std::make_shared<WikiListItem>(
			idx, name,
			[this](WikiListItem * clicked) {
				// Deselect previously visible category item before updating the index
				if(activeCategoryIndex >= 0 && categoryList)
				{
					auto old = std::dynamic_pointer_cast<WikiListItem>(
						categoryList->getItem(activeCategoryIndex));
					if(old && old.get() != clicked)
						old->setSelected(false);
				}
				clicked->setSelected(true);
				onCategoryClicked((int)clicked->index);
			},
			std::nullopt,
			style == Style::BLUE);
		// No icon in the default stub – icons can be added later per-entry.
		item->pos.w = COL1_LIST_W + SLIDER_W;
		item->pos.h = ITEM_H;
		if((int)idx == activeCategoryIndex)
			item->setSelected(true);
		return item;
	};

	// Slider style bits: bit0=enabled, bit1=horizontal(0=vertical), bit2=blue
	// Brown vertical = 1,  Blue vertical = 5
	const int sliderBits = (style == Style::BLUE) ? 5 : 1;

	// Relative positions: CListBox sits at (COL1_X, CONTENT_TOP) in WikiWindow space.
	// CListBox does pos += Pos first, then OBJECT_CONSTRUCTION makes the slider
	// a child with adjustPosition=true, adding listbox.pos again.
	// Therefore pass slider position RELATIVE to the listbox origin (0,0).
	// SliderPos.w is used as slider length, SliderPos.h as visual width.
	static constexpr int COL1_SLIDER_REL_X = COL1_LIST_W + 1; // = 81

	categoryList = std::make_shared<CListBox>(
		createCatItem,
		Point(COL1_X, CONTENT_TOP),
		Point(0, ITEM_H),
		VISIBLE_ITEMS,
		categoryNames.size(),
		0,
		(categoryNames.size() > VISIBLE_ITEMS) ? sliderBits : 0,
		Rect(COL1_SLIDER_REL_X, 0, CONTENT_H, SLIDER_W));

	// FIX: CListBox never sets pos.w/h – without these the CanvasClipRectGuard
	// in WikiListItem::showAll clips to a 0×0 rect, making all text invisible.
	categoryList->pos.w = COL1_LIST_W + SLIDER_W;
	categoryList->pos.h = CONTENT_H;

	// Ensure full window repaint on scroll so the semi-transparent background
	// is cleared before items are re-drawn over it.
	categoryList->setRedrawParent(true);
}

void WikiWindow::buildElementList(int categoryIndex)
{
	// Remove the old list from parent before constructing a new one
	elementList.reset();

	OBJECT_CONSTRUCTION;

	const std::string filter = searchBox ? searchBox->getText() : "";

	const auto & allEntries =
		(categoryIndex >= 0 && categoryIndex < (int)categoryEntries.size())
		? categoryEntries[categoryIndex]
		: std::vector<WikiEntry>{};

	std::vector<WikiEntry> entries;
	if(filter.empty())
		entries = allEntries;
	else
		for(const auto & entry : allEntries)
			if(TextOperations::textSearchSimilarityScore(filter, entry.name))
				entries.push_back(entry);

	currentDisplayedEntries = entries;
	size_t total = entries.size();

	auto createElemItem = [this, entries](size_t idx) -> std::shared_ptr<CIntObject>
	{
		const std::string name = (idx < entries.size()) ? entries[idx].name : "";
		const std::optional<WikiIconInfo> icon = (idx < entries.size()) ? entries[idx].icon : std::nullopt;
		auto item = std::make_shared<WikiListItem>(
			idx, name,
			[this](WikiListItem * clicked) {
				// Deselect previously visible element item before updating the index
				if(activeElementIndex >= 0 && elementList)
				{
					auto old = std::dynamic_pointer_cast<WikiListItem>(
						elementList->getItem(activeElementIndex));
					if(old && old.get() != clicked)
						old->setSelected(false);
				}
				clicked->setSelected(true);
				onElementClicked((int)clicked->index);
			}, icon, style == Style::BLUE);
		item->pos.w = COL2_LIST_W + SLIDER_W;
		item->pos.h = ITEM_H;
		if((int)idx == activeElementIndex)
			item->setSelected(true);
		return item;
	};

	// See COL1 comment above for the coordinate rationale.
	static constexpr int COL2_SLIDER_REL_X = COL2_LIST_W + 1; // = 145

	const int sliderBits = (style == Style::BLUE) ? 5 : 1;

	elementList = std::make_shared<CListBox>(
		createElemItem,
		Point(COL2_X, CONTENT_TOP),
		Point(0, ITEM_H),
		ELEM_VISIBLE_ITEMS,
		total,
		0,
		(total > ELEM_VISIBLE_ITEMS) ? sliderBits : 0,
		Rect(COL2_SLIDER_REL_X, 0, ELEM_LIST_H, SLIDER_W));

	// FIX: set pos.w/h so WikiListItem::showAll's clip rect is non-zero
	elementList->pos.w = COL2_LIST_W + SLIDER_W;
	elementList->pos.h = ELEM_LIST_H;

	elementList->setRedrawParent(true);

	// Update scroll bounds for the freshly created slider
	applyScrollBounds();
}

void WikiWindow::onSearchInput()
{
	// Toggle placeholder hint visibility
	if(searchBoxHint)
		searchBoxHint->setEnabled(searchBox && searchBox->getText().empty());

	activeElementIndex = -1;
	buildElementList(activeCategoryIndex);
	updateContent();
}

void WikiWindow::clearElementList()
{
	activeCategoryIndex = -1;
	activeElementIndex  = -1;
	buildElementList(-1);
}

void WikiWindow::updateContent()
{
	if(!contentBox)
		return;

	// Show the town viewport only when Town category is active AND an element is selected.
	const bool useTownViewport = (activeCategoryIndex == static_cast<int>(WikiCategory::TOWN))
	                           && (activeElementIndex >= 0)
	                           && townContentView;

	// Toggle viewport / textbox visibility
	if(townContentView)
		townContentView->setEnabled(useTownViewport);
	contentBox->setEnabled(!useTownViewport);

	if(useTownViewport)
	{
		// Rebuild the viewport content if the selected town changed
		const std::string & townName = currentDisplayedEntries[activeElementIndex].name;
		if(townName != currentTownName)
			rebuildTownViewport(townName);

		// Full repaint so the background clears old contentBox pixels and
		// the viewport renders on a clean surface.  Static child widgets
		// (CLabel, CAnimImage) only paint in showAll(), not show(), so a
		// single enable()→redraw() on the viewport alone is not enough
		// once the regular per-frame show() loop takes over.
		redraw();
		return;
	}

	if(activeCategoryIndex < 0 || activeElementIndex < 0)
	{
		contentBox->setText(LIBRARY->generaltexth->translate("vcmi.wiki.content.placeholder"));
		return;
	}

	const std::string & catName  = categoryNames[activeCategoryIndex];
	if(activeElementIndex < 0 || activeElementIndex >= (int)currentDisplayedEntries.size())
	{
		contentBox->setText(LIBRARY->generaltexth->translate("vcmi.wiki.content.placeholder"));
		return;
	}
	const WikiEntry & entry = currentDisplayedEntries[activeElementIndex];

	// Use stored description if available, otherwise show a stub
	std::string text;
	if(!entry.description.empty())
	{
		text = "{" + entry.name + "}\n\n" + entry.description;
	}
	else
	{
		text =
			"{" + entry.name + "}\n\n"
			+ boost::str(boost::format(LIBRARY->generaltexth->translate("vcmi.wiki.stub.category")) % catName) + "\n\n"
			+ LIBRARY->generaltexth->translate("vcmi.wiki.stub.intro") + "\n\n"
			+ boost::str(boost::format(LIBRARY->generaltexth->translate("vcmi.wiki.stub.body")) % entry.name) + "\n\n"
			+ LIBRARY->generaltexth->translate("vcmi.wiki.stub.hint");
	}

	contentBox->setText(text);
}

void WikiWindow::rebuildTownViewport(const std::string & factionName)
{
	currentTownName = factionName;
	townContentWidgets.clear();

	// Look up the faction by translated name
	const CFaction * faction = nullptr;
	for(const auto & f : LIBRARY->townh->objects)
	{
		if(f && f->hasTown() && !f->special && f->getNameTranslated() == factionName)
		{
			faction = f.get();
			break;
		}
	}

	if(!faction || !townContentView)
		return;

	// Destroy and recreate the viewport so old children are properly removed.
	static constexpr int VP_W = COL3_W - 6;
	static constexpr int VP_H = CONTENT_H - 6;

	townContentView.reset();
	{
		OBJECT_CONSTRUCTION;
		townContentView = std::make_shared<CViewport>(
			Rect(COL3_X + 3, CONTENT_TOP + 3, VP_W, VP_H),
			Point(VP_W, VP_H),
			(style == Style::BLUE) ? CSlider::BLUE : CSlider::BROWN);
	}

	// Populate with real town data.
	// Subtract scrollbar width so tables/grids end before the scrollbar.
	const bool isBlue = (style == Style::BLUE);
	auto navCb = [this](WikiCategory cat, const std::string & name)
	{
		navigateTo(WikiEntryKey{cat, name});
	};
	townContentWidgets = buildTownContent(*townContentView, faction,
		VP_W - CViewport::SLIDER_W, isBlue, navCb);
	townContentView->fitContentSize();

	applyScrollBounds();
}

// ============================================================================
// WikiWindow – event handlers
// ============================================================================

void WikiWindow::onCategoryClicked(int index)
{
	activeCategoryIndex = index;
	activeElementIndex  = -1;

	// Clear the search filter when switching categories
	if(searchBox) { searchBox->setText(""); }
	if(searchBoxHint) { searchBoxHint->setEnabled(true); }

	// Rebuild the element list for the new category
	buildElementList(activeCategoryIndex);
	updateContent();
}

void WikiWindow::onElementClicked(int index)
{
	activeElementIndex = index;
	updateContent();
}

void WikiWindow::navigateTo(const WikiEntryKey & key)
{
	const int catIdx = static_cast<int>(key.category);
	if(catIdx < 0 || catIdx >= (int)categoryNames.size())
		return;

	// Push current entry onto navigation history (if we have a valid selection)
	if(activeCategoryIndex >= 0 && activeElementIndex >= 0
		&& activeElementIndex < (int)currentDisplayedEntries.size())
	{
		WikiCategory curCat = static_cast<WikiCategory>(activeCategoryIndex);
		navHistory.push_back(WikiEntryKey{curCat, currentDisplayedEntries[activeElementIndex].name});
	}
	if(backButton)
		backButton->block(navHistory.empty());

	activeCategoryIndex = catIdx;
	buildElementList(activeCategoryIndex);

	// Select the category item visually
	if(categoryList)
	{
		auto catItem = std::dynamic_pointer_cast<WikiListItem>(categoryList->getItem(activeCategoryIndex));
		if(catItem)
			catItem->setSelected(true);
		categoryList->scrollTo(activeCategoryIndex);
	}

	// Find the entry by name and select it
	for(int i = 0; i < (int)currentDisplayedEntries.size(); ++i)
	{
		if(currentDisplayedEntries[i].name == key.entryName)
		{
			activeElementIndex = i;
			if(elementList)
			{
				auto elemItem = std::dynamic_pointer_cast<WikiListItem>(elementList->getItem(i));
				if(elemItem)
					elemItem->setSelected(true);
				elementList->scrollTo(i);
			}
			break;
		}
	}
	updateContent();
}

void WikiWindow::navigateBack()
{
	if(navHistory.empty())
		return;

	WikiEntryKey prev = navHistory.back();
	navHistory.pop_back();

	if(backButton)
		backButton->block(navHistory.empty());

	// Navigate without pushing to history (direct implementation to avoid recursive push)
	const int catIdx = static_cast<int>(prev.category);
	if(catIdx < 0 || catIdx >= (int)categoryNames.size())
		return;

	activeCategoryIndex = catIdx;
	buildElementList(activeCategoryIndex);

	if(categoryList)
	{
		auto catItem = std::dynamic_pointer_cast<WikiListItem>(categoryList->getItem(activeCategoryIndex));
		if(catItem)
			catItem->setSelected(true);
		categoryList->scrollTo(activeCategoryIndex);
	}

	for(int i = 0; i < (int)currentDisplayedEntries.size(); ++i)
	{
		if(currentDisplayedEntries[i].name == prev.entryName)
		{
			activeElementIndex = i;
			if(elementList)
			{
				auto elemItem = std::dynamic_pointer_cast<WikiListItem>(elementList->getItem(i));
				if(elemItem)
					elemItem->setSelected(true);
				elementList->scrollTo(i);
			}
			break;
		}
	}
	updateContent();
}
