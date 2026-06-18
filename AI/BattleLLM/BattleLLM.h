#pragma once

#include "../../lib/battle/BattleAction.h"
#include "../../lib/battle/BattleHex.h"
#include "../../lib/callback/CBattleGameInterface.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

class JsonNode;
class CStack;
class CGHeroInstance;
class CCreatureSet;
namespace battle
{
class Unit;
}

class CBattleLLM : public CBattleGameInterface
{
	struct CandidateAction
	{
		int actionId = 0;
		std::string type;
		std::string optionId;
		std::string targetId;
		std::string approach;
		int x = -1;
		int y = -1;
		BattleAction action;
	};

	BattleSide side;
	std::shared_ptr<CBattleCallback> cb;
	std::shared_ptr<Environment> env;

	bool wasWaitingForRealize;
	bool bridgeEnabled;
	std::string socketPath;
	std::string matchId;
	int decisionTimeoutMs;
	uint64_t seqCounter;
	int roundIndex;
	int turnIndex;

	void print(const std::string & text) const;
	std::string sideName() const;
	std::string unitToken(const battle::Unit * unit) const;
	std::string unitDisplayName(const battle::Unit * unit) const;
	std::string approachToken(const BattleHex & targetHex, const BattleHex & attackFrom) const;

	std::vector<CandidateAction> buildCandidates(const BattleID & battleID, const CStack * stack) const;
	BattleAction chooseFallbackAction(const std::vector<CandidateAction> & candidates, const CStack * stack) const;
	JsonNode buildTurnRequestPayload(const BattleID & battleID, const CStack * stack, const std::vector<CandidateAction> & candidates) const;
	void appendUnitSnapshot(JsonNode & outArray, const CStack * stack) const;

	bool sendEnvelope(const std::string & msgType, const JsonNode & payload, JsonNode & responseOut);
	std::optional<int> parseSelectedActionId(const JsonNode & responseEnvelope) const;
	std::optional<int> jsonInt(const JsonNode & node) const;

public:
	CBattleLLM();
	~CBattleLLM();

	void initBattleInterface(std::shared_ptr<Environment> env_, std::shared_ptr<CBattleCallback> cb_) override;
	void initBattleInterface(std::shared_ptr<Environment> env_, std::shared_ptr<CBattleCallback> cb_, AutocombatPreferences autocombatPreferences) override;

	void actionFinished(const BattleID & battleID, const BattleAction & action) override;
	void actionStarted(const BattleID & battleID, const BattleAction & action) override;
	void activeStack(const BattleID & battleID, const CStack * stack) override;
	void yourTacticPhase(const BattleID & battleID, int distance) override;

	void battleAttack(const BattleID & battleID, const BattleAttack * ba) override;
	void battleStacksAttacked(const BattleID & battleID, const std::vector<BattleStackAttacked> & bsa, bool ranged) override;
	void battleEnd(const BattleID & battleID, const BattleResult * br, QueryID queryID) override;
	void battleNewRoundFirst(const BattleID & battleID) override;
	void battleNewRound(const BattleID & battleID) override;
	void battleStackMoved(const BattleID & battleID, const CStack * stack, const BattleHexArray & dest, int distance, bool teleport) override;
	void battleSpellCast(const BattleID & battleID, const BattleSpellCast * sc) override;
	void battleStacksEffectsSet(const BattleID & battleID, const SetStackEffect & sse) override;
	void battleStart(const BattleID & battleID, const CCreatureSet * army1, const CCreatureSet * army2, int3 tile, const CGHeroInstance * hero1, const CGHeroInstance * hero2, BattleSide side_, bool replayAllowed) override;
	void battleCatapultAttacked(const BattleID & battleID, const CatapultAttack & ca) override;
};
