/*
 * WikiMarkdown.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "WikiMarkdown.h"
#include "WikiCommon.h"

#include "../../widgets/CViewport.h"
#include "../../widgets/Images.h"
#include "../../widgets/TextControls.h"
#include "../../widgets/GraphicalPrimitiveCanvas.h"
#include "../../widgets/VideoWidget.h"
#include "../InfoWindows.h"
#include "../../render/Canvas.h"
#include "../../render/CanvasImage.h"
#include "../../render/Colors.h"
#include "../../render/IFont.h"
#include "../../render/IImage.h"
#include "../../render/IRenderHandler.h"
#include "../../GameEngine.h"

// Layout constants
static constexpr int MD_MARGIN      = 4;
static constexpr int MD_GAP         = 8;
static constexpr int MD_PARA_GAP    = 4;
static constexpr int MD_BULLET_X    = MD_MARGIN + 4;
static constexpr int MD_BULLET_SZ   = 4;
static constexpr int MD_BULLET_TXT  = MD_MARGIN + 14;
static constexpr int MD_ORDERED_TXT = MD_MARGIN + 20;
static constexpr int MD_H1_PAD_TOP  = 12;
static constexpr int MD_H2_PAD_TOP  = 8;
static constexpr int MD_H3_PAD_TOP  = 4;

// Padding around inline / block code spans
static constexpr int MD_CODE_PAD_X  = 5;   ///< horizontal padding per side
static constexpr int MD_CODE_PAD_Y  = 1;   ///< vertical padding per side (inline only)

// Semi-transparent background colour for code (used for both inline and block)
static constexpr ColorRGBA MD_CODE_BLOCK_BG = {20, 20, 20, 170};

// Blockquote left accent bar and text
static constexpr int       MD_BQ_BAR_W     = 3;              ///< left bar width in pixels
static constexpr int       MD_BQ_TXT       = MD_MARGIN + 10; ///< text start x (after bar + gap)
static constexpr ColorRGBA MD_BQ_BAR_COLOR  = {100, 150, 200, 210};
static constexpr ColorRGBA MD_BQ_TEXT_COLOR = {200, 200, 200, 255};

enum class MDAlign { LEFT, CENTER, RIGHT };

static ETextAlignment toTextAlign(MDAlign a)
{
	switch(a)
	{
		case MDAlign::CENTER: return ETextAlignment::CENTER;
		case MDAlign::RIGHT:  return ETextAlignment::TOPRIGHT;
		default:              return ETextAlignment::TOPLEFT;
	}
}

static int alignedX(MDAlign a, int elemW, int margin, int contentW)
{
	switch(a)
	{
		case MDAlign::CENTER: return margin + (contentW - elemW) / 2;
		case MDAlign::RIGHT:  return margin + contentW - elemW;
		default:              return margin;
	}
}

// WikiScaledImage – renders a static image down-scaled to fit maxWidth.
class WikiScaledImage : public CIntObject
{
	std::shared_ptr<CanvasImage> offscreen;
	Point scaledSize;

public:
	WikiScaledImage(const ImagePath & path, int x, int y, int maxW)
		: CIntObject(0, Point(x, y))
	{
		auto img = ENGINE->renderHandler().loadImage(path, EImageBlitMode::COLORKEY);
		if(!img) return;
		const Point nativeSz = img->dimensions();
		if(nativeSz.x <= 0 || nativeSz.y <= 0) return;

		if(nativeSz.x <= maxW)
			scaledSize = nativeSz;
		else
		{
			const double scale = static_cast<double>(maxW) / nativeSz.x;
			scaledSize = Point(maxW, std::max(1, static_cast<int>(std::round(nativeSz.y * scale))));
		}

		offscreen = ENGINE->renderHandler().createImage(nativeSz, CanvasScalingPolicy::IGNORE);
		Canvas c  = offscreen->getCanvas();
		c.draw(img, Point(0, 0));
		pos.w = scaledSize.x;
		pos.h = scaledSize.y;
	}

	int height() const { return scaledSize.y; }

	void showAll(Canvas & to) override
	{
		if(!offscreen || scaledSize.x <= 0 || scaledSize.y <= 0) return;
		Canvas src = offscreen->getCanvas();
		to.drawScaled(src, pos.topLeft(), scaledSize);
	}
};

// WikiAnimLoopWidget – cycles all frames of a DEF animation at ~6 fps.
class WikiAnimLoopWidget : public CIntObject
{
	std::shared_ptr<CAnimImage> img;
	size_t   frameCount = 0;
	size_t   curFrame   = 0;
	uint32_t elapsed    = 0;

	static constexpr uint32_t FRAME_MS = 150;

public:
	WikiAnimLoopWidget(const AnimationPath & path, int x, int y)
		: CIntObject(TIME, Point(x, y))
	{
		OBJECT_CONSTRUCTION;
		img = std::make_shared<CAnimImage>(path, 0, 0, 0, 0);
		if(img)
		{
			frameCount = img->size();
			pos.w      = img->pos.w;
			pos.h      = img->pos.h;
		}
	}

	void tick(uint32_t msPassed) override
	{
		if(!img || frameCount <= 1) return;
		elapsed += msPassed;
		while(elapsed >= FRAME_MS)
		{
			elapsed -= FRAME_MS;
			curFrame = (curFrame + 1) % frameCount;
			img->setFrame(curFrame);
		}
		redraw();
	}
};

// WikiVideoWidget – seamlessly looped video widget with optional downscale.
class WikiVideoWidget final : public VideoWidgetBase
{
	VideoPath loopedVideo;

	void onPlaybackFinished() final { playVideo(loopedVideo); }

public:
	WikiVideoWidget(const Point & position, const VideoPath & video, float scaleFactor)
		: VideoWidgetBase(position, video, false, scaleFactor)
		, loopedVideo(video)
	{}
};

// WikiAltPopup – right-click overlay that shows the ![ alt ] text as a popup.
class WikiAltPopup : public CIntObject
{
	std::string altText;

public:
	WikiAltPopup(const Rect & area, std::string alt)
		: CIntObject(SHOW_POPUP, area.topLeft())
		, altText(std::move(alt))
	{
		pos.w = area.w;
		pos.h = area.h;
	}

	void showPopupWindow(const Point &) override
	{
		CRClickPopup::createAndPush(altText);
	}
};

// WikiLinkLabel – clickable underlined inline link to another wiki page.
class WikiLinkLabel : public CIntObject
{
	std::string linkText;
	std::string target;
	std::function<void(const std::string &)> onLink;
	bool hovered = false;

	static constexpr ColorRGBA LINK_NORMAL = {120, 180, 255, 255};
	static constexpr ColorRGBA LINK_HOVER  = {180, 220, 255, 255};

public:
	WikiLinkLabel(int x, int y, int w, int h,
	              std::string text_, std::string target_,
	              std::function<void(const std::string &)> cb)
		: CIntObject(LCLICK | HOVER)
		, linkText(std::move(text_))
		, target(std::move(target_))
		, onLink(std::move(cb))
	{
		pos.x += x; pos.y += y; pos.w = w; pos.h = h;
	}

	void showAll(Canvas & to) override
	{
		CIntObject::showAll(to);
		const ColorRGBA col = hovered ? LINK_HOVER : LINK_NORMAL;
		to.drawText(pos.topLeft(), FONT_SMALL, col, ETextAlignment::TOPLEFT, linkText);
		// Draw underline 1 px below baseline
		const auto & f = ENGINE->renderHandler().loadFont(FONT_SMALL);
		const int lineY = pos.y + (int)f->getLineHeight() - 1;
		to.drawLine(Point(pos.x, lineY), Point(pos.x + pos.w - 1, lineY), col, col);
	}

	void clickPressed(const Point &) override
	{
		if(onLink) onLink(target);
	}

	void hover(bool on) override
	{
		hovered = on;
		redraw();
	}
};

// WikiLinkOverlay – invisible click-area overlaid on a media widget to make it a link.
class WikiLinkOverlay : public CIntObject
{
	std::string target;
	std::function<void(const std::string &)> onLink;
public:
	WikiLinkOverlay(const Rect & area, std::string target_,
	                std::function<void(const std::string &)> cb)
		: CIntObject(LCLICK)
		, target(std::move(target_))
		, onLink(std::move(cb))
	{
		pos.x += area.x; pos.y += area.y; pos.w = area.w; pos.h = area.h;
	}
	void clickPressed(const Point &) override { if(onLink) onLink(target); }
};

// WikiBoldSpan – renders a text span in bold using TTF style change.
// Delegates to IFont::renderTextBold; falls back to plain for bitmap fonts.
class WikiBoldSpan : public CIntObject
{
	std::string text;
public:
	WikiBoldSpan(int x, int y, int w, int h, std::string t)
		: CIntObject(0, Point(x, y))
		, text(std::move(t))
	{ pos.w = w; pos.h = h; }

	void showAll(Canvas & to) override
	{
		to.drawTextBold(pos.topLeft(), FONT_SMALL, Colors::WHITE, text);
	}
};

// WikiItalicSpan – renders a text span in italic using TTF style change.
// Delegates to IFont::renderTextItalic; falls back to plain for bitmap fonts.
class WikiItalicSpan : public CIntObject
{
	std::string text;
public:
	WikiItalicSpan(int x, int y, int w, int h, std::string t)
		: CIntObject(0, Point(x, y))
		, text(std::move(t))
	{ pos.w = w; pos.h = h; }

	void showAll(Canvas & to) override
	{
		to.drawTextItalic(pos.topLeft(), FONT_SMALL, Colors::WHITE, text);
	}
};

// WikiCodeSpan – renders an inline code snippet with a semi-transparent background rect.
// bgW  = visual width of the background box (text without trailing space + padding).
// advW = total advance width used by the layout (may include trailing space).
class WikiCodeSpan : public CIntObject
{
	std::string text;
	int         bgW;
public:
	WikiCodeSpan(int x, int y, std::string t, int bgW_, int advW, int lineH)
		: CIntObject(0, Point(x, y))
		, text(std::move(t))
		, bgW(bgW_)
	{ pos.w = advW; pos.h = lineH; }

	void showAll(Canvas & to) override
	{
		// Shift background 1px down so it doesn't overlap the descenders of the line above.
		to.drawColorBlended(Rect(Point(pos.x, pos.y + 1), Point(bgW, pos.h - 1)), MD_CODE_BLOCK_BG);
		to.drawText(Point(pos.x + MD_CODE_PAD_X, pos.y + MD_CODE_PAD_Y),
		            FONT_SMALL, Colors::WHITE, ETextAlignment::TOPLEFT, text);
	}
};

// WikiCodeBlockWidget – fenced code block (``` ... ```) with a semi-transparent background.
class WikiCodeBlockWidget : public CIntObject
{
	std::vector<std::string> lines;
	int lineH;
public:
	WikiCodeBlockWidget(int x, int y, int w, std::vector<std::string> ls, int lh)
		: CIntObject(0, Point(x, y))
		, lines(std::move(ls))
		, lineH(lh)
	{
		pos.w = w;
		pos.h = MD_CODE_PAD_X * 2 + (int)lines.size() * lineH;  // same padding on all sides
	}

	void showAll(Canvas & to) override
	{
		to.drawColorBlended(Rect(pos.topLeft(), Point(pos.w, pos.h)), MD_CODE_BLOCK_BG);
		for(int i = 0; i < (int)lines.size(); ++i)
			to.drawText(Point(pos.x + MD_CODE_PAD_X, pos.y + MD_CODE_PAD_X + i * lineH),
			            FONT_SMALL, Colors::WHITE, ETextAlignment::TOPLEFT, lines[i]);
	}
};

static int parseHeading(const std::string & line, std::string & outText)
{
	int    level = 0;
	size_t i     = 0;
	while(i < line.size() && line[i] == '#') { ++level; ++i; }
	if(level == 0 || level > 3) return 0;
	if(i >= line.size() || line[i] != ' ') return 0;
	outText = line.substr(i + 1);
	while(!outText.empty() &&
	      (outText.back() == ' ' || outText.back() == '\t' || outText.back() == '\r'))
		outText.pop_back();
	return level;
}

static bool isHRule(const std::string & line)
{
	if(line.size() < 3) return false;
	const char c = line[0];
	if(c != '-' && c != '_' && c != '*') return false;
	int count = 0;
	for(char ch : line)
	{
		if(ch == c) ++count;
		else if(ch != ' ') return false;
	}
	return count >= 3;
}

static bool parseBullet(const std::string & line, std::string & outText)
{
	if(line.size() < 2) return false;
	if((line[0] == '-' || line[0] == '*') && line[1] == ' ')
	{
		outText = line.substr(2);
		return true;
	}
	return false;
}

static int parseOrdered(const std::string & line, std::string & outText)
{
	size_t i = 0;
	while(i < line.size() && std::isdigit(static_cast<unsigned char>(line[i]))) ++i;
	if(i == 0 || i + 1 >= line.size()) return -1;
	if(line[i] != '.' || line[i + 1] != ' ') return -1;
	const int num = std::stoi(line.substr(0, i));
	outText = line.substr(i + 2);
	return num;
}

struct MDAlignTag { MDAlign align; bool close = false; };

static std::optional<MDAlignTag> parseAlignTag(const std::string & line)
{
	if(line == "<center>")  return MDAlignTag{MDAlign::CENTER, false};
	if(line == "<left>")    return MDAlignTag{MDAlign::LEFT,   false};
	if(line == "<right>")   return MDAlignTag{MDAlign::RIGHT,  false};
	if(line == "</center>") return MDAlignTag{MDAlign::CENTER, true};
	if(line == "</left>")   return MDAlignTag{MDAlign::LEFT,   true};
	if(line == "</right>")  return MDAlignTag{MDAlign::RIGHT,  true};
	return std::nullopt;
}

// Parses an <a id="name" /> or <a name="name" /> HTML anchor tag anywhere in
// a string and returns the first captured name/id value (lower-cased).
// Returns an empty string if no tag is found.
static std::string parseAnchorTag(const std::string & s)
{
	static const std::regex RE(R"re(<a\s+(?:id|name)\s*=\s*"([^"]*?)"\s*/>)re",
	                            std::regex::icase);
	std::smatch m;
	if(std::regex_search(s, m, RE))
		return m[1].str();
	return {};
}

// Removes all <a id="..."/> / <a name="..."/> tags from a string.
static std::string stripAnchorTags(const std::string & s)
{
	static const std::regex RE(R"re(<a\s+(?:id|name)\s*=\s*"[^"]*?"\s*/>)re",
	                            std::regex::icase);
	return std::regex_replace(s, RE, "");
}

struct ParsedMedia
{
	std::string path;
	std::string alt;
	std::string linkTarget; ///< non-empty when media is wrapped in a wiki link: [![](path)](uri)
	int  frame    = 0;
	bool hasFrame = false; ///< true when path contained #N
	bool isAnim   = false; ///< .def / .json → AnimationPath
	bool isVideo  = false; ///< .bik / .smk / .webm / .mp4 → VideoPath
};

static bool parseMediaLine(const std::string & line, ParsedMedia & out)
{
	out.linkTarget.clear();
	std::string mediaPart = line;

	// Support linked media: [![alt](media)](uri)
	if(line.size() > 6 && line[0] == '[' && line[1] == '!')
	{
		// mediaPart is everything after the outer [
		const std::string inner = line.substr(1); // starts with ![alt](media)](uri)
		const auto cb = inner.find(']', 2);       // close bracket of alt text
		if(cb != std::string::npos && cb + 1 < inner.size() && inner[cb + 1] == '(')
		{
			const auto cp = inner.find(')', cb + 2); // close paren of media path
			if(cp != std::string::npos
				&& cp + 2 < inner.size()
				&& inner[cp + 1] == ']'
				&& inner[cp + 2] == '(')
			{
				const auto uriEnd = inner.find(')', cp + 3);
				if(uriEnd != std::string::npos && uriEnd + 1 == inner.size())
				{
					out.linkTarget = inner.substr(cp + 3, uriEnd - cp - 3);
					mediaPart = inner.substr(0, cp + 1); // just ![alt](media)
				}
			}
		}
	}
	if(mediaPart.size() < 5 || mediaPart[0] != '!' || mediaPart[1] != '[') return false;
	const auto closeBracket = mediaPart.find(']', 2);
	if(closeBracket == std::string::npos) return false;
	if(closeBracket + 1 >= mediaPart.size() || mediaPart[closeBracket + 1] != '(') return false;
	const auto closeParen = mediaPart.find(')', closeBracket + 2);
	if(closeParen == std::string::npos || closeParen + 1 != mediaPart.size()) return false;

	out.alt = mediaPart.substr(2, closeBracket - 2);
	std::string rawPath = mediaPart.substr(closeBracket + 2, closeParen - closeBracket - 2);
	if(rawPath.empty()) return false;

	// Separate optional frame suffix: path.def#N
	const auto hash = rawPath.rfind('#');
	if(hash != std::string::npos)
	{
		out.path     = rawPath.substr(0, hash);
		out.hasFrame = true;
		try { out.frame = std::stoi(rawPath.substr(hash + 1)); } catch(...) {}
	}
	else
	{
		out.path = rawPath;
	}

	// Classify by file extension (extension is stripped by resource system at load).
	const auto dot = out.path.rfind('.');
	if(dot != std::string::npos)
	{
		std::string ext = out.path.substr(dot + 1);
		for(auto & ch : ext) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
		out.isAnim  = (ext == "def" || ext == "json");
		out.isVideo = (ext == "bik" || ext == "smk" || ext == "webm" || ext == "mp4");
	}
	return true;
}

static bool isTableRow(const std::string & line)
{
	if(line.size() < 3) return false;
	size_t first = 0;
	while(first < line.size() && line[first] == ' ') ++first;
	if(first >= line.size() || line[first] != '|') return false;
	size_t last = line.size() - 1;
	while(last > first && line[last] == ' ') --last;
	return line[last] == '|';
}

static bool isTableSeparator(const std::string & row)
{
	for(char c : row)
		if(c != '-' && c != '|' && c != ':' && c != ' ') return false;
	return row.find('-') != std::string::npos;
}

static std::vector<std::string> splitTableRow(const std::string & row)
{
	std::string trimmed = row;
	while(!trimmed.empty() && trimmed.front() == ' ') trimmed.erase(trimmed.begin());
	while(!trimmed.empty() && trimmed.back()  == ' ') trimmed.pop_back();
	if(!trimmed.empty() && trimmed.front() == '|') trimmed.erase(trimmed.begin());
	if(!trimmed.empty() && trimmed.back()  == '|') trimmed.pop_back();

	std::vector<std::string> cells;
	std::istringstream ss(trimmed);
	std::string cell;
	while(std::getline(ss, cell, '|'))
	{
		while(!cell.empty() && cell.front() == ' ') cell.erase(cell.begin());
		while(!cell.empty() && cell.back()  == ' ') cell.pop_back();
		cells.push_back(cell);
	}
	return cells;
}

// Strips VCMI color tags ({text} and {color|text}) from a string, returning only
// the visible characters.  Used for pixel-width measurement of plain-text tokens
// that may contain color markup (e.g. "{highlighted} alongside").
static std::string stripColorTags(const std::string & s)
{
	std::string out;
	out.reserve(s.size());
	size_t i = 0;
	while(i < s.size())
	{
		if(s[i] == '{')
		{
			++i;
			const size_t close = s.find('}', i);
			if(close == std::string::npos) { out += '{'; continue; }
			// skip optional "color|" prefix
			const size_t pipe = s.find('|', i);
			const size_t textStart = (pipe != std::string::npos && pipe < close) ? pipe + 1 : i;
			out += s.substr(textStart, close - textStart);
			i = close + 1;
		}
		else
			out += s[i++];
	}
	return out;
}

static std::string replaceAll(std::string str, const std::string & from, const std::string & to)
{
	size_t pos = 0;
	while((pos = str.find(from, pos)) != std::string::npos)
	{
		str.replace(pos, from.size(), to);
		pos += to.size();
	}
	return str;
}

// ---------------------------------------------------------------------------
// Inline token types and tokenizer
// ---------------------------------------------------------------------------

enum class MDInlineType { PLAIN, LINK, BOLD, ITALIC, CODE };

struct MDInlineToken
{
	std::string   text;
	MDInlineType  type   = MDInlineType::PLAIN;
	std::string   target; ///< only for LINK
};

/// Tokenises a single line (no embedded newlines) into inline spans.
/// Handles: `` `code` ``, **bold**, *italic*, [text](uri).
/// All other characters are plain.
static std::vector<MDInlineToken> tokenizeInline(const std::string & line)
{
	std::vector<MDInlineToken> result;
	std::string plainAcc;

	auto flushPlain = [&]()
	{
		if(!plainAcc.empty())
		{
			result.push_back({std::move(plainAcc), MDInlineType::PLAIN, {}});
			plainAcc.clear();
		}
	};

	size_t i = 0;
	while(i < line.size())
	{
		// Inline code: `...`
		if(line[i] == '`')
		{
			const size_t end = line.find('`', i + 1);
			if(end == std::string::npos) { plainAcc += line[i++]; continue; }
			flushPlain();
			result.push_back({line.substr(i + 1, end - i - 1), MDInlineType::CODE, {}});
			i = end + 1;
			continue;
		}

		// Bold: **text**
		if(i + 1 < line.size() && line[i] == '*' && line[i + 1] == '*')
		{
			const size_t end = line.find("**", i + 2);
			if(end == std::string::npos) { plainAcc += line[i++]; continue; }
			flushPlain();
			result.push_back({line.substr(i + 2, end - i - 2), MDInlineType::BOLD, {}});
			i = end + 2;
			continue;
		}

		// Italic: *text*
		if(line[i] == '*')
		{
			const size_t end = line.find('*', i + 1);
			if(end == std::string::npos) { plainAcc += line[i++]; continue; }
			flushPlain();
			result.push_back({line.substr(i + 1, end - i - 1), MDInlineType::ITALIC, {}});
			i = end + 1;
			continue;
		}

		// Link: [text](uri)
		if(line[i] == '[')
		{
			const size_t cb = line.find(']', i + 1);
			if(cb == std::string::npos || cb + 1 >= line.size() || line[cb + 1] != '(')
			{
				plainAcc += line[i++];
				continue;
			}
			const size_t cp = line.find(')', cb + 2);
			if(cp == std::string::npos) { plainAcc += line[i++]; continue; }
			flushPlain();
			result.push_back({
				line.substr(i + 1, cb - i - 1),
				MDInlineType::LINK,
				line.substr(cb + 2, cp - cb - 2)
			});
			i = cp + 1;
			continue;
		}

		plainAcc += line[i++];
	}
	flushPlain();
	return result;
}

std::vector<std::shared_ptr<CIntObject>> buildMarkdownContent(
	CViewport & viewport,
	const std::string & markdownText,
	int viewportWidth,
	bool blueStyle,
	std::function<void(const std::string &)> onWikiLink,
	std::map<std::string, int> * anchors)
{
	std::vector<std::shared_ptr<CIntObject>> widgets;
	OBJECT_CONSTRUCTION_TARGETED(viewport.content());

	const int W     = viewportWidth;
	const int textW = W - MD_MARGIN * 2;
	int curY = MD_MARGIN;

	std::optional<MDAlign> pendingAlign;

	auto consumeAlign = [&](MDAlign defaultAlign) -> MDAlign
	{
		return pendingAlign.value_or(defaultAlign);
	};

	auto addParagraph = [&](const std::string & text, int x, int width, int gap,
	                         EFonts font = FONT_SMALL,
	                         ColorRGBA color = Colors::WHITE,
	                         ETextAlignment alignment = ETextAlignment::TOPLEFT)
	{
		if(text.empty()) return;
		auto lbl = std::make_shared<CMultiLineLabel>(
			Rect(x, curY, width, 4000), font, alignment, color, text);
		lbl->pos.h = lbl->textSize.y;
		curY += lbl->textSize.y + gap;
		widgets.push_back(std::move(lbl));
	};

	// Split into lines, strip trailing CR.
	std::vector<std::string> lines;
	{
		std::istringstream ss(markdownText);
		std::string ln;
		while(std::getline(ss, ln))
		{
			if(!ln.empty() && ln.back() == '\r') ln.pop_back();
			lines.push_back(ln);
		}
	}

	std::vector<std::string> paraBuffer;
	std::vector<std::string> tableBuffer;

	// Fenced code block state
	bool                     inCodeBlock = false;
	std::vector<std::string> codeBuffer;

	auto flushCodeBlock = [&]()
	{
		if(codeBuffer.empty()) return;
		const auto & font2 = ENGINE->renderHandler().loadFont(FONT_SMALL);
		const int lh = (int)font2->getLineHeight();
		auto block = std::make_shared<WikiCodeBlockWidget>(
			MD_MARGIN, curY, textW, std::move(codeBuffer), lh);
		curY += block->pos.h + MD_GAP;
		widgets.push_back(std::move(block));
		codeBuffer.clear();
	};

	// Blockquote accumulation buffer
	std::vector<std::string> bqBuffer;

	auto flushBlockquote = [&]()
	{
		if(bqBuffer.empty()) return;
		std::string joined;
		for(const auto & l : bqBuffer)
		{
			if(!joined.empty()) joined += '\n';
			joined += l;
		}
		bqBuffer.clear();

		const int bqW = textW - (MD_BQ_TXT - MD_MARGIN);
		auto lbl = std::make_shared<CMultiLineLabel>(
			Rect(MD_BQ_TXT, curY, bqW, 4000),
			FONT_SMALL, ETextAlignment::TOPLEFT, MD_BQ_TEXT_COLOR, joined);
		lbl->pos.h = lbl->textSize.y;
		const int barH = lbl->pos.h;
		auto bar = std::make_shared<GraphicalPrimitiveCanvas>(
			Rect(MD_MARGIN, curY, MD_BQ_BAR_W, barH));
		bar->addBox(Point(0, 0), Point(MD_BQ_BAR_W, barH), MD_BQ_BAR_COLOR);
		widgets.push_back(std::move(bar));
		widgets.push_back(std::move(lbl));
		curY += barH + MD_PARA_GAP;
	};

	auto flushPara = [&]()
	{
		if(paraBuffer.empty()) return;
		std::string joined;
		for(const auto & l : paraBuffer)
		{
			if(!joined.empty()) joined += ' ';
			joined += l;
		}
		paraBuffer.clear();
		joined = replaceAll(joined, "<br>", "\n");
		joined = replaceAll(joined, "<BR>", "\n");
		const MDAlign a = consumeAlign(MDAlign::LEFT);
		const ETextAlignment ta = toTextAlign(a);

		// Fast path: no inline markup characters → use word-wrapping CMultiLineLabel.
		// Also use fast path for non-LEFT alignment (inline layout only supports LEFT).
		const bool hasMarkup = (joined.find('*') != std::string::npos)
		                    || (joined.find('`') != std::string::npos)
		                    || (joined.find('[') != std::string::npos);
		if(!hasMarkup || a != MDAlign::LEFT)
		{
			addParagraph(joined, MD_MARGIN, textW, MD_PARA_GAP, FONT_SMALL, Colors::WHITE, ta);
			return;
		}

		// Inline layout: tokenise each <br>-split logical line and perform
		// word-wrap-aware per-token layout with proper widget types.
		const auto & fontPtr = ENGINE->renderHandler().loadFont(FONT_SMALL);
		const int    lineH   = (int)fontPtr->getLineHeight();

		std::vector<std::string> brLines;
		{
			std::istringstream ss(joined);
			std::string ln2;
			while(std::getline(ss, ln2)) brLines.push_back(ln2);
		}

		// Accumulate plain-only lines for CMultiLineLabel batching.
		std::string plainAcc;
		auto flushPlain = [&]()
		{
			if(plainAcc.empty()) return;
			auto lbl = std::make_shared<CMultiLineLabel>(
				Rect(MD_MARGIN, curY, textW, 4000), FONT_SMALL, ta, Colors::WHITE, plainAcc);
			lbl->pos.h = lbl->textSize.y;
			curY += lbl->textSize.y + MD_PARA_GAP;
			widgets.push_back(std::move(lbl));
			plainAcc.clear();
		};

		for(const std::string & ln : brLines)
		{
			const auto tokens = tokenizeInline(ln);

			// Check if this line actually has styled tokens.
			const bool lineHasStyle = std::any_of(tokens.begin(), tokens.end(),
				[](const MDInlineToken & t){ return t.type != MDInlineType::PLAIN; });

			if(!lineHasStyle)
			{
				// Batch into CMultiLineLabel.
				if(!plainAcc.empty()) plainAcc += '\n';
				plainAcc += ln;
				continue;
			}
			flushPlain();

			// Build word-level layout tokens:
			//   PLAIN/BOLD/ITALIC/CODE → split at spaces so word-wrap works.
			//   LINK                   → atomic (clickable area must not be split).
			// PLAIN with color tags: spaces inside {color|…} are also split points;
			//   the color prefix is re-opened for each continuation chunk.
			// CODE: each word-chunk keeps its own background box, but the box only
			//   covers the visible text (no trailing-space gap); advance width includes
			//   the space so the next token is positioned correctly.
			struct LayoutToken
			{
				std::string   text;    // rendered text
				MDInlineType  type;
				std::string   target;  // LINK only
				int           width;   // cursor advance (may include trailing space)
				int           bgWidth; // visual/bg width (CODE only: no trailing space)
			};
			std::vector<LayoutToken> ltoks;

			// Split PLAIN text at every space, re-wrapping active color-tag prefixes.
			// e.g. "{FF0000|hello world}" → "{FF0000|hello} ", "{FF0000|world}"
			auto splitPlainWithColorRewrap = [](const std::string & text) -> std::vector<std::string>
			{
				std::vector<std::string> result;
				std::string chunk;
				std::string colorPrefix; // active "{colorname|" prefix when inside a tag

				for(size_t i = 0; i < text.size(); ++i)
				{
					const char c = text[i];
					if(c == '{')
					{
						// Check for {color| pattern.
						const size_t pipe  = text.find('|', i + 1);
						const size_t close = text.find('}', i + 1);
						if(pipe != std::string::npos && pipe < close)
							colorPrefix = text.substr(i, pipe - i + 1); // "{color|"
						// else: plain brace block, no color prefix
						chunk += c;
					}
					else if(c == '}')
					{
						colorPrefix.clear();
						chunk += c;
					}
					else if(c == ' ')
					{
						if(!colorPrefix.empty())
						{
							// Close the tag before the split, reopen after.
							chunk += '}'; // close
							chunk += ' ';
							result.push_back(std::move(chunk));
							chunk = colorPrefix; // reopen: "{color|"
						}
						else
						{
							chunk += ' ';
							result.push_back(std::move(chunk));
							chunk.clear();
						}
					}
					else
						chunk += c;
				}
				if(!chunk.empty())
					result.push_back(std::move(chunk));
				return result;
			};

			for(const auto & tok : tokens)
			{
				if(tok.type == MDInlineType::LINK)
				{
					// Links stay atomic.
					const int tw = (int)fontPtr->getStringWidth(tok.text);
					ltoks.push_back({tok.text, tok.type, tok.target, tw, tw});
					continue;
				}

				if(tok.type == MDInlineType::PLAIN)
				{
					// Split respecting color-tag re-wrapping.
					for(const auto & word : splitPlainWithColorRewrap(tok.text))
					{
						const int tw = (int)fontPtr->getStringWidth(stripColorTags(word));
						ltoks.push_back({word, MDInlineType::PLAIN, {}, tw, tw});
					}
					continue;
				}

				// BOLD / ITALIC: split at spaces so word-wrap works.
				// CODE: stays atomic in ltoks; the render loop does per-line box accumulation.
				if(tok.type == MDInlineType::CODE)
				{
					// Store full content; width is only used as a hint (render loop overrides).
					const int tw = (int)fontPtr->getStringWidth(tok.text) + 2 * MD_CODE_PAD_X;
					ltoks.push_back({tok.text, MDInlineType::CODE, {}, tw, tw});
					continue;
				}
				size_t wp = 0;
				const std::string & src = tok.text;
				while(wp < src.size())
				{
					const size_t sp   = src.find(' ', wp);
					const bool   last = (sp == std::string::npos);
					const std::string word = last ? src.substr(wp) : src.substr(wp, sp - wp + 1);
					const int tw = (tok.type == MDInlineType::BOLD)
					              ? (int)fontPtr->getStringWidthBold(word)
					              : (int)fontPtr->getStringWidthItalic(word);
					ltoks.push_back({word, tok.type, {}, tw, tw});
					if(last) break;
					wp = sp + 1;
				}
			}

			// Word-wrap layout.
			int curX      = MD_MARGIN;
			const int maxX = MD_MARGIN + textW;

			for(const auto & lt : ltoks)
			{
				if(lt.text.empty()) continue;

				// CODE: accumulate words per line; each line gets exactly one background box.
				if(lt.type == MDInlineType::CODE)
				{
					// Split the full code text at spaces.
					std::vector<std::string> codeWords;
					{
						size_t p = 0;
						while(p <= lt.text.size())
						{
							const size_t e = lt.text.find(' ', p);
							if(e == std::string::npos) { codeWords.push_back(lt.text.substr(p)); break; }
							codeWords.push_back(lt.text.substr(p, e - p));
							p = e + 1;
						}
					}

					std::string seg; // words accumulated for the current line
					auto flushCodeSeg = [&]()
					{
						if(seg.empty()) return;
						const int bgW = (int)fontPtr->getStringWidth(seg) + 2 * MD_CODE_PAD_X;
						widgets.push_back(std::make_shared<WikiCodeSpan>(
							curX, curY, seg, bgW, bgW, lineH));
						curX += bgW;
						seg.clear();
					};

					for(const auto & word : codeWords)
					{
						if(word.empty()) continue;
						const std::string candidate = seg.empty() ? word : (seg + ' ' + word);
						const int candW = (int)fontPtr->getStringWidth(candidate) + 2 * MD_CODE_PAD_X;
						// If adding this word would overflow (and we already have content), flush.
						if(!seg.empty() && curX + candW > maxX)
						{
							flushCodeSeg();
							curY += lineH;
							curX = MD_MARGIN;
						}
						// If even the first word alone doesn't fit, wrap before starting.
						if(seg.empty() && curX > MD_MARGIN)
						{
							const int singleW = (int)fontPtr->getStringWidth(word) + 2 * MD_CODE_PAD_X;
							if(curX + singleW > maxX)
							{
								curY += lineH;
								curX = MD_MARGIN;
							}
						}
						seg = seg.empty() ? word : (seg + ' ' + word);
					}
					flushCodeSeg();
					continue; // skip standard wrap-check and curX advance below
				}

				// Wrap if token doesn't fit (but always allow the first token on a line).
				if(curX > MD_MARGIN && curX + lt.width > maxX)
				{
					curY += lineH;
					curX = MD_MARGIN;
				}
				switch(lt.type)
				{
				case MDInlineType::PLAIN:
				{
					// Strip trailing space from rendered text – the space width is already
					// accounted for in lt.width so cursor advancement is correct.
					const std::string rendered = (!lt.text.empty() && lt.text.back() == ' ')
					                             ? lt.text.substr(0, lt.text.size() - 1)
					                             : lt.text;
					if(!rendered.empty())
						widgets.push_back(std::make_shared<CLabel>(
							curX, curY, FONT_SMALL, ETextAlignment::TOPLEFT, Colors::WHITE, rendered));
					break;
				}
				case MDInlineType::LINK:
					if(onWikiLink)
						widgets.push_back(std::make_shared<WikiLinkLabel>(
							curX, curY, lt.width, lineH, lt.text, lt.target, onWikiLink));
					else
						widgets.push_back(std::make_shared<CLabel>(
							curX, curY, FONT_SMALL, ETextAlignment::TOPLEFT, Colors::WHITE, lt.text));
					break;
				case MDInlineType::BOLD:
				{
					const std::string rendered = (!lt.text.empty() && lt.text.back() == ' ')
					                             ? lt.text.substr(0, lt.text.size() - 1) : lt.text;
					if(!rendered.empty())
						widgets.push_back(std::make_shared<WikiBoldSpan>(
							curX, curY, lt.width, lineH, rendered));
					break;
				}
				case MDInlineType::ITALIC:
				{
					const std::string rendered = (!lt.text.empty() && lt.text.back() == ' ')
					                             ? lt.text.substr(0, lt.text.size() - 1) : lt.text;
					if(!rendered.empty())
						widgets.push_back(std::make_shared<WikiItalicSpan>(
							curX, curY, lt.width, lineH, rendered));
					break;
				}
				case MDInlineType::CODE:
					break; // handled above, never reached
				}
				curX += lt.width;
			}
			curY += lineH + MD_PARA_GAP;
		}
		flushPlain();
	};

	auto flushTable = [&]()
	{
		if(tableBuffer.empty()) return;

		// Filter out separator rows (|---|---| style).

		std::vector<std::string> rawRows;
		for(const auto & row : tableBuffer)
			if(!isTableSeparator(row))
				rawRows.push_back(row);
		tableBuffer.clear();
		if(rawRows.empty()) return;

		// Parse cells.
		std::vector<std::vector<std::string>> parsedRows;
		for(const auto & r : rawRows)
			parsedRows.push_back(splitTableRow(r));

		int numCols = 0;
		for(const auto & row : parsedRows)
			numCols = std::max(numCols, static_cast<int>(row.size()));
		if(numCols <= 0) return;

		const bool hasHeader   = parsedRows.size() >= 2;
		const int  dataStart   = hasHeader ? 1 : 0;
		const int  numDataRows = std::max(0, (int)parsedRows.size() - dataStart);
		// 1 px per inner column divider.
		const int  colW        = (textW - std::max(0, numCols - 1)) / numCols;

		static constexpr int CELL_PAD = 3;

		struct CellInfo
		{
			std::string text;
			bool        isMedia   = false;
			ParsedMedia media;
			int         measuredH = 16;
			MDAlign     textAlign = MDAlign::LEFT;
		};

		// Strip a leading alignment tag from cell text and return the align value.
		auto parseCellAlign = [](std::string & s) -> MDAlign
		{
			const struct { std::string_view open; std::string_view close; MDAlign align; } tags[] =
			{
				{ "<center>", "</center>", MDAlign::CENTER },
				{ "<right>",  "</right>",  MDAlign::RIGHT  },
				{ "<left>",   "</left>",   MDAlign::LEFT   },
			};
			for(const auto & tag : tags)
			{
				if(s.starts_with(tag.open))
				{
					s = s.substr(tag.open.size());
					if(s.ends_with(tag.close))
						s.resize(s.size() - tag.close.size());
					return tag.align;
				}
			}
			return MDAlign::LEFT;
		};

		std::vector<std::vector<CellInfo>> grid(parsedRows.size(),
		                                        std::vector<CellInfo>(numCols));

		for(int ri = 0; ri < (int)parsedRows.size(); ++ri)
		{
			for(int ci = 0; ci < numCols; ++ci)
			{
				const std::string & cellText = (ci < (int)parsedRows[ri].size())
					? parsedRows[ri][ci] : std::string{};
				CellInfo & info = grid[ri][ci];
				ParsedMedia pm;
				if(parseMediaLine(cellText, pm))
				{
					info.isMedia = true;
					info.media   = pm;
					const int cw = colW - CELL_PAD * 2;

					if(pm.isVideo)
					{
						info.measuredH = 60; // cannot probe video dimensions cheaply
					}
					else if(pm.isAnim)
					{
						const size_t fr = pm.hasFrame ? (size_t)pm.frame : 0;
						auto probe = ENGINE->renderHandler().loadImage(
							AnimationPath::builtin(pm.path), fr, 0, EImageBlitMode::COLORKEY);
						if(probe)
						{
							const Point sz = probe->dimensions();
							if(sz.x > 0)
							{
								const double sc = std::min(1.0, (double)cw / sz.x);
								info.measuredH = std::max(1, static_cast<int>(std::round(sz.y * sc)));
							}
						}
					}
					else
					{
						auto probe = ENGINE->renderHandler().loadImage(
							ImagePath::builtin(pm.path), EImageBlitMode::COLORKEY);
						if(probe)
						{
							const Point sz = probe->dimensions();
							if(sz.x > 0)
							{
								const double sc = std::min(1.0, (double)cw / sz.x);
								info.measuredH = std::max(1, static_cast<int>(std::round(sz.y * sc)));
							}
						}
					}
					info.measuredH += CELL_PAD * 2;
				}
				else
				{
					info.text = cellText;
					info.textAlign = parseCellAlign(info.text);
					const std::string labelText = info.text.empty() ? " " : info.text;
					auto probe = std::make_shared<CMultiLineLabel>(
						Rect(0, 0, colW - CELL_PAD * 2, 4000),
						FONT_SMALL, toTextAlign(info.textAlign), Colors::WHITE, labelText);
					info.measuredH = probe->textSize.y + CELL_PAD * 2;
				}
				info.measuredH = std::max(info.measuredH, 16);
			}
		}

		const int headerH = hasHeader ? 18 : 0;
		int dataRowH = 0;
		for(int ri = dataStart; ri < (int)parsedRows.size(); ++ri)
			for(int ci = 0; ci < numCols; ++ci)
				dataRowH = std::max(dataRowH, grid[ri][ci].measuredH);
		dataRowH = std::max(dataRowH, 16);

		// Draw the table grid (border + dividers).
		std::vector<int> colWidths(numCols, colW);
		widgets.push_back(wikiMakeTableGrid(
			MD_MARGIN, curY, textW, colWidths, headerH, dataRowH, numDataRows, blueStyle));

		// Header row labels (yellow, centred).
		if(hasHeader)
		{
			for(int ci = 0; ci < numCols; ++ci)
			{
				const int cx = MD_MARGIN + ci * (colW + 1);
				const CellInfo & info = grid[0][ci];
				if(!info.isMedia)
					widgets.push_back(std::make_shared<CLabel>(
						cx + colW / 2, curY + headerH / 2,
						FONT_SMALL, ETextAlignment::CENTER, Colors::YELLOW, info.text));
			}
		}

		// Data rows.
		for(int ri = dataStart; ri < (int)parsedRows.size(); ++ri)
		{
			const int ry = curY + headerH + (ri - dataStart) * dataRowH;
			for(int ci = 0; ci < numCols; ++ci)
			{
				const int cx  = MD_MARGIN + ci * (colW + 1);
				const int mx  = cx + CELL_PAD;
				const int my  = ry + CELL_PAD;
				const int cw  = colW - CELL_PAD * 2;
				const int ch  = dataRowH - CELL_PAD * 2;
				const CellInfo & info = grid[ri][ci];

				if(info.isMedia)
				{
					const auto & pm = info.media;
					if(pm.isVideo)
					{
						auto vid = std::make_shared<WikiVideoWidget>(
							Point(mx, my), VideoPath::builtin(pm.path), 1.0f);
						if(vid->pos.w > cw && vid->pos.w > 0)
						{
							const float sc = static_cast<float>(cw) / vid->pos.w;
							vid = std::make_shared<WikiVideoWidget>(
								Point(mx, my), VideoPath::builtin(pm.path), sc);
						}
						widgets.push_back(std::move(vid));
					}
					else if(pm.isAnim && !pm.hasFrame)
					{
						widgets.push_back(std::make_shared<WikiAnimLoopWidget>(
							AnimationPath::builtin(pm.path), mx, my));
					}
					else if(pm.isAnim)
					{
						auto probe = ENGINE->renderHandler().loadImage(
							AnimationPath::builtin(pm.path), (size_t)pm.frame, 0, EImageBlitMode::COLORKEY);
						if(probe)
						{
							const Point sz = probe->dimensions();
							int rW, rH;
							if(sz.x <= cw) { rW = sz.x; rH = sz.y; }
							else
							{
								const double sc = (double)cw / sz.x;
								rW = cw; rH = std::max(1, static_cast<int>(std::round(sz.y * sc)));
							}
							const int vOff = std::max(0, (ch - rH) / 2);
							widgets.push_back(std::make_shared<CAnimImage>(
								AnimationPath::builtin(pm.path), (size_t)pm.frame,
								Rect(mx, my + vOff, rW, rH), 0));
						}
					}
					else
					{
						auto pic = std::make_shared<WikiScaledImage>(
							ImagePath::builtin(pm.path), mx, my, cw);
						if(pic->height() > 0)
						{
							const int vOff = std::max(0, (ch - pic->height()) / 2);
							pic->moveBy(Point(0, vOff));
							widgets.push_back(std::move(pic));
						}
					}
				}
				else
				{
					widgets.push_back(std::make_shared<CMultiLineLabel>(
						Rect(mx, my, cw, dataRowH),
						FONT_SMALL, toTextAlign(info.textAlign), Colors::WHITE, info.text));
				}
			}
		}
		curY += headerH + numDataRows * dataRowH + MD_GAP;
	};

	for(const auto & rawLine : lines)
	{
		std::string t = rawLine;
		while(!t.empty() && (t.back() == ' ' || t.back() == '\t')) t.pop_back();

		// Fenced code block: lines starting with ``` open or close the block.
		if(t.size() >= 3 && t[0] == '`' && t[1] == '`' && t[2] == '`')
		{
			if(!inCodeBlock)
			{
				flushPara();
				flushBlockquote();
				inCodeBlock = true;
				codeBuffer.clear();
				// Optional language tag after the opening ``` is ignored (no highlighting).
			}
			else
			{
				inCodeBlock = false;
				flushCodeBlock();
			}
			continue;
		}
		if(inCodeBlock)
		{
			codeBuffer.push_back(t); // raw line – no further processing
			continue;
		}

		// Table row accumulation – collect consecutive | rows.
		if(isTableRow(t))
		{
			if(!paraBuffer.empty()) flushPara();
			flushBlockquote();
			tableBuffer.push_back(t);
			continue;
		}
		if(!tableBuffer.empty())
			flushTable();

		// Empty line → paragraph break.
		if(t.empty())
		{
			flushPara();
			flushBlockquote();
			curY += MD_GAP;
			continue;
		}

		// Alignment tags (<center>/<left>/<right> and matching closing tags).
		{
			auto a = parseAlignTag(t);
			if(a.has_value())
			{
				if(a->close)
					pendingAlign.reset();
				else
					pendingAlign = a->align;
				continue;
			}
		}

		// Invisible named anchor: <a id="name" /> or <a name="name" />
		// Records the current Y position in the anchors map and emits no widget.
		// Anchor tags may also appear inside heading lines (handled in the heading
		// branch below); this branch catches standalone anchor-only lines.
		{
			const std::string anchorId = parseAnchorTag(t);
			if(!anchorId.empty())
			{
				// Only treat as a standalone anchor if the whole line reduces to
				// nothing after stripping the tag (no other content).
				const std::string stripped = stripAnchorTags(t);
				std::string rest = stripped;
				while(!rest.empty() && (rest.front() == ' ' || rest.front() == '\t')) rest.erase(rest.begin());
				while(!rest.empty() && (rest.back()  == ' ' || rest.back()  == '\t')) rest.pop_back();
				if(rest.empty())
				{
					if(anchors) (*anchors)[anchorId] = curY;
					continue;
				}
				// Otherwise fall through – line has content beyond the anchor tag.
			}
		}

		// <p> – explicit paragraph break with gap.
		if(t == "<p>" || t == "<P>")
		{
			flushPara();
			flushBlockquote();
			curY += MD_GAP;
			continue;
		}

		// <br> on its own line – flush without gap.
		if(t == "<br>" || t == "<BR>")
		{
			flushPara();
			flushBlockquote();
			continue;
		}

		// Headings (# / ## / ###).
		{
			std::string headText;
			const int level = parseHeading(t, headText);
			if(level > 0)
			{
				flushPara();
				flushBlockquote();
				// Record any embedded anchor at the heading's current Y.
				if(anchors)
				{
					const std::string anchorId = parseAnchorTag(headText);
					if(!anchorId.empty())
						(*anchors)[anchorId] = curY;
				}
				// Strip anchor tags so they don't appear in the rendered text.
				headText = stripAnchorTags(headText);
				while(!headText.empty() && (headText.front() == ' ' || headText.front() == '\t')) headText.erase(headText.begin());
				while(!headText.empty() && (headText.back()  == ' ' || headText.back()  == '\t')) headText.pop_back();

				const EFonts fnt    = (level == 1) ? FONT_BIG : (level == 2) ? FONT_MEDIUM : FONT_SMALL;
				const int    topPad = (level == 1) ? MD_H1_PAD_TOP : (level == 2) ? MD_H2_PAD_TOP : MD_H3_PAD_TOP;
				const int    lineH  = (level == 1) ? 22 : (level == 2) ? 16 : 12;
				curY += topPad;

				const MDAlign a = consumeAlign(MDAlign::LEFT);
				int lx;
				ETextAlignment ta;
				if(a == MDAlign::CENTER)
				{
					lx = W / 2;
					ta = ETextAlignment::CENTER;
				}
				else if(a == MDAlign::RIGHT)
				{
					lx = W - MD_MARGIN;
					ta = ETextAlignment::TOPRIGHT;
				}
				else
				{
					lx = MD_MARGIN;
					ta = ETextAlignment::TOPLEFT;
				}

				widgets.push_back(std::make_shared<CLabel>(
					lx, curY, fnt, ta, Colors::YELLOW, headText));
				curY += lineH;

				if(level <= 2)
				{
					const ColorRGBA uCol = wikiBorderColor(blueStyle);
					auto ul = std::make_shared<GraphicalPrimitiveCanvas>(
						Rect(MD_MARGIN, curY + 2, textW, 1));
					ul->addLine(Point(0, 0), Point(textW, 0), uCol);
					widgets.push_back(std::move(ul));
					curY += 4;
				}
				curY += MD_PARA_GAP;
				continue;
			}
		}

		// Horizontal rule.
		if(isHRule(t))
		{
			flushPara();
			flushBlockquote();
			curY += MD_GAP / 2;
			auto rule = std::make_shared<GraphicalPrimitiveCanvas>(
				Rect(MD_MARGIN, curY, textW, 2));
			rule->addBox(Point(0, 0), Point(textW, 2), wikiBorderColor(blueStyle));
			widgets.push_back(std::move(rule));
			curY += 2 + MD_GAP / 2;
			continue;
		}

		// Media line: ![alt](path) – image / animation / video.
		{
			ParsedMedia pm;
			if(parseMediaLine(t, pm))
			{
				flushPara();
				flushBlockquote();
				if(pm.isVideo)
				{
					const MDAlign a = consumeAlign(MDAlign::LEFT);
					auto vid = std::make_shared<WikiVideoWidget>(
						Point(0, curY), VideoPath::builtin(pm.path), 1.0f);
					float scale = 1.0f;
					if(vid->pos.w > textW && vid->pos.w > 0)
						scale = static_cast<float>(textW) / vid->pos.w;
					if(scale < 1.0f)
						vid = std::make_shared<WikiVideoWidget>(
							Point(0, curY), VideoPath::builtin(pm.path), scale);
					const int vx = alignedX(a, vid->pos.w, MD_MARGIN, textW);
					const int vW = vid->pos.w;
					const int vH = vid->pos.h;
					const int vY = curY;
					vid->moveBy(Point(vx, 0));
					curY += vH + MD_GAP;
					widgets.push_back(std::move(vid));
					if(!pm.alt.empty())
						widgets.push_back(std::make_shared<WikiAltPopup>(Rect(vx, vY, vW, vH), pm.alt));
					if(!pm.linkTarget.empty() && onWikiLink)
						widgets.push_back(std::make_shared<WikiLinkOverlay>(Rect(vx, vY, vW, vH), pm.linkTarget, onWikiLink));
				}
				else if(pm.isAnim && !pm.hasFrame)
				{
					// No #N suffix → play all frames as a loop.
					const MDAlign a = consumeAlign(MDAlign::LEFT);
					auto anim = std::make_shared<WikiAnimLoopWidget>(
						AnimationPath::builtin(pm.path), 0, curY);
					const int ax = alignedX(a, anim->pos.w, MD_MARGIN, textW);
					const int aW = anim->pos.w;
					const int aH = anim->pos.h;
					const int aY = curY;
					anim->moveBy(Point(ax, 0));
					curY += aH + MD_GAP;
					widgets.push_back(std::move(anim));
					if(!pm.alt.empty())
						widgets.push_back(std::make_shared<WikiAltPopup>(Rect(ax, aY, aW, aH), pm.alt));
					if(!pm.linkTarget.empty() && onWikiLink)
						widgets.push_back(std::make_shared<WikiLinkOverlay>(Rect(ax, aY, aW, aH), pm.linkTarget, onWikiLink));
				}
				else if(pm.isAnim)
				{
					// #N suffix → single static frame.
					auto probe = ENGINE->renderHandler().loadImage(
						AnimationPath::builtin(pm.path), (size_t)pm.frame, 0, EImageBlitMode::COLORKEY);
					if(probe)
					{
						const Point nativeSz = probe->dimensions();
						if(nativeSz.x > 0 && nativeSz.y > 0)
						{
							int rW, rH;
							if(nativeSz.x <= textW) { rW = nativeSz.x; rH = nativeSz.y; }
							else
							{
								const double sc = (double)textW / nativeSz.x;
								rW = textW;
								rH = std::max(1, static_cast<int>(std::round(nativeSz.y * sc)));
							}
							const MDAlign a = consumeAlign(MDAlign::LEFT);
							const int     rx = alignedX(a, rW, MD_MARGIN, textW);
							const Rect frameRect(rx, curY, rW, rH);
							widgets.push_back(std::make_shared<CAnimImage>(
								AnimationPath::builtin(pm.path), (size_t)pm.frame,
								frameRect, 0));
							curY += rH + MD_GAP;
							if(!pm.alt.empty())
								widgets.push_back(std::make_shared<WikiAltPopup>(frameRect, pm.alt));
							if(!pm.linkTarget.empty() && onWikiLink)
								widgets.push_back(std::make_shared<WikiLinkOverlay>(frameRect, pm.linkTarget, onWikiLink));
						}
					}
				}
				else
				{
					// Plain static image.
					const MDAlign a = consumeAlign(MDAlign::LEFT);
					auto pic = std::make_shared<WikiScaledImage>(
						ImagePath::builtin(pm.path), 0, curY, textW);
					if(pic->height() > 0)
					{
						const int px = alignedX(a, pic->pos.w, MD_MARGIN, textW);
						const int pW = pic->pos.w;
						const int pH = pic->height();
						const int pY = curY;
						pic->moveBy(Point(px, 0));
						curY += pH + MD_GAP;
						widgets.push_back(std::move(pic));
						if(!pm.alt.empty())
							widgets.push_back(std::make_shared<WikiAltPopup>(Rect(px, pY, pW, pH), pm.alt));
						if(!pm.linkTarget.empty() && onWikiLink)
							widgets.push_back(std::make_shared<WikiLinkOverlay>(Rect(px, pY, pW, pH), pm.linkTarget, onWikiLink));
					}
				}
				continue;
			}
		}

		// Unordered bullet list item (- or *).
		{
			std::string bText;
			if(parseBullet(t, bText))
			{
				flushPara();
				flushBlockquote();
				auto dot = std::make_shared<GraphicalPrimitiveCanvas>(
					Rect(MD_BULLET_X, curY + 5, MD_BULLET_SZ, MD_BULLET_SZ));
				dot->addBox(Point(0, 0), Point(MD_BULLET_SZ, MD_BULLET_SZ), Colors::WHITE);
				widgets.push_back(std::move(dot));
				const int bW = textW - (MD_BULLET_TXT - MD_MARGIN);
				auto lbl = std::make_shared<CMultiLineLabel>(
					Rect(MD_BULLET_TXT, curY, bW, 4000),
					FONT_SMALL, ETextAlignment::TOPLEFT, Colors::WHITE, bText);
				lbl->pos.h = lbl->textSize.y;
				curY += lbl->textSize.y + MD_PARA_GAP;
				widgets.push_back(std::move(lbl));
				continue;
			}
		}

		// Ordered list item (N. text).
		{
			std::string oText;
			const int num = parseOrdered(t, oText);
			if(num >= 0)
			{
				flushPara();
				flushBlockquote();
				const std::string numStr = std::to_string(num) + ".";
				widgets.push_back(std::make_shared<CLabel>(
					MD_MARGIN + 2, curY,
					FONT_SMALL, ETextAlignment::TOPLEFT, Colors::WHITE, numStr));
				const int oW = textW - (MD_ORDERED_TXT - MD_MARGIN);
				auto lbl = std::make_shared<CMultiLineLabel>(
					Rect(MD_ORDERED_TXT, curY, oW, 4000),
					FONT_SMALL, ETextAlignment::TOPLEFT, Colors::WHITE, oText);
				lbl->pos.h = lbl->textSize.y;
				curY += lbl->textSize.y + MD_PARA_GAP;
				widgets.push_back(std::move(lbl));
				continue;
			}
		}

		// Blockquote line: starts with '>' optionally followed by a space.
		if(!t.empty() && t[0] == '>')
		{
			flushPara();
			const std::string bqText = (t.size() > 1 && t[1] == ' ') ? t.substr(2) : t.substr(1);
			bqBuffer.push_back(bqText);
			continue;
		}

		// Anything else: accumulate into paragraph buffer.
		// Inline <br> tags are expanded to newlines in flushPara().
		flushBlockquote();
		paraBuffer.push_back(t);
	}

	flushPara();
	flushTable();
	flushBlockquote();
	if(inCodeBlock) flushCodeBlock(); // unclosed ``` fence
	curY += MD_MARGIN;

	return widgets;
}

