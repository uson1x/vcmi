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

std::vector<std::shared_ptr<CIntObject>> buildMarkdownContent(
	CViewport & viewport,
	const std::string & markdownText,
	int viewportWidth,
	bool blueStyle,
	std::function<void(const std::string &)> onWikiLink)
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

		// Detect inline wiki-links: [text](uri)
		static const std::regex LINK_RE(R"(\[([^\]\r\n]*)\]\(([^)\r\n]*)\))");
		if(!onWikiLink || !std::regex_search(joined, LINK_RE))
		{
			addParagraph(joined, MD_MARGIN, textW, MD_PARA_GAP, FONT_SMALL, Colors::WHITE, ta);
			return;
		}

		// Has inline links: process line by line, splitting each line at link positions.
		const auto & fontPtr = ENGINE->renderHandler().loadFont(FONT_SMALL);
		const int lineH = (int)fontPtr->getLineHeight();

		std::vector<std::string> splitLines;
		{
			std::istringstream ss(joined);
			std::string ln2;
			while(std::getline(ss, ln2)) splitLines.push_back(ln2);
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

		for(const std::string & ln : splitLines)
		{
			std::smatch m2;
			if(!std::regex_search(ln, m2, LINK_RE))
			{
				if(!plainAcc.empty()) plainAcc += '\n';
				plainAcc += ln;
				continue;
			}
			flushPlain();

			// Word-wrap-aware inline segment layout.
			// Tokenise: split line at link boundaries, further split plain spans at spaces.
			struct InlineToken { std::string text; bool isLink = false; std::string target; };
			std::vector<InlineToken> tokens;
			{
				size_t pos2 = 0;
				for(std::sregex_iterator rit(ln.begin(), ln.end(), LINK_RE), rend2; rit != rend2; ++rit)
				{
					const auto & m = *rit;
					const size_t mstart = static_cast<size_t>(m.position());
					// Plain text before this link – split at space boundaries.
					if(mstart > pos2)
					{
						const std::string plain = ln.substr(pos2, mstart - pos2);
						size_t wp = 0;
						while(wp < plain.size())
						{
							const size_t sp = plain.find(' ', wp);
							if(sp == std::string::npos)
							{
								tokens.push_back({plain.substr(wp), false, ""});
								break;
							}
							tokens.push_back({plain.substr(wp, sp - wp + 1), false, ""}); // word + space
							wp = sp + 1;
						}
					}
					// Link token.
					tokens.push_back({m[1].str(), true, m[2].str()});
					pos2 = mstart + m.length();
				}
				// Trailing plain text.
				if(pos2 < ln.size())
				{
					const std::string plain = ln.substr(pos2);
					size_t wp = 0;
					while(wp < plain.size())
					{
						const size_t sp = plain.find(' ', wp);
						if(sp == std::string::npos)
						{
							tokens.push_back({plain.substr(wp), false, ""});
							break;
						}
						tokens.push_back({plain.substr(wp, sp - wp + 1), false, ""});
						wp = sp + 1;
					}
				}
			}

			// Lay out tokens with word wrapping.
			int curX = MD_MARGIN;
			const int maxX = MD_MARGIN + textW;
			for(const auto & tok : tokens)
			{
				if(tok.text.empty()) continue;
				const int tw = (int)fontPtr->getStringWidth(tok.text);
				// Wrap if token doesn't fit (allow at least one token per row).
				if(curX > MD_MARGIN && curX + tw > maxX)
				{
					curY += lineH;
					curX = MD_MARGIN;
				}
				if(tok.isLink)
				{
					widgets.push_back(std::make_shared<WikiLinkLabel>(
						curX, curY, tw, lineH, tok.text, tok.target, onWikiLink));
				}
				else
				{
					auto seg = std::make_shared<CLabel>(
						curX, curY, FONT_SMALL, ETextAlignment::TOPLEFT, Colors::WHITE, tok.text);
					widgets.push_back(std::move(seg));
				}
				curX += tw;
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

		// Table row accumulation – collect consecutive | rows.
		if(isTableRow(t))
		{
			if(!paraBuffer.empty()) flushPara();
			tableBuffer.push_back(t);
			continue;
		}
		if(!tableBuffer.empty())
			flushTable();

		// Empty line → paragraph break.
		if(t.empty())
		{
			flushPara();
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

		// <p> – explicit paragraph break with gap.
		if(t == "<p>" || t == "<P>")
		{
			flushPara();
			curY += MD_GAP;
			continue;
		}

		// <br> on its own line – flush without gap.
		if(t == "<br>" || t == "<BR>")
		{
			flushPara();
			continue;
		}

		// Headings (# / ## / ###).
		{
			std::string headText;
			const int level = parseHeading(t, headText);
			if(level > 0)
			{
				flushPara();
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

		// Anything else: accumulate into paragraph buffer.
		// Inline <br> tags are expanded to newlines in flushPara().
		paraBuffer.push_back(t);
	}

	flushPara();
	flushTable();
	curY += MD_MARGIN;

	return widgets;
}

