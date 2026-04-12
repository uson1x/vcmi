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

#include "../gui/Shortcut.h"
#include "../widgets/Buttons.h"
#include "../widgets/GraphicalPrimitiveCanvas.h"
#include "../widgets/Images.h"
#include "../widgets/ObjectLists.h"
#include "../widgets/Slider.h"
#include "../widgets/TextControls.h"
#include "../render/Canvas.h"
#include "../render/Colors.h"

// ============================================================================
// WikiListItem
// ============================================================================

WikiListItem::WikiListItem(size_t itemIndex, std::string itemText,
	std::function<void(WikiListItem *)> callback,
	std::optional<WikiIconInfo> iconInfo)
	: onSelected(std::move(callback))
	, text(std::move(itemText))
	, index(itemIndex)
{
	OBJECT_CONSTRUCTION;

	addUsedEvents(LCLICK | HOVER);

	int labelOffsetX = MARGIN_L;

	if(iconInfo)
	{
		const int iconY = (ITEM_H - ICON_SIZE) / 2;
		icon = std::make_shared<CAnimImage>(
			iconInfo->path,
			iconInfo->frame,
			Rect(MARGIN_L, iconY, ICON_SIZE, ICON_SIZE),
			iconInfo->group);
		labelOffsetX = MARGIN_L + ICON_SIZE + 3;
	}

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
	if(onSelected)
		onSelected(this);
}

void WikiListItem::hover(bool on)
{
	if(!selected)
	{
		label->setColor(on ? ColorRGBA(255, 220, 120, 255) : Colors::WHITE);
		redraw();
	}
}

void WikiListItem::showAll(Canvas & to)
{
	// Clip to parent (CListBox) bounds so items never spill into adjacent columns
	if(parent)
	{
		CanvasClipRectGuard guard(to, parent->pos);
		// Separator line at bottom of row
		to.drawColorBlended(Rect(pos.x, pos.y + pos.h - 1, pos.w, 1),
		                    ColorRGBA(120, 90, 40, 160));
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
static constexpr int CONTENT_BOT   = 552;
static constexpr int CONTENT_H     = CONTENT_BOT - CONTENT_TOP; // 494

// Row height – must match WikiListItem::ITEM_H
static_assert(WikiListItem::ITEM_H == 20, "ITEM_H mismatch");
static constexpr int ITEM_H        = WikiListItem::ITEM_H;
static constexpr int VISIBLE_ITEMS = CONTENT_H / ITEM_H; // 24

// Slider width
static constexpr int SLIDER_W      = 16;

// Column 1 – Categories  (x: 10 … 164)
static constexpr int COL1_X        = 10;
static constexpr int COL1_LIST_W   = 138; // list items width
static constexpr int COL1_SLIDER_X = COL1_X + COL1_LIST_W + 1; // 149

// Column 2 – Elements  (x: 170 … 344)
static constexpr int COL2_X        = 170;
static constexpr int COL2_LIST_W   = 158;
static constexpr int COL2_SLIDER_X = COL2_X + COL2_LIST_W + 1; // 329

// Column 3 – Content  (x: 350 … 790)
static constexpr int COL3_X        = 350;
static constexpr int COL3_W        = WIN_W - COL3_X - 10; // 440 incl. textbox slider

// Horizontal dividers (drawn as thin filled rects)
static constexpr int DIV1_X        = 165;
static constexpr int DIV2_X        = 345;

// Close button (IOKAY.DEF is ~52 × 28 px)
static constexpr int CLOSE_Y       = 564;

// ============================================================================
// WikiWindow – constructor
// ============================================================================

WikiWindow::WikiWindow(WikiWindow::Style style_)
	: CWindowObject(BORDERED, ImagePath::builtin("DIBOXBCK"))
	, style(style_)
{
	OBJECT_CONSTRUCTION;

	// Resize and centre
	pos = Rect(pos.x, pos.y, WIN_W, WIN_H);
	bgTexture = std::make_shared<CFilledTexture>(ImagePath::builtin("DIBOXBCK"), Rect(0, 0, WIN_W, WIN_H));
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
		Colors::YELLOW, "Wiki");

	// ---- Column headers -----------------------------------------------------
	col1Header = std::make_shared<CLabel>(
		COL1_X + (COL1_LIST_W + SLIDER_W) / 2, HEADER_Y,
		FONT_MEDIUM, ETextAlignment::CENTER,
		Colors::YELLOW, "Category");

	col2Header = std::make_shared<CLabel>(
		COL2_X + (COL2_LIST_W + SLIDER_W) / 2, HEADER_Y,
		FONT_MEDIUM, ETextAlignment::CENTER,
		Colors::YELLOW, "Entry");

	col3Header = std::make_shared<CLabel>(
		COL3_X + COL3_W / 2, HEADER_Y,
		FONT_MEDIUM, ETextAlignment::CENTER,
		Colors::YELLOW, "Information");

	// ---- Stub data ----------------------------------------------------------
	categoryNames = { "Town", "Hero", "Creature", "Artifact", "Spell", "Skill", "Terrain" };

	categoryElements = {
		// Town
		{ "Castle", "Rampart", "Tower", "Inferno", "Necropolis",
		  "Dungeon", "Stronghold", "Fortress", "Conflux" },
		// Hero
		{ "Knight", "Cleric", "Ranger", "Druid", "Alchemist",
		  "Wizard", "Death Knight", "Necromancer", "Overlord",
		  "Warlock", "Battle Mage", "Barbarian", "Witch", "Planeswalker",
		  "Elementalist" },
		// Creature
		{ "Pikeman", "Halberdier", "Archer", "Marksman", "Griffin",
		  "Royal Griffin", "Swordsman", "Crusader", "Monk", "Zealot",
		  "Cavalier", "Champion", "Angel", "Archangel" },
		// Artifact
		{ "Centaur's Axe", "Blackshard of the Dead Knight",
		  "Greater Gnoll's Flail", "Ogre's Club of Havoc",
		  "Sword of Hellfire", "Titan's Gladius",
		  "Shield of the Dwarven Lords", "Armor of Wonder",
		  "Dragon Scale Shield", "Sandals of the Saint" },
		// Spell
		{ "Bless", "Curse", "Haste", "Slow", "Bloodlust",
		  "Precision", "Weakness", "Stone Skin", "Disrupting Ray",
		  "Prayer", "Mirth", "Sorrow", "Fortune", "Misfortune",
		  "Armageddon", "Inferno", "Meteor Shower", "Fireball",
		  "Magic Arrow", "Ice Bolt", "Lightning Bolt", "Chain Lightning",
		  "Frost Ring", "Implosion", "Resurrection", "Animate Dead" },
		// Skill
		{ "Archery", "Armorer", "Artillery", "Ballistics",
		  "Diplomacy", "Eagle Eye", "Estates", "First Aid",
		  "Intelligence", "Leadership", "Learning", "Logistics",
		  "Luck", "Mysticism", "Navigation", "Necromancy",
		  "Offense", "Pathfinding", "Resistance", "Scholar",
		  "Scouting", "Sorcery", "Tactics", "Wisdom" },
		// Terrain
		{ "Grass", "Snow", "Swamp", "Rough", "Subterranean",
		  "Lava", "Water", "Rock", "Desert", "Highlands", "Wasteland" }
	};

	// ---- Category list ------------------------------------------------------
	buildCategoryList();

	// ---- Element list (initially empty) ------------------------------------
	buildElementList(-1);

	// ---- Content text box ---------------------------------------------------
	contentBox = std::make_shared<CTextBox>(
		"Select a category and an entry to view its description.",
		Rect(COL3_X, CONTENT_TOP, COL3_W, CONTENT_H),
		(style == Style::BLUE) ? CSlider::BLUE : CSlider::BROWN,
		FONT_SMALL,
		ETextAlignment::TOPLEFT,
		Colors::WHITE);

	// ---- Close button -------------------------------------------------------
	closeButton = std::make_shared<CButton>(
		Point(WIN_W / 2 - 26, CLOSE_Y),
		AnimationPath::builtin("IOKAY"),
		CButton::tooltip("", "Close"),
		std::bind(&WikiWindow::close, this),
		EShortcut::GLOBAL_RETURN);

	// Apply scroll-wheel bounds after center() so pos is finalised
	applyScrollBounds();
}

// ============================================================================
// WikiWindow – helpers
// ============================================================================

void WikiWindow::applyScrollBounds()
{
	// Allow mouse-wheel scrolling whenever the cursor is anywhere inside the window.
	if(categoryList)
	{
		auto slider = categoryList->getSlider();
		if(slider)
			slider->setScrollBounds(pos);
	}
	if(elementList)
	{
		auto slider = elementList->getSlider();
		if(slider)
			slider->setScrollBounds(pos);
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
			[this](WikiListItem * clicked) { onCategoryClicked((int)clicked->index); });
		// No icon in the default stub – icons can be added later per-entry.
		item->pos.w = COL1_LIST_W;
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
	static constexpr int COL1_SLIDER_REL_X = COL1_LIST_W + 1; // = 139

	categoryList = std::make_shared<CListBox>(
		createCatItem,
		Point(COL1_X, CONTENT_TOP),
		Point(0, ITEM_H),
		VISIBLE_ITEMS,
		categoryNames.size(),
		0,
		sliderBits,
		Rect(COL1_SLIDER_REL_X, 0, CONTENT_H, SLIDER_W));

	// FIX: CListBox never sets pos.w/h – without these the CanvasClipRectGuard
	// in WikiListItem::showAll clips to a 0×0 rect, making all text invisible.
	categoryList->pos.w = COL1_LIST_W;
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

	const std::vector<std::string> & elems =
		(categoryIndex >= 0 && categoryIndex < (int)categoryElements.size())
		? categoryElements[categoryIndex]
		: std::vector<std::string>{};

	size_t total = elems.size();

	auto createElemItem = [this, elems](size_t idx) -> std::shared_ptr<CIntObject>
	{
		std::string name = (idx < elems.size()) ? elems[idx] : "";
		auto item = std::make_shared<WikiListItem>(
			idx, name,
			[this](WikiListItem * clicked) { onElementClicked((int)clicked->index); });
		// No icon in the default stub – icons can be added later per-entry.
		item->pos.w = COL2_LIST_W;
		item->pos.h = ITEM_H;
		if((int)idx == activeElementIndex)
			item->setSelected(true);
		return item;
	};

	// See COL1 comment above for the coordinate rationale.
	static constexpr int COL2_SLIDER_REL_X = COL2_LIST_W + 1; // = 159

	const int sliderBits = (style == Style::BLUE) ? 5 : 1;

	elementList = std::make_shared<CListBox>(
		createElemItem,
		Point(COL2_X, CONTENT_TOP),
		Point(0, ITEM_H),
		VISIBLE_ITEMS,
		total,
		0,
		(total > 0) ? sliderBits : 0,
		Rect(COL2_SLIDER_REL_X, 0, CONTENT_H, SLIDER_W));

	// FIX: set pos.w/h so WikiListItem::showAll's clip rect is non-zero
	elementList->pos.w = COL2_LIST_W;
	elementList->pos.h = CONTENT_H;

	elementList->setRedrawParent(true);

	// Update scroll bounds for the freshly created slider
	applyScrollBounds();
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

	if(activeCategoryIndex < 0 || activeElementIndex < 0)
	{
		contentBox->setText("Select a category and an entry to view its description.");
		return;
	}

	const std::string & catName  = categoryNames[activeCategoryIndex];
	const auto & elems           = categoryElements[activeCategoryIndex];
	if(activeElementIndex >= (int)elems.size())
	{
		contentBox->setText("Select a category and an entry to view its description.");
		return;
	}
	const std::string & elemName = elems[activeElementIndex];

	// Stub – real content will be filled in later
	std::string text =
		"{" + elemName + "}\n\n"
		"[Category: " + catName + "]\n\n"
		"This entry is a stub.\n\n"
		"Detailed information about \"" + elemName + "\" will be added here in a future update.\n\n"
		"You can describe stats, background lore, tactical notes, and other "
		"relevant information for this entry.";

	contentBox->setText(text);
}

// ============================================================================
// WikiWindow – event handlers
// ============================================================================

void WikiWindow::onCategoryClicked(int index)
{
	activeCategoryIndex = index;
	activeElementIndex  = -1;

	// Rebuild the element list for the new category
	buildElementList(activeCategoryIndex);
	updateContent();
}

void WikiWindow::onElementClicked(int index)
{
	activeElementIndex = index;
	updateContent();
}
