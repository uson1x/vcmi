/*
 * api/Services.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include <vcmi/Services.h>

#include <vcmi/ArtifactService.h>
#include <vcmi/CreatureService.h>
#include <vcmi/FactionService.h>
#include <vcmi/HeroClassService.h>
#include <vcmi/HeroTypeService.h>
#include <vcmi/SkillService.h>
#include <vcmi/spells/Service.h>

#include "../LuaWrapper.h"

VCMI_LIB_NAMESPACE_BEGIN

namespace scripting
{
namespace api
{

class ServicesProxy : public RawPointerWrapper<const Services, ServicesProxy>
{
public:
	using Wrapper = RawPointerWrapper<const Services, ServicesProxy>;
	static const std::vector<typename Wrapper::CustomRegType> REGISTER_CUSTOM;
};

class ArtifactServiceProxy : public RawPointerWrapper<const ArtifactService, ArtifactServiceProxy>
{
public:
	using Wrapper = RawPointerWrapper<const ArtifactService, ArtifactServiceProxy>;
	static const std::vector<typename Wrapper::CustomRegType> REGISTER_CUSTOM;
};

class CreatureServiceProxy : public RawPointerWrapper<const CreatureService, CreatureServiceProxy>
{
public:
	using Wrapper = RawPointerWrapper<const CreatureService, CreatureServiceProxy>;
	static const std::vector<typename Wrapper::CustomRegType> REGISTER_CUSTOM;
};

class FactionServiceProxy : public RawPointerWrapper<const FactionService, FactionServiceProxy>
{
public:
	using Wrapper = RawPointerWrapper<const FactionService, FactionServiceProxy>;
	static const std::vector<typename Wrapper::CustomRegType> REGISTER_CUSTOM;
};

class HeroClassServiceProxy : public RawPointerWrapper<const HeroClassService, HeroClassServiceProxy>
{
public:
	using Wrapper = RawPointerWrapper<const HeroClassService, HeroClassServiceProxy>;
	static const std::vector<typename Wrapper::CustomRegType> REGISTER_CUSTOM;
};

class HeroTypeServiceProxy : public RawPointerWrapper<const HeroTypeService, HeroTypeServiceProxy>
{
public:
	using Wrapper = RawPointerWrapper<const HeroTypeService, HeroTypeServiceProxy>;
	static const std::vector<typename Wrapper::CustomRegType> REGISTER_CUSTOM;
};

class SkillServiceProxy : public RawPointerWrapper<const SkillService, SkillServiceProxy>
{
public:
	using Wrapper = RawPointerWrapper<const SkillService, SkillServiceProxy>;
	static const std::vector<typename Wrapper::CustomRegType> REGISTER_CUSTOM;
};

class SpellServiceProxy : public RawPointerWrapper<const spells::Service, SpellServiceProxy>
{
public:
	using Wrapper = RawPointerWrapper<const spells::Service, SpellServiceProxy>;
	static const std::vector<typename Wrapper::CustomRegType> REGISTER_CUSTOM;
};

}
}



VCMI_LIB_NAMESPACE_END
