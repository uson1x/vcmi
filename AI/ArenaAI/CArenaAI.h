#pragma once

#include "../../lib/callback/CAdventureAI.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

class JsonNode;
class CGTownInstance;

class CArenaAI : public CAdventureAI
{
	std::shared_ptr<CCallback> cb;
	std::string matchId = "vcmi_live_match";
	std::string socketPath;
	std::string sideName = "red";
	std::string battleAiName = "BattleAI";
	int decisionTimeoutMs = 8000;
	int maxActionsPerTurn = 8;
	int turnTimeBudgetMs = 64000;
	std::atomic<uint64_t> seqCounter{1};
	bool bridgeEnabled = false;

	// Battle-resolution handshake. The turn loop runs on a worker thread so the
	// network thread stays free to deliver battle packs and PackageApplied acks;
	// waitTillRealize then makes adventure requests block until the server has
	// fully applied them (a move that triggers a battle blocks until the inherited
	// CAdventureAI -> BattleAI delegation auto-resolves it).
	std::thread turnThread;
	std::mutex battleMx;
	std::condition_variable battleCv;
	int battlesInProgress = 0;
	std::atomic<bool> aborting{false};

public:
	~CArenaAI() override;
	void initGameInterface(std::shared_ptr<Environment> env, std::shared_ptr<CCallback> callback) override;
	std::string getBattleAIName() const override;
	void yourTurn(QueryID queryID) override;

	void battleStart(const BattleID & battleID, const CCreatureSet * army1, const CCreatureSet * army2, int3 tile, const CGHeroInstance * hero1, const CGHeroInstance * hero2, BattleSide side, bool replayAllowed) override;
	void battleEnd(const BattleID & battleID, const BattleResult * br, QueryID queryID) override;
	void gameOver(PlayerColor player, const EVictoryLossCheckResult & victoryLossCheckResult) override;

	void heroGotLevel(const CGHeroInstance * hero, PrimarySkill pskill, std::vector<SecondarySkill> & skills, QueryID queryID) override;
	void commanderGotLevel(const CCommanderInstance * commander, std::vector<ui32> skills, QueryID queryID) override;
	void showBlockingDialog(const std::string & text, const std::vector<Component> & components, QueryID askID, const int soundID, bool selection, bool cancel, bool safeToAutoaccept) override;
	void showTeleportDialog(const CGHeroInstance * hero, TeleportChannelID channel, TTeleportExitsList exits, bool impassable, QueryID askID) override;
	void showGarrisonDialog(const CArmedInstance * up, const CGHeroInstance * down, bool removableUnits, QueryID queryID) override;
	void showMapObjectSelectDialog(QueryID askID, const Component & icon, const MetaString & title, const MetaString & description, const std::vector<ObjectInstanceID> & objects) override;
	std::optional<BattleAction> makeSurrenderRetreatDecision(const BattleID & battleID, const BattleStateInfoForRetreat & battleState) override;

private:
	void runTurn(QueryID queryID);
	void waitForBattles();
	void answerQuery(QueryID queryID, int choice);

	JsonNode buildTurnRequestPayload(QueryID queryID, int actionIndex, int turnRemainingMs) const;
	bool sendEnvelope(const std::string & msgType, const JsonNode & payload, JsonNode & responseOut);
	bool applyTurnResponse(const JsonNode & responsePayload);
	std::string selectedActionType(const JsonNode & responsePayload) const;
	int chooseSelectionViaBridge(QueryID queryID, const std::string & queryType, const std::vector<int> & options, int defaultSelection);

	const CGHeroInstance * findHeroById(int heroId) const;
	const CGTownInstance * findTownById(int townId) const;
	std::optional<int> parseOptionValue(const std::string & token) const;
	std::optional<int> parseTaggedOptionValue(const std::string & token, const std::string & marker) const;
	std::optional<int> jsonInt(const JsonNode & node) const;
};
