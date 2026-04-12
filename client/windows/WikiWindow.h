/*
 * WikiWindow.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "CWindowObject.h"
#include "../../lib/filesystem/ResourcePath.h"
#include "../../lib/Color.h"

#include <optional>

class CButton;
class CLabel;
class CListBox;
class CTextBox;
class CTextInput;
class CFilledTexture;
class TransparentFilledRectangle;
class CAnimImage;
class Canvas;

/// Optional icon descriptor for a WikiListItem row
struct WikiIconInfo
{
	AnimationPath path;    ///< empty when colorFill is used
	size_t frame = 0;
	size_t group = 0;
	std::optional<ColorRGBA> colorFill; ///< drawn as solid square when set (no CAnimImage)
};

/// A single wiki entry – name (translated), optional description and optional icon
struct WikiEntry
{
	std::string name;         ///< translated display name (shown in list)
	std::string description;  ///< full description; empty = show auto-stub text
	std::optional<WikiIconInfo> icon;
};

/// A single clickable row inside the category or element columns
class WikiListItem : public CIntObject
{
	// Private constants – sizes / margins
	static constexpr int ICON_SIZE  = 14;  ///< icon rendered at this square size
	static constexpr int MARGIN_L   = 6;   ///< left padding
	static constexpr int MARGIN_TOP = 2;   ///< top padding for text

	// Private members
	std::shared_ptr<CAnimImage>  icon;
	std::optional<ColorRGBA>     colorFillIcon; ///< used for terrain (solid color square)
	std::shared_ptr<CLabel>      label;
	bool selected = false;
	bool blueStyle = false;

	void updateLook();

public:
	static constexpr int ITEM_H = 20;  ///< row height – set on item->pos.h by callers

	std::function<void(WikiListItem *)> onSelected;
	std::string text;
	size_t index = 0;

	WikiListItem(size_t index, std::string text,
	             std::function<void(WikiListItem *)> callback,
	             std::optional<WikiIconInfo> iconInfo = std::nullopt,
	             bool blueStyle = false);

	void clickPressed(const Point & cursorPosition) override;
	void hover(bool on) override;
	void showAll(Canvas & to) override;
	void setSelected(bool sel);
	bool isSelected() const { return selected; }
};

/// In-game Glossary / Wiki - 800x600 stub window
/// Layout has three vertically-scrollable columns:
///   [Categories] | [Elements] | [Content (largest)]
class WikiWindow : public CWindowObject
{
public:
	/// Visual colour theme – matches CSlider::EStyle values
	enum class Style { BROWN = 0, BLUE = 1 };

private:
	Style style;

	// --- background & decoration -----------------------------------------
	std::shared_ptr<CFilledTexture>             bgTexture;
	std::shared_ptr<TransparentFilledRectangle> col1Bg;
	std::shared_ptr<TransparentFilledRectangle> col2Bg;
	std::shared_ptr<TransparentFilledRectangle> col3Bg;

	// --- title + column headers -------------------------------------------
	std::shared_ptr<CLabel> titleLabel;
	std::shared_ptr<CLabel> col1Header;
	std::shared_ptr<CLabel> col2Header;
	std::shared_ptr<CLabel> col3Header;

	// --- selection lists --------------------------------------------------
	std::shared_ptr<CListBox> categoryList;
	std::shared_ptr<CListBox> elementList;

	// selected state
	/// Currently active category index (-1 = none)
	int activeCategoryIndex = -1;
	/// Currently active element index within the category (-1 = none)
	int activeElementIndex  = -1;

	// stub data
	std::vector<std::string> categoryNames;
	/// Maps category index -> list of entries
	std::vector<std::vector<WikiEntry>> categoryEntries;

	// --- element search box (below element list) -------------------------
	std::shared_ptr<TransparentFilledRectangle> searchBoxRect;
	std::shared_ptr<CLabel>                     searchBoxHint;
	std::shared_ptr<CTextInput>                 searchBox;

	/// Currently displayed (possibly filtered) entries; activeElementIndex indexes into this
	std::vector<WikiEntry> currentDisplayedEntries;

	// --- content area -----------------------------------------------------
	std::shared_ptr<CTextBox> contentBox;

	// --- controls ---------------------------------------------------------
	std::shared_ptr<CButton> closeButton;

	// Helpers
	void buildCategoryList();
	void buildElementList(int categoryIndex);
	void clearElementList();
	void updateContent();
	/// Applies correct scroll bounds to both slider widgets so that
	/// mouse-wheel scrolling works when the cursor is anywhere in the window.
	void applyScrollBounds();
	/// Called when the search text in the element search box changes.
	void onSearchInput();

	void onCategoryClicked(int index);
	void onElementClicked(int index);

public:
	explicit WikiWindow(Style style = Style::BROWN);
};
