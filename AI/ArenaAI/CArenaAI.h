#pragma once

#include "../../lib/callback/CAdventureAI.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

class JsonNode;
class CGTownInstance;
class CGHeroInstance;
class CGObjectInstance;

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

	// T1 persistent MOVE_TO intent: when the model aims a hero at a still-distant
	// unowned object, we remember the destination (per hero id) and keep advancing
	// toward it at the start of each turn instead of re-asking the model every step
	// (which made the hero oscillate locally and never reach distant mines/towns).
	std::map<int, int3> heroTravelGoals;

	// The teleport exit the CURRENT move's path intends to come out of. Written by
	// executeHeroMoveTo before it sends a move whose path transits a teleporter;
	// consumed by showTeleportDialog to answer the exit-choice query with the
	// pathfinder's exit instead of a blind first-option pick (which sent heroes out
	// of the wrong monolith of a multi-exit channel and made them ping-pong between
	// map pockets forever). Guarded by teleportExitMx: the dialog arrives on the
	// network thread while the turn thread is still inside the move request.
	std::mutex teleportExitMx;
	int3 plannedTeleportExit = int3(-1, -1, -1);

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
	void showGarrisonDialog(const CArmedInstance * up, const CGHeroInstance * down, bool removableUnits, QueryID queryID, const MetaString & customTitle) override;
	void showMapObjectSelectDialog(QueryID askID, const Component & icon, const MetaString & title, const MetaString & description, const std::vector<ObjectInstanceID> & objects) override;
	std::optional<BattleAction> makeSurrenderRetreatDecision(const BattleID & battleID, const BattleStateInfoForRetreat & battleState) override;

private:
	void runTurn(QueryID queryID);
	void waitForBattles();
	void answerQuery(QueryID queryID, int choice);

	// Execute the engine-pathed move toward `destination` for this turn; returns
	// true if the hero reached that tile (so a visit/capture/battle fired there).
	// With stopBeforeCombat, the move halts BEFORE stepping into a wandering
	// monster's zone of control (no battle), setting *blockedByGuard if it did so.
	bool executeHeroMoveTo(const CGHeroInstance * hero, const int3 & destination,
		bool stopBeforeCombat = false, bool * blockedByGuard = nullptr);
	// Continue any standing per-hero travel goals one turn's worth before asking
	// the model, so multi-turn "go take that object" intents actually complete.
	void advanceTravelGoals();
	// Pull each town's garrison into its visiting hero at turn start, so recruited
	// reinforcements join the field army instead of piling up idle (the model
	// recruits remotely but often never returns to collect). Kill-switch:
	// ARENA_DISABLE_AUTO_COLLECT=1.
	void autoCollectGarrisons();
	// The first unowned (not ours) visitable object on a tile, or nullptr.
	const CGObjectInstance * unownedObjectAt(const int3 & pos) const;

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
