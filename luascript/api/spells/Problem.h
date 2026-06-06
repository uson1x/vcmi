/*
 * Mechanics.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include "../../LuaWrapper.h"
#include "../MethodRegistrar.h"
#include "../../../lib/spells/Problem.h"
#include "../../../lib/constants/Enumerations.h"

VCMI_LIB_NAMESPACE_BEGIN

namespace spells { class Mechanics; }

namespace scripting::api
{
		struct LuaMetaString;

		class ProblemProxy : public RawPointerWrapper<::spells::Problem, ProblemProxy>
		{
		public:
			static constexpr std::string_view luaName = "SpellProblem";

			using Wrapper = RawPointerWrapper<::spells::Problem, ProblemProxy>;
			static void registerMethods(MethodRegistrar & R);

			static void addCustom(::spells::Problem & problem, const LuaMetaString & config);
			static void addGeneric(::spells::Problem & problem, const ::spells::Mechanics & mechanics);
			static void addStandard(::spells::Problem & problem, const ::spells::Mechanics & mechanics, ESpellCastProblem spellProblem);
		};

		inline std::string luaTypeNameOf(LuaTypeNameTag<::spells::Problem>)
		{
			return std::string(ProblemProxy::luaName);
		}
}

VCMI_LIB_NAMESPACE_END
