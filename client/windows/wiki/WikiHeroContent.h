/*
 * WikiHeroContent.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include <functional>
#include <memory>
#include <vector>

class CIntObject;
class CViewport;
class CHero;

/// Callback for wiki internal navigation from the hero page
using WikiHeroNavigateCallback = std::function<void(const std::string & heroName)>;

/**
 * Builds the rich scrollable content for a "Hero" wiki entry.
 *
 * @param viewport         The viewport whose content() node the widgets are added to.
 * @param hero             The hero type to display.  Must not be null.
 * @param viewportWidth    Usable pixel width (excluding the vertical scrollbar).
 * @param blueStyle        true = blue colour theme, false = brown/default.
 * @param navigateCallback Called when the user left-clicks a linked creature row.
 */
std::vector<std::shared_ptr<CIntObject>> buildHeroContent(
	CViewport & viewport,
	const CHero * hero,
	int viewportWidth,
	bool blueStyle,
	WikiHeroNavigateCallback navigateCallback = {});
