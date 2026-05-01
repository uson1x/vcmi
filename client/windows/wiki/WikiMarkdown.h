/*
 * WikiMarkdown.h, part of VCMI engine
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
#include <string>
#include <vector>

class CIntObject;
class CViewport;

/// Renders a Markdown-formatted string as a flat list of CIntObject widgets
/// suitable for populating a CViewport's content() container.
///
/// Supported syntax:
///   # / ## / ###          Headings (FONT_BIG/MEDIUM/SMALL, yellow).
///                         Default alignment is CENTER; use <left> or <right>
///                         on the preceding line to override.
///   ---  ___  ***         Horizontal rule.
///   - text  /  * text     Unordered bullet list item.
///   N. text               Ordered list item (any leading integer).
///   ![alt](path.png)      Static image via ImagePath::builtin(path).
///                         File extension is used to detect the media type;
///                         the resource system strips it at load time.
///   ![alt](path.def)      Animation – all frames played as a looping ~6 fps
///                         animation (WikiAnimLoopWidget).
///   ![alt](path.def#N)    Single static frame N of an animation.
///   ![alt](path.bik)      Video file (.bik / .smk / .webm / .mp4), looped
///                         and downscaled automatically to fit the viewport.
///   <center>  <left>  <right>
///                         Alignment tag for the next block element.
///                         Images/video default to LEFT; headings to CENTER.
///   <p>                   Explicit paragraph break (same as a blank line).
///   <br>                  Line-break.  On its own line: flushes the current
///                         paragraph without adding extra vertical gap.
///                         Inline (e.g. "first line<br>second"): inserts a
///                         newline inside the rendered label.
///   | H1 | H2 |           GFM-style pipe table.  First row is the header;
///   |----|----|           second row must be the separator (|---|---|).
///   | c1 | c2 |           Cells may contain any media syntax above.
///   [text](wiki:category/id)  Clickable inline link to another wiki page.
///                         Category names: glossary, creature, spell, hero,
///                         town, artifact, skill, terrain, mod.
///                         The id for glossary entries is the translation-key
///                         base without the trailing .name suffix, e.g.:
///                         wiki:glossary/vcmi.wiki.glossary.mdtest
///                         Requires onWikiLink callback to be non-null.
///   [![alt](media)](wiki:id) Clickable image/animation/video link.
///                         Wrapping any media line in [...](...) makes the
///                         whole media widget navigate on left-click.
///                         Right-click still shows the alt-text popup.
///   {VCMI color tags}     Passed through unchanged to all text labels.
///   regular paragraphs   Auto-wrapped CMultiLineLabel; blank line = gap.
///
/// Images wider than @p viewportWidth are downscaled preserving aspect ratio.
///
/// @param viewport       CViewport whose content() is the construction target.
/// @param markdownText   Markdown source (may contain VCMI {tag} syntax).
/// @param viewportWidth  Pixel width available for content.
/// @param blueStyle      True → blue wiki theme; false → brown theme.
/// @param onWikiLink     Optional callback invoked when a wiki link is clicked.
///                       Argument is the raw URI string, e.g. "wiki:creature/imp".
/// @return               Flat list of created widgets (parented to viewport).
std::vector<std::shared_ptr<CIntObject>> buildMarkdownContent(
	CViewport & viewport,
	const std::string & markdownText,
	int viewportWidth,
	bool blueStyle,
	std::function<void(const std::string &)> onWikiLink = nullptr);
