/*
 * DocExport.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "StdInc.h"

#include "DocExport.h"

#include "DocRegistrar.h"
#include "FieldDocRegistrar.h"
#include "Registry.h"

#include <boost/filesystem/operations.hpp>

#include <fstream>

VCMI_LIB_NAMESPACE_BEGIN

namespace scripting::api
{

namespace
{

/// Escape pipe characters and newlines so a value safely fits in a Markdown table cell.
std::string escapeMarkdownCell(const std::string & s)
{
	std::string out;
	out.reserve(s.size());
	for(char c : s)
	{
		if(c == '|')
			out += "\\|";
		else if(c == '\n')
			out += ' ';
		else
			out += c;
	}
	return out;
}

/// Renders the host-side metadata for a single type as a Markdown section.
void emitMarkdownType(std::ofstream & out,
                      const std::string & typeName,
                      std::string_view description,
                      const std::vector<DocRegistrar::Entry> & methods,
                      const std::vector<FieldDocRegistrar::Entry> & fields)
{
	out << "## " << typeName << "\n\n";

	if(!description.empty())
		out << description << "\n\n";

	if(!fields.empty())
	{
		out << "**Fields**\n\n";
		out << "| Field | Type | Description |\n";
		out << "|---|---|---|\n";
		for(const auto & entry : fields)
		{
			out << "| `" << escapeMarkdownCell(entry.name)
				<< "` | `" << escapeMarkdownCell(entry.type)
				<< "` | " << escapeMarkdownCell(entry.description)
				<< " |\n";
		}
		out << "\n";
	}

	if(methods.empty() && fields.empty())
	{
		out << "_No bindings exposed._\n\n";
		return;
	}

	if(methods.empty())
		return;

	out << "**Methods**\n\n";
	out << "| Method | Signature | Description |\n";
	out << "|---|---|---|\n";

	for(const auto & entry : methods)
	{
		out << "| `" << escapeMarkdownCell(entry.name)
			<< "` | `" << escapeMarkdownCell(entry.signature)
			<< "` | " << escapeMarkdownCell(entry.description)
			<< " |\n";
	}
	out << "\n";
}

/// Splits a signature string like "(arg1: T, arg2: U): R" into args text, return text.
/// Falls back to passing the raw signature into the Lua-side function body when parsing fails.
struct SignatureParts
{
	std::string args;
	std::string returnType;
	bool        ok = false;
};

SignatureParts parseSignature(const std::string & signature)
{
	SignatureParts result;
	if(signature.empty() || signature.front() != '(')
		return result;

	// Find the matching closing paren — supports nested parens (e.g. fun() callback types).
	int depth = 0;
	std::size_t closeIdx = std::string::npos;
	for(std::size_t i = 0; i < signature.size(); ++i)
	{
		if(signature[i] == '(')
			++depth;
		else if(signature[i] == ')')
		{
			--depth;
			if(depth == 0)
			{
				closeIdx = i;
				break;
			}
		}
	}
	if(closeIdx == std::string::npos)
		return result;

	result.args = signature.substr(1, closeIdx - 1);

	// Anything after "): " is the return type.
	const std::size_t after = closeIdx + 1;
	if(after < signature.size())
	{
		std::size_t retStart = after;
		while(retStart < signature.size() && (signature[retStart] == ':' || signature[retStart] == ' '))
			++retStart;
		if(retStart < signature.size())
			result.returnType = signature.substr(retStart);
	}

	result.ok = true;
	return result;
}

/// Splits "arg1: T, arg2: U" into a list of (name, type) pairs.
/// When entries are bare types (e.g. auto-derived signatures that did not name args), the name
/// is left empty and the emitter falls back to "arg1, arg2, ...".
std::vector<std::pair<std::string, std::string>> splitArgs(const std::string & argsText)
{
	std::vector<std::pair<std::string, std::string>> result;

	std::string current;
	int depth = 0;
	const auto flush = [&]() {
		if(current.empty())
			return;
		// trim
		auto l = current.find_first_not_of(" \t");
		auto r = current.find_last_not_of(" \t");
		std::string token = (l == std::string::npos) ? std::string{} : current.substr(l, r - l + 1);
		if(token.empty())
		{
			current.clear();
			return;
		}

		const auto colon = token.find(':');
		if(colon == std::string::npos)
		{
			result.push_back({"", token});
		}
		else
		{
			std::string name = token.substr(0, colon);
			std::string type = token.substr(colon + 1);
			// trim both halves
			const auto trim = [](std::string & s) {
				auto a = s.find_first_not_of(" \t");
				auto b = s.find_last_not_of(" \t");
				if(a == std::string::npos)
					s.clear();
				else
					s = s.substr(a, b - a + 1);
			};
			trim(name);
			trim(type);
			result.push_back({name, type});
		}
		current.clear();
	};

	for(char c : argsText)
	{
		if(c == '(' || c == '{' || c == '<')
			++depth;
		else if(c == ')' || c == '}' || c == '>')
			--depth;

		if(c == ',' && depth == 0)
			flush();
		else
			current += c;
	}
	flush();

	return result;
}

/// Picks `argN` when the signature didn't name an argument.
std::string nameOrFallback(const std::string & rawName, std::size_t index)
{
	if(!rawName.empty())
		return rawName;
	return "arg" + std::to_string(index + 1);
}

/// Emits a single function declaration as Lua Language Server annotations.
void emitLualsMethod(std::ofstream & out, const std::string & className, const DocRegistrar::Entry & entry)
{
	const auto parts = parseSignature(entry.signature);

	// Description (markdown is preserved by luals in hover popups).
	if(!entry.description.empty())
		out << "---" << entry.description << "\n";

	std::vector<std::pair<std::string, std::string>> args;
	if(parts.ok && !parts.args.empty())
		args = splitArgs(parts.args);

	for(std::size_t i = 0; i < args.size(); ++i)
	{
		const auto name = nameOrFallback(args[i].first, i);
		out << "---@param " << name << ' ' << args[i].second << "\n";
	}

	if(parts.ok && !parts.returnType.empty())
		out << "---@return " << parts.returnType << "\n";

	out << "function " << className << ":" << entry.name << "(";
	for(std::size_t i = 0; i < args.size(); ++i)
	{
		if(i)
			out << ", ";
		out << nameOrFallback(args[i].first, i);
	}
	out << ") end\n\n";
}

/// Wraps a description across multiple `---` lines so luals' hover popup renders
/// each line as a separate paragraph instead of one long row.
void emitLualsDescription(std::ofstream & out, std::string_view description)
{
	if(description.empty())
		return;

	std::size_t start = 0;
	while(start < description.size())
	{
		const auto end = description.find('\n', start);
		const auto len = (end == std::string_view::npos) ? description.size() - start : end - start;
		out << "---" << description.substr(start, len) << "\n";
		if(end == std::string_view::npos)
			break;
		start = end + 1;
	}
}

void emitLualsType(std::ofstream & out,
                   const std::string & typeName,
                   std::string_view description,
                   const std::vector<DocRegistrar::Entry> & methods,
                   const std::vector<FieldDocRegistrar::Entry> & fields)
{
	emitLualsDescription(out, description);
	out << "---@class " << typeName << "\n";

	for(const auto & entry : fields)
	{
		out << "---@field " << entry.name << ' ' << entry.type;
		if(!entry.description.empty())
			out << " # " << entry.description;
		out << "\n";
	}

	out << "local " << typeName << " = {}\n\n";

	for(const auto & entry : methods)
		emitLualsMethod(out, typeName, entry);
}

}

void exportLuaApiDocs(const boost::filesystem::path & outDir)
{
	boost::filesystem::create_directories(outDir);

	const auto mdPath  = outDir / "API.md";
	const auto luaPath = outDir / "api.lua";

	std::ofstream md(mdPath.string());
	std::ofstream lua(luaPath.string());

	md  << "# Lua scripting API reference\n\n"
	    << "Auto-generated from C++ host bindings via `vcmiserver --export-lua-docs`. Do not edit by hand.\n\n";

	lua << "---@meta\n"
	    << "-- Auto-generated Lua Language Server definitions for the VCMI scripting API.\n"
	    << "-- Generated by `vcmiserver --export-lua-docs`. Do not edit by hand.\n\n";

	for(const auto & [typeName, registar] : Registry::get()->getAllTypes())
	{
		DocRegistrar methodSink;
		registar->collectDocs(methodSink);

		FieldDocRegistrar fieldSink;
		registar->collectFields(fieldSink);

		const auto description = registar->getDescription();

		emitMarkdownType(md, typeName, description, methodSink.get(), fieldSink.get());
		emitLualsType(lua, typeName, description, methodSink.get(), fieldSink.get());
	}
}

}

VCMI_LIB_NAMESPACE_END
