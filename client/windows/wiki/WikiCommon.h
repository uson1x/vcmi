/*
 * WikiCommon.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "../../gui/CIntObject.h"
#include "../../widgets/GraphicalPrimitiveCanvas.h"
#include "../../../lib/Color.h"
#include "../../../lib/ResourceSet.h"

#include <functional>
#include <memory>
#include <optional>
#include <vector>

class Canvas;

/// Returns the outer border colour for the given wiki theme (brown / blue).
inline ColorRGBA wikiBorderColor(bool blue) noexcept
{
	return blue ? ColorRGBA{80, 140, 220, 220} : ColorRGBA{200, 180, 120, 220};
}

/// Returns the inner divider colour for the given wiki theme.
inline ColorRGBA wikiInnerColor(bool blue) noexcept
{
	return blue ? ColorRGBA{30, 60, 120, 160} : ColorRGBA{90, 80, 50, 160};
}

/// Unified clickable overlay for wiki content rows and image areas.
/// Supports left-click (navigate), right-click (info popup), and hover highlight.
/// Pass a non-empty @p clip to restrict hit-testing to a visible viewport rect.
class WikiClickable : public CIntObject
{
	std::function<void()> onLeftClick;
	std::function<void()> onRightClick;
	std::optional<Rect>   clipRect;
	bool hovered   = false;
	bool blueTheme = false;

public:
	WikiClickable(Rect area,
	              std::function<void()> lclick,
	              std::function<void()> rclick,
	              bool blue,
	              std::optional<Rect> clip = std::nullopt);

	void clickPressed(const Point & cur) override;
	void showPopupWindow(const Point & cur) override;
	void hover(bool on) override;
	void showAll(Canvas & to) override;
};

/// Creates a styled table-grid canvas.
///
/// When @p headerH > 0 a filled header row and a separator line are added
/// above the data rows.  When @p headerH == 0 only the outer border,
/// inter-row dividers, and column separators are drawn (key-value grid style).
std::shared_ptr<GraphicalPrimitiveCanvas> wikiMakeTableGrid(
	int x, int y, int totalW,
	const std::vector<int> & colWidths,
	int headerH,
	int dataRowH,
	int dataRowCount,
	bool blue = false);

/// Appends resource icon + amount label pairs into @p out, laid out
/// horizontally from (@p startX, @p y) and clipped to @p maxWidth.
void wikiAddResourceCost(
	std::vector<std::shared_ptr<CIntObject>> & out,
	const TResources & cost,
	int startX, int y, int maxWidth);
