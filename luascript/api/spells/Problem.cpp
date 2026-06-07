/*
 * Problem.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"

#include "Problem.h"

#include "../Enums.h"
#include "../LuaMetaString.h"
#include "../../../lib/spells/ISpellMechanics.h"
#include "../../../lib/texts/MetaString.h"

// Proxy header brought in for its luaTypeNameOf ADL overload.
#include "Mechanics.h"

VCMI_LIB_NAMESPACE_BEGIN

namespace scripting::api
{
using ::spells::Problem;
using ::spells::Mechanics;

void ProblemProxy::addCustom(Problem & problem, const LuaMetaString & config)
{
	problem.add(config.toMetaString());
}

void ProblemProxy::addGeneric(Problem & problem, const Mechanics & mechanics)
{
	mechanics.adaptGenericProblem(problem);
}

void ProblemProxy::addStandard(Problem & problem, const Mechanics & mechanics, ESpellCastProblem spellProblem)
{
	mechanics.adaptProblem(spellProblem, problem);
}

void ProblemProxy::registerMethods(MethodRegistrar & R)
{
	R.function<&ProblemProxy::addCustom>("addCustom",
		"Adds a custom-message problem entry built from the given MetaString config.");
	R.function<&ProblemProxy::addGeneric>("addGeneric",
		"Adds the generic 'cannot cast' problem entry derived from the given mechanics.");
	R.function<&ProblemProxy::addStandard>("addStandard",
		"Adds a standard problem entry with the requested ESpellCastProblem value.");
}

}

VCMI_LIB_NAMESPACE_END
