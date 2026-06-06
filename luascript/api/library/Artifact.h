/*
 * api/Artifact.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include <vcmi/Artifact.h>

#include "../../LuaWrapper.h"
#include "../MethodRegistrar.h"

VCMI_LIB_NAMESPACE_BEGIN

namespace scripting::api
{

class ArtifactProxy : public RawPointerWrapper<const Artifact, ArtifactProxy>
{
public:
	static constexpr std::string_view luaName = "Artifact";

	using Wrapper = RawPointerWrapper<const Artifact, ArtifactProxy>;
	static void registerMethods(MethodRegistrar & R);
};

inline std::string luaTypeNameOf(LuaTypeNameTag<Artifact>)
{
	return std::string(ArtifactProxy::luaName);
}

}

VCMI_LIB_NAMESPACE_END
