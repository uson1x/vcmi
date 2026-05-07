/*
 * WikiCommon.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "WikiCommon.h"

#include "../../widgets/Images.h"
#include "../../widgets/TextControls.h"
#include "../../render/Canvas.h"
#include "../../render/Colors.h"
#include "../../GameEngine.h"
#include "../../eventsSDL/InputHandler.h"

#include "../../../lib/filesystem/ResourcePath.h"

// ─────────────────────────────────────────────────────────────────────────────
// WikiClickable
// ─────────────────────────────────────────────────────────────────────────────

WikiClickable::WikiClickable(Rect area,
                              std::function<void()> lclick,
                              std::function<void()> rclick,
                              bool blue,
                              std::optional<Rect> clip)
	: CIntObject(LCLICK | SHOW_POPUP | HOVER, area.topLeft())
	, onLeftClick(std::move(lclick))
	, onRightClick(std::move(rclick))
	, clipRect(clip)
	, blueTheme(blue)
{
	pos.w = area.w;
	pos.h = area.h;
}

void WikiClickable::clickPressed(const Point & cur)
{
	if(clipRect && !clipRect->isInside(cur))
		return;
	if(onLeftClick)
		onLeftClick();
}

void WikiClickable::showPopupWindow(const Point & cur)
{
	if(clipRect && !clipRect->isInside(cur))
		return;
	if(onRightClick)
		onRightClick();
}

void WikiClickable::hover(bool on)
{
	if(hovered != on)
	{
		hovered = on;
		redraw();
	}
}

void WikiClickable::showAll(Canvas & to)
{
	const Point cur = ENGINE->getCursorPosition();
	const bool inClip = !clipRect || clipRect->isInside(cur);
	hovered = pos.isInside(cur) && inClip && ENGINE->input().getCurrentInputMode() != InputMode::TOUCH;
	if(hovered)
	{
		const ColorRGBA hoverCol = blueTheme
			? ColorRGBA(60, 100, 180, 60)
			: ColorRGBA(180, 160, 100, 60);
		to.drawColorBlended(pos, hoverCol);
	}
	CIntObject::showAll(to);
}

// ─────────────────────────────────────────────────────────────────────────────
// wikiMakeTableGrid
// ─────────────────────────────────────────────────────────────────────────────

std::shared_ptr<GraphicalPrimitiveCanvas> wikiMakeTableGrid(
	int x, int y, int totalW,
	const std::vector<int> & colWidths,
	int headerH,
	int dataRowH,
	int dataRowCount,
	bool blue)
{
	const ColorRGBA border = wikiBorderColor(blue);
	const ColorRGBA inner  = wikiInnerColor(blue);
	const int totalH = headerH + dataRowH * dataRowCount;

	auto grid = std::make_shared<GraphicalPrimitiveCanvas>(Rect(x, y, totalW, totalH));

	if(headerH > 0)
	{
		const ColorRGBA header = blue
			? ColorRGBA{20, 40, 80, 140}
			: ColorRGBA{60, 50, 30, 140};
		grid->addBox(Point(0, 0), Point(totalW, headerH), header);
	}

	grid->addRectangle(Point(0, 0), Point(totalW, totalH), border);

	if(headerH > 0)
		grid->addLine(Point(0, headerH), Point(totalW, headerH), border);

	for(int r = 1; r < dataRowCount; ++r)
	{
		const int lineY = headerH + r * dataRowH;
		grid->addLine(Point(1, lineY), Point(totalW - 2, lineY), inner);
	}

	int cx = 0;
	for(int i = 0; i + 1 < (int)colWidths.size(); ++i)
	{
		cx += colWidths[i];
		grid->addLine(Point(cx, 1), Point(cx, totalH - 2), inner);
	}

	return grid;
}

// ─────────────────────────────────────────────────────────────────────────────
// wikiAddResourceCost
// ─────────────────────────────────────────────────────────────────────────────

static constexpr int RES_ICON_W = 14;
static constexpr int RES_GAP    =  3;

void wikiAddResourceCost(
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

	// Gold always first, then others in natural enum order
	std::stable_sort(entries.begin(), entries.end(), [](const ResInfo & a, const ResInfo & b){
		const bool aGold = (a.id == GameResID::GOLD);
		const bool bGold = (b.id == GameResID::GOLD);
		if(aGold != bGold) return aGold;
		return a.id < b.id;
	});

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
