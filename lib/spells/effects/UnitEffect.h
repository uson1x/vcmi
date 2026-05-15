/*
 * UnitEffect.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include "Effect.h"

VCMI_LIB_NAMESPACE_BEGIN

namespace spells
{
namespace effects
{

class UnitEffect : public Effect
{
public:
	void adjustTargetTypes(std::vector<TargetType> & types) const override;
	void adjustAffectedHexes(BattleHexArray & hexes, const Mechanics * m, const Target & spellTarget) const final;

	bool applicableGeneral(Problem & problem, const Mechanics * m) const override;
	bool applicableTarget(Problem & problem, const Mechanics * m, const EffectTarget & target) const override;
	bool applicableUnit(const Mechanics * m, const battle::Unit * s, bool alwaysSmart, bool alwaysReceptive) const;

	EffectTarget filterTarget(const Mechanics * m, const EffectTarget & target) const final;
	EffectTarget transformTarget(const Mechanics * m, const Target & aimPoint, const Target & spellTarget) const override;

protected:
	int32_t chainLength = 0;
	double chainFactor = 0.0;

	virtual bool isReceptive(const Mechanics * m, const battle::Unit * unit) const;
	virtual bool isValidTarget(const Mechanics * m, const battle::Unit * unit) const;

	void serializeJsonEffect(JsonSerializeFormat & handler) final;
	virtual void serializeJsonUnitEffect(JsonSerializeFormat & handler) = 0;

private:
	bool ignoreImmunity = false;

	EffectTarget transformTargetByRange(const Mechanics * m, const Target & aimPoint, const Target & spellTarget) const;
	EffectTarget transformTargetByChain(const Mechanics * m, const Target & aimPoint, const Target & spellTarget) const;
};

}
}

VCMI_LIB_NAMESPACE_END
