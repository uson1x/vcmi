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
#include "WikiWindow.h"

#include "../../widgets/CViewport.h"
#include "../../widgets/Images.h"
#include "../../widgets/TextControls.h"
#include "../../widgets/GraphicalPrimitiveCanvas.h"

#include "../../render/Canvas.h"
#include "../../render/CanvasImage.h"
#include "../../render/IRenderHandler.h"
#include "../../render/Colors.h"
#include "../../render/IImage.h"

#include "../../GameEngine.h"
#include "../InfoWindows.h"

#include "../../../lib/GameLibrary.h"
#include "../../../lib/entities/faction/CFaction.h"
#include "../../../lib/entities/faction/CTown.h"
#include "../../../lib/entities/building/CBuilding.h"
#include "../../../lib/CCreatureHandler.h"
#include "../../../lib/ResourceSet.h"
#include "../../../lib/texts/CGeneralTextHandler.h"

#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────────────────────
// Colours for table grid – brown (default) theme
// ─────────────────────────────────────────────────────────────────────────────

static const ColorRGBA COL_TABLE_BORDER_BROWN {200, 180, 120, 220};
static const ColorRGBA COL_TABLE_INNER_BROWN  { 90,  80,  50, 160};
static const ColorRGBA COL_HEADER_FILL_BROWN  { 60,  50,  30, 140};

// ─────────────────────────────────────────────────────────────────────────────
// Colours for table grid – blue theme
// ─────────────────────────────────────────────────────────────────────────────

static const ColorRGBA COL_TABLE_BORDER_BLUE { 80, 140, 220, 220};
static const ColorRGBA COL_TABLE_INNER_BLUE  { 30,  60, 120, 160};
static const ColorRGBA COL_HEADER_FILL_BLUE  { 20,  40,  80, 140};

// ─────────────────────────────────────────────────────────────────────────────
// Layout constants
// ─────────────────────────────────────────────────────────────────────────────

static constexpr int SECTION_GAP  = 10;  ///< vertical gap between sections
static constexpr int CELL_PAD_L   =  4;  ///< left padding inside a table cell
static constexpr int CELL_PAD_T   =  2;  ///< top  padding inside a table cell
static constexpr int RES_ICON_W   = 14;  ///< resource icon width
static constexpr int RES_GAP      =  3;  ///< gap between resource entries
static constexpr int TABLE_MARGIN =  4;  ///< horizontal margin around tables

// ─────────────────────────────────────────────────────────────────────────────
// WikiTownView
// ─────────────────────────────────────────────────────────────────────────────

/// Pre-renders the town background and all structure overlays at native
/// resolution into an offscreen CanvasImage, then scales the result to
/// exactly viewportWidth pixels during showAll().
///
/// Benefits:
///  - A single drawScaled() call replaces dozens of individual widget blits.
///  - The town view never causes a horizontal scrollbar (scales to width).
///  - Structure positions are guaranteed to be correct relative to the bg.
class WikiTownView : public CIntObject
{
	std::shared_ptr<CanvasImage> offscreen; ///< pre-rendered at natural bg size
	Point scaledSize;                       ///< target draw dimensions

public:
	/// @param posY  content-local Y offset – passed as the initial CIntObject
	///              position so that addChild() computes the correct screen
	///              coordinates *before* we touch pos in the body.
	WikiTownView(const CTown * town, int vpW, int posY)
		: CIntObject(0, Point(0, posY))
	{
		if(!town)
			return;

		if(town->clientInfo.townBackground.getName().empty())
			return;

		// Load background image to determine natural size
		auto bgImg = ENGINE->renderHandler().loadImage(
			town->clientInfo.townBackground, EImageBlitMode::OPAQUE);
		if(!bgImg)
			return;

		const Point naturalSize = bgImg->dimensions();
		if(naturalSize.x <= 0 || naturalSize.y <= 0)
			return;

		// Scale so the width fits exactly into vpW
		const double scale = (double)vpW / naturalSize.x;
		scaledSize = Point(vpW, std::max(1, (int)std::round(naturalSize.y * scale)));

		// Pre-render background + structure overlays into an offscreen canvas
		offscreen = ENGINE->renderHandler().createImage(
			naturalSize, CanvasScalingPolicy::IGNORE);
		Canvas c = offscreen->getCanvas();

		c.draw(bgImg, {0, 0});

		// ── Collect structures to render ────────────────────────────
		// Mirrors CCastleInterface::recreate() logic:
		//  - Structures with no building are always shown (decorative)
		//  - Remaining structures are grouped by building base
		//  - For each group only the highest upgrade is drawn
		//  - All selected structures are sorted by z-order (pos.z)

		std::vector<const CStructure *> toRender;
		std::map<BuildingID, std::vector<const CStructure *>> groups;

		for(const auto & structure : town->clientInfo.structures)
		{
			if(!structure || structure->defName.empty())
				continue;

			if(!structure->building)
			{
				// Decorative / always-present structure
				toRender.push_back(structure.get());
				continue;
			}

			groups[structure->building->getBase()].push_back(structure.get());
		}

		for(auto & [baseId, group] : groups)
		{
			auto it = town->buildings.find(baseId);
			if(it == town->buildings.end())
				continue;

			const CBuilding * base = it->second.get();
			// Pick the structure whose building is farthest from the base
			const CStructure * best = *std::max_element(
				group.begin(), group.end(),
				[base](const CStructure * a, const CStructure * b)
				{
					return base->getDistance(a->building->bid)
					     < base->getDistance(b->building->bid);
				});
			toRender.push_back(best);
		}

		// Sort by z-order (pos.z) so overlapping sprites draw correctly
		std::sort(toRender.begin(), toRender.end(),
			[](const CStructure * a, const CStructure * b)
			{ return a->pos.z < b->pos.z; });

		// Draw structure overlays in z-order
		for(const CStructure * structure : toRender)
		{
			auto sImg = ENGINE->renderHandler().loadImage(
				structure->defName, 0, 0, EImageBlitMode::COLORKEY);
			if(sImg)
				c.draw(sImg, Point(structure->pos.x, structure->pos.y));
		}

		// Only set the size; pos.x/y were already initialised by the base
		// CIntObject(0, Point(0,posY)) constructor and adjusted to screen-
		// absolute coordinates by addChild().  Overwriting them here would
		// destroy the adjustment and make the town view draw at the wrong
		// screen position.
		pos.w = scaledSize.x;
		pos.h = scaledSize.y;
	}

	/// Height of the scaled town view; 0 if the background could not be loaded.
	int height() const { return scaledSize.y; }

	void showAll(Canvas & to) override
	{
		if(!offscreen || scaledSize.x <= 0 || scaledSize.y <= 0)
			return;

		Canvas src = offscreen->getCanvas();
		to.drawScaled(src, pos.topLeft(), scaledSize);
	}
};

// ─────────────────────────────────────────────────────────────────────────────
// Table grid helper
// ─────────────────────────────────────────────────────────────────────────────

/// Creates a GraphicalPrimitiveCanvas that draws the outer border,
/// header/body separator, column separators and inter-row dividers.
///
/// @param x,y          top-left of the table (parent-local)
/// @param totalW       total width (must equal sum of colWidths)
/// @param colWidths    width of each column in order
/// @param headerH      height of the header row
/// @param dataRowH     height of each data row
/// @param dataRowCount number of data rows (not counting the header)
/// @param blue         use blue colour theme instead of brown
static std::shared_ptr<GraphicalPrimitiveCanvas> makeTableGrid(
	int x, int y, int totalW,
	const std::vector<int> & colWidths,
	int headerH,
	int dataRowH,
	int dataRowCount,
	bool blue = false)
{
	const ColorRGBA & colBorder = blue ? COL_TABLE_BORDER_BLUE : COL_TABLE_BORDER_BROWN;
	const ColorRGBA & colInner  = blue ? COL_TABLE_INNER_BLUE  : COL_TABLE_INNER_BROWN;
	const ColorRGBA & colHeader = blue ? COL_HEADER_FILL_BLUE  : COL_HEADER_FILL_BROWN;

	const int totalH = headerH + dataRowH * dataRowCount;
	auto grid = std::make_shared<GraphicalPrimitiveCanvas>(Rect(x, y, totalW, totalH));

	// Header background fill
	grid->addBox(Point(0, 0), Point(totalW, headerH), colHeader);

	// Outer border
	grid->addRectangle(Point(0, 0), Point(totalW, totalH), colBorder);

	// Header / body separator
	grid->addLine(Point(0, headerH), Point(totalW, headerH), colBorder);

	// Horizontal inter-row dividers
	for(int r = 1; r < dataRowCount; ++r)
	{
		const int lineY = headerH + r * dataRowH;
		grid->addLine(Point(0, lineY), Point(totalW, lineY), colInner);
	}

	// Vertical column separators
	int cx = 0;
	for(int i = 0; i + 1 < (int)colWidths.size(); ++i)
	{
		cx += colWidths[i];
		grid->addLine(Point(cx, 0), Point(cx, totalH), colInner);
	}

	return grid;
}

// ─────────────────────────────────────────────────────────────────────────────
// Resource-cost helper
// ─────────────────────────────────────────────────────────────────────────────

/// Appends resource icon + amount label pairs into `out`, laid out
/// horizontally starting at (startX, y), clipped to maxWidth.
static void addResourceCost(
	std::vector<std::shared_ptr<CIntObject>> & out,
	const TResources & cost,
	int startX, int y, int maxWidth)
{
	struct ResInfo { GameResID id; int amount; };
	std::vector<ResInfo> entries;

	TResources::nziterator it(cost);
	while(it.valid())
	{
		entries.push_back({it->resType, static_cast<int>(it->resVal)});
		++it;
	}

	if(entries.empty())
		return;

	const int entryW = RES_ICON_W + 26 + RES_GAP;
	int x = startX;

	for(const auto & e : entries)
	{
		if(x + entryW > startX + maxWidth)
			break;

		out.push_back(std::make_shared<CAnimImage>(
			AnimationPath::builtin("SMALRES"), e.id.getNum(), 0, x, y));
		out.push_back(std::make_shared<CLabel>(
			x + RES_ICON_W + 7, y + 1,
			FONT_TINY, ETextAlignment::TOPLEFT, Colors::WHITE,
			std::to_string(e.amount)));

		x += entryW;
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Icon dimension helper
// ─────────────────────────────────────────────────────────────────────────────

/// Loads the first frame of an animation and returns its clamped pixel size.
static Point iconDimensions(const AnimationPath & path, int maxSide = 200)
{
	if(path.getName().empty())
		return {32, 32};

	auto img = ENGINE->renderHandler().loadImage(path, 0, 0, EImageBlitMode::COLORKEY);
	if(!img)
		return {32, 32};

	const Point d = img->dimensions();
	return {std::min(d.x, maxSide), std::min(d.y, maxSide)};
}

// ─────────────────────────────────────────────────────────────────────────────
// ClickableTableRow – transparent click area for a table row
// ─────────────────────────────────────────────────────────────────────────────

/// Invisible click overlay for one table row.  Supports left-click
/// (navigate) and right-click (popup description).
class ClickableTableRow : public CIntObject
{
	std::function<void()> onLeftClick;
	std::string popupText;
	bool hovered = false;
	bool blueTheme = false;

public:
	ClickableTableRow(Rect area, std::function<void()> lclick = nullptr,
	                  std::string rclickText = {}, bool blue = false)
		: CIntObject(LCLICK | SHOW_POPUP | HOVER, area.topLeft())
		, onLeftClick(std::move(lclick))
		, popupText(std::move(rclickText))
		, blueTheme(blue)
	{
		pos.w = area.w;
		pos.h = area.h;
	}

	void clickPressed(const Point &) override
	{
		if(onLeftClick)
			onLeftClick();
	}
	void showPopupWindow(const Point &) override
	{
		if(!popupText.empty())
			CRClickPopup::createAndPush(popupText);
	}
	void hover(bool on) override
	{
		hovered = on;
		redraw();
	}
	void showAll(Canvas & to) override
	{
		if(hovered)
		{
			const ColorRGBA hoverCol = blueTheme
				? ColorRGBA(60, 100, 180, 60)
				: ColorRGBA(180, 160, 100, 60);
			to.drawColorBlended(pos, hoverCol);
		}
		CIntObject::showAll(to);
	}
};

// ─────────────────────────────────────────────────────────────────────────────
// buildTownContent
// ─────────────────────────────────────────────────────────────────────────────

std::vector<std::shared_ptr<CIntObject>> buildTownContent(
	CViewport & viewport,
	const CFaction * faction,
	int viewportWidth,
	bool blueStyle,
	WikiNavigateCallback navigateCallback)
{
	std::vector<std::shared_ptr<CIntObject>> widgets;

	if(!faction || !faction->hasTown())
		return widgets;

	const CTown * town = faction->town.get();
	if(!town)
		return widgets;

	OBJECT_CONSTRUCTION_TARGETED(viewport.content());

	const int W  = viewportWidth;
	int       curY = 12;

	// ── 1. Town title ────────────────────────────────────────────────────

	widgets.push_back(std::make_shared<CLabel>(
		W / 2, curY,
		FONT_BIG, ETextAlignment::CENTER, Colors::YELLOW,
		faction->getNameTranslated()));
	curY += 24;

	// ── 2. Town view (background + structures, scaled to W) ─────────────

	{
		auto townView = std::make_shared<WikiTownView>(town, W, curY);
		curY += townView->height() + SECTION_GAP;
		widgets.push_back(townView);
	}

	// ── 3. Buildings table ──────────────────────────────────────────────

	{
		widgets.push_back(std::make_shared<CLabel>(
			W / 2, curY,
			FONT_MEDIUM, ETextAlignment::CENTER, Colors::YELLOW,
			LIBRARY->generaltexth->translate("vcmi.wiki.town.buildings")));
		curY += 20;

		// Actual icon dimensions from the building atlas (first frame)
		const Point iconSz = iconDimensions(town->clientInfo.buildingsIcons);
		const int rowH     = iconSz.y + 4;
		const int headerH  = 18;

		// Collect and sort buildings by ID for stable display order
		std::vector<const CBuilding *> bldRows;
		for(const auto & [bid, bld] : town->buildings)
		{
			if(!bld) continue;
			if(bld->mode == CBuilding::BUILD_AUTO || bld->mode == CBuilding::BUILD_SPECIAL) continue;
			bldRows.push_back(bld.get());
		}
		std::sort(bldRows.begin(), bldRows.end(),
			[](const CBuilding * a, const CBuilding * b){ return a->bid < b->bid; });

		// Column layout: [icon | name (50 %) | cost (50 %)]
		const int tableW  = W - TABLE_MARGIN * 2;
		const int colIcon = iconSz.x + 4;
		const int colHalf = (tableW - colIcon) / 2;
		const std::vector<int> cols = {colIcon, colHalf, tableW - colIcon - colHalf};

		// Grid lines
		widgets.push_back(makeTableGrid(
			TABLE_MARGIN, curY, tableW, cols,
			headerH, rowH, (int)bldRows.size(), blueStyle));

		// Header labels
		widgets.push_back(std::make_shared<CLabel>(
			TABLE_MARGIN + colIcon + CELL_PAD_L, curY + CELL_PAD_T,
			FONT_TINY, ETextAlignment::TOPLEFT, Colors::YELLOW,
			LIBRARY->generaltexth->translate("vcmi.wiki.town.buildingName")));
		widgets.push_back(std::make_shared<CLabel>(
			TABLE_MARGIN + colIcon + colHalf + CELL_PAD_L, curY + CELL_PAD_T,
			FONT_TINY, ETextAlignment::TOPLEFT, Colors::YELLOW,
			LIBRARY->generaltexth->translate("vcmi.wiki.town.cost")));
		curY += headerH;

		// Data rows
		for(const CBuilding * bld : bldRows)
		{
			if(!town->clientInfo.buildingsIcons.getName().empty())
				widgets.push_back(std::make_shared<CAnimImage>(
					town->clientInfo.buildingsIcons,
					bld->bid.getNum(), 0,
					TABLE_MARGIN + 2, curY + 2));

			widgets.push_back(std::make_shared<CLabel>(
				TABLE_MARGIN + colIcon + CELL_PAD_L, curY + CELL_PAD_T,
				FONT_SMALL, ETextAlignment::TOPLEFT, Colors::WHITE,
				bld->getNameTranslated()));

			addResourceCost(widgets, bld->resources,
				TABLE_MARGIN + colIcon + colHalf + CELL_PAD_L,
				curY + CELL_PAD_T,
				cols[2] - CELL_PAD_L * 2);

			// Right-click on row -> show building description
			{
				std::string desc = bld->getDescriptionTranslated();
				if(!desc.empty())
					desc = "{" + bld->getNameTranslated() + "}\n\n" + desc;
				widgets.push_back(std::make_shared<ClickableTableRow>(
					Rect(TABLE_MARGIN, curY, tableW, rowH),
					nullptr, desc, blueStyle));
			}

			curY += rowH;
		}
	}

	curY += SECTION_GAP;

	// ── 4. Creatures table ──────────────────────────────────────────────

	{
		widgets.push_back(std::make_shared<CLabel>(
			W / 2, curY,
			FONT_MEDIUM, ETextAlignment::CENTER, Colors::YELLOW,
			LIBRARY->generaltexth->translate("vcmi.wiki.town.creatures")));
		curY += 20;

		// Actual CPRSMALL icon dimensions
		const Point iconSz = iconDimensions(AnimationPath::builtin("CPRSMALL"));
		const int rowH     = iconSz.y + 4;
		const int headerH  = 18;

		// Collect creature rows (base + upgrades per tier)
		struct CreatureRow { const CCreature * creature; int tier; bool isUpgrade; };
		std::vector<CreatureRow> crRows;

		for(int tier = 0; tier < (int)town->creatures.size(); ++tier)
		{
			for(int ci = 0; ci < (int)town->creatures[tier].size(); ++ci)
			{
				const CCreature * cr =
					LIBRARY->creh->objects[town->creatures[tier][ci]].get();
				if(cr)
					crRows.push_back({cr, tier + 1, ci > 0});
			}
		}

		// Column layout: [icon | name | Lv | Atk | Def | Dmg | Spd | HP | Gr | AI]
		// Keep it compact – stat columns use FONT_TINY
		const int tableW   = W - TABLE_MARGIN * 2;
		const int colIcon  = iconSz.x + 4;
		const int statW    = 28;  // width per stat column
		const int numStats = 8;   // Lv, Atk, Def, Dmg, Spd, HP, Gr, AI
		const int nameW    = tableW - colIcon - statW * numStats;
		const std::vector<int> cols = {
			colIcon, nameW,
			statW, statW, statW, statW, statW, statW, statW, statW
		};

		// Grid lines
		widgets.push_back(makeTableGrid(
			TABLE_MARGIN, curY, tableW, cols,
			headerH, rowH, (int)crRows.size(), blueStyle));

		// Header labels
		{
			int hx = TABLE_MARGIN + colIcon;
			widgets.push_back(std::make_shared<CLabel>(
				hx + CELL_PAD_L, curY + CELL_PAD_T,
				FONT_TINY, ETextAlignment::TOPLEFT, Colors::YELLOW,
				LIBRARY->generaltexth->translate("vcmi.wiki.town.creatureName")));
			hx += nameW;

			const std::vector<std::string> statHeaders = {
				"Lv", "Atk", "Def", "Dmg", "Spd", "HP", "Gr", "AI"
			};
			for(const auto & hdr : statHeaders)
			{
				widgets.push_back(std::make_shared<CLabel>(
					hx + statW / 2, curY + CELL_PAD_T,
					FONT_TINY, ETextAlignment::TOPCENTER, Colors::YELLOW,
					hdr));
				hx += statW;
			}
		}
		curY += headerH;

		// Data rows
		for(const auto & row : crRows)
		{
			widgets.push_back(std::make_shared<CAnimImage>(
				AnimationPath::builtin("CPRSMALL"),
				row.creature->getIconIndex(), 0,
				TABLE_MARGIN + 2, curY + 2));

			// Tier prefix: "T1: " for base creature, " +  " for upgrade
			const std::string tierStr = row.isUpgrade
				? "  +  "
				: ("T" + std::to_string(row.tier) + ": ");

			widgets.push_back(std::make_shared<CLabel>(
				TABLE_MARGIN + colIcon + CELL_PAD_L, curY + CELL_PAD_T,
				FONT_SMALL, ETextAlignment::TOPLEFT, Colors::WHITE,
				tierStr + row.creature->getNameSingularTranslated()));

			// Stat values
			{
				int sx = TABLE_MARGIN + colIcon + nameW;
				const auto * cr = row.creature;

				// Damage as "min-max" or just "min" if equal
				const int dmgMin = cr->getBaseDamageMin();
				const int dmgMax = cr->getBaseDamageMax();
				const std::string dmgStr = (dmgMin == dmgMax)
					? std::to_string(dmgMin)
					: (std::to_string(dmgMin) + "-" + std::to_string(dmgMax));

				const std::vector<std::string> statVals = {
					std::to_string(cr->getLevel()),
					std::to_string(cr->getBaseAttack()),
					std::to_string(cr->getBaseDefense()),
					dmgStr,
					std::to_string(cr->getBaseSpeed()),
					std::to_string(cr->getBaseHitPoints()),
					std::to_string(cr->getGrowth()),
					std::to_string(cr->getAIValue())
				};
				for(const auto & val : statVals)
				{
					widgets.push_back(std::make_shared<CLabel>(
						sx + statW / 2, curY + CELL_PAD_T,
						FONT_TINY, ETextAlignment::TOPCENTER, Colors::WHITE,
						val));
					sx += statW;
				}
			}

			// Clickable row overlay: left-click navigates to creature, right-click shows creature description
			{
				std::function<void()> lclick;
				if(navigateCallback)
				{
					const std::string crName = row.creature->getNameSingularTranslated();
					lclick = [navigateCallback, crName]()
					{
						navigateCallback(WikiCategory::CREATURE, crName);
					};
				}
				widgets.push_back(std::make_shared<ClickableTableRow>(
					Rect(TABLE_MARGIN, curY, tableW, rowH),
					std::move(lclick), std::string{}, blueStyle));
			}

			curY += rowH;
		}
	}

	return widgets;
}
