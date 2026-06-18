#include "StdInc.h"
#include "BattleLLM.h"

#include "../../lib/CStack.h"
#include "../../lib/CCreatureHandler.h"
#include "../../lib/battle/BattleAction.h"
#include "../../lib/battle/BattleInfo.h"
#include "../../lib/battle/CPlayerBattleCallback.h"
#include "../../lib/callback/CBattleCallback.h"
#include "../../lib/CRandomGenerator.h"
#include "../../lib/json/JsonNode.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace
{
const char * BRIDGE_SCHEMA_VERSION = "vcmi_arena_bridge.v1";

constexpr int MAX_UNITS_PER_SIDE = 18;
constexpr int MAX_MOVE_OPTIONS = 24;
constexpr int MAX_SHOOT_OPTIONS = 16;
constexpr int MAX_MELEE_OPTIONS = 24;

int clampTimeoutMs(int timeout)
{
	if(timeout < 100)
		return 100;
	if(timeout > 120000)
		return 120000;
	return timeout;
}

std::string nowUtcSeconds()
{
	return std::to_string(static_cast<long long>(std::time(nullptr)));
}

std::optional<int> parseEnvInt(const char * key)
{
	if(const char * raw = std::getenv(key))
	{
		try
		{
			return std::stoi(raw);
		}
		catch(...)
		{
			return std::nullopt;
		}
	}
	return std::nullopt;
}

int actionPriority(const std::string & type)
{
	if(type == "SHOOT")
		return 0;
	if(type == "MELEE")
		return 1;
	if(type == "MOVE")
		return 2;
	if(type == "WAIT")
		return 3;
	if(type == "RETREAT")
		return 4;
	return 99;
}

} // namespace

CBattleLLM::CBattleLLM()
	: side(BattleSide::NONE)
	, wasWaitingForRealize(false)
	, bridgeEnabled(false)
	, matchId("battle_live_match")
	, decisionTimeoutMs(12000)
	, seqCounter(1)
	, roundIndex(0)
	, turnIndex(0)
{
	print("created");
}

CBattleLLM::~CBattleLLM()
{
	print("destroyed");
	if(cb)
		cb->waitTillRealize = wasWaitingForRealize;
}

void CBattleLLM::initBattleInterface(std::shared_ptr<Environment> env_, std::shared_ptr<CBattleCallback> cb_)
{
	env = env_;
	cb = cb_;

	if(cb)
	{
		wasWaitingForRealize = cb->waitTillRealize;
		cb->waitTillRealize = false;
	}

	if(const char * value = std::getenv("BATTLE_LLM_MATCH_ID"))
	{
		if(std::strlen(value) > 0)
			matchId = value;
	}
	if(const char * value = std::getenv("BATTLE_LLM_BRIDGE_SOCKET"))
		socketPath = value;
	if(const auto timeoutOverride = parseEnvInt("BATTLE_LLM_DECISION_TIMEOUT_MS"))
		decisionTimeoutMs = clampTimeoutMs(*timeoutOverride);

	bridgeEnabled = !socketPath.empty();
}

void CBattleLLM::initBattleInterface(std::shared_ptr<Environment> env_, std::shared_ptr<CBattleCallback> cb_, AutocombatPreferences autocombatPreferences)
{
	(void)autocombatPreferences;
	initBattleInterface(env_, cb_);
}

std::string CBattleLLM::sideName() const
{
	if(side == BattleSide::ATTACKER)
		return "attacker";
	if(side == BattleSide::DEFENDER)
		return "defender";
	return "unknown";
}

std::string CBattleLLM::unitToken(const battle::Unit * unit) const
{
	if(unit == nullptr)
		return "u_unknown";
	return "u_" + std::to_string(unit->unitId());
}

std::string CBattleLLM::unitDisplayName(const battle::Unit * unit) const
{
	if(unit == nullptr)
		return "Unknown";
	if(const auto * stack = dynamic_cast<const CStack *>(unit))
		return stack->getName();
	return "Unit " + std::to_string(unit->unitId());
}

std::string CBattleLLM::approachToken(const BattleHex & targetHex, const BattleHex & attackFrom) const
{
	switch(BattleHex::mutualPosition(targetHex, attackFrom))
	{
	case BattleHex::TOP_LEFT:
		return "TL";
	case BattleHex::TOP_RIGHT:
		return "TR";
	case BattleHex::LEFT:
		return "L";
	case BattleHex::RIGHT:
		return "R";
	case BattleHex::BOTTOM_LEFT:
		return "BL";
	case BattleHex::BOTTOM_RIGHT:
		return "BR";
	default:
		return "TL";
	}
}

void CBattleLLM::appendUnitSnapshot(JsonNode & outArray, const CStack * stack) const
{
	if(stack == nullptr)
		return;

	JsonNode item;
	item["id"].String() = unitToken(stack);
	item["name"].String() = stack->getName();
	item["hex"]["x"].Integer() = stack->getPosition().getX();
	item["hex"]["y"].Integer() = stack->getPosition().getY();
	item["count"].Integer() = stack->getCount();
	item["is_ranged"].Bool() = stack->isShooter();
	item["can_retaliate"].Bool() = stack->ableToRetaliate();
	item["attack"].Integer() = stack->getAttack(false);
	item["defense"].Integer() = stack->getDefense(false);
	item["damage_min"].Integer() = stack->getMinDamage(false);
	item["damage_max"].Integer() = stack->getMaxDamage(false);
	item["hp"].Integer() = static_cast<si64>(stack->getMaxHealth());
	item["hp_left"].Integer() = stack->getFirstHPleft();
	item["speed"].Integer() = static_cast<si64>(stack->getMovementRange());
	item["shots"].Integer() = stack->isShooter() ? stack->shots.available() : 0;
	outArray.Vector().push_back(item);
}

std::vector<CBattleLLM::CandidateAction> CBattleLLM::buildCandidates(const BattleID & battleID, const CStack * stack) const
{
	std::vector<CandidateAction> result;
	if(!cb || !stack)
		return result;

	auto battleCb = cb->getBattle(battleID);
	if(!battleCb)
		return result;

	int nextActionId = 0;
	auto addAction = [&](CandidateAction action)
	{
		action.actionId = nextActionId++;
		result.push_back(action);
	};

	{
		CandidateAction waitAction;
		waitAction.type = "WAIT";
		waitAction.action = stack->waited() ? BattleAction::makeDefend(stack) : BattleAction::makeWait(stack);
		addAction(waitAction);
	}

	if(battleCb->battleCanFlee())
	{
		CandidateAction retreatAction;
		retreatAction.type = "RETREAT";
		retreatAction.action = BattleAction::makeRetreat(side);
		addAction(retreatAction);
	}

	if(stack->creatureId() != CreatureID::CATAPULT && !stack->hasBonusOfType(BonusType::SIEGE_WEAPON))
	{
		BattleHexArray availableHexes = battleCb->battleGetAvailableHexes(stack, false);
		int moveCount = 0;
		for(const auto & hex : availableHexes)
		{
			if(moveCount >= MAX_MOVE_OPTIONS)
				break;
			if(hex == stack->getPosition())
				continue;

			CandidateAction moveAction;
			moveAction.type = "MOVE";
			moveAction.optionId = "m_" + std::to_string(hex.getX()) + "_" + std::to_string(hex.getY());
			moveAction.x = hex.getX();
			moveAction.y = hex.getY();
			moveAction.action = BattleAction::makeMove(stack, hex);
			addAction(moveAction);
			moveCount++;
		}

		auto enemyStacks = battleCb->battleGetStacks(CBattleInfoEssentials::ONLY_ENEMY);
		std::sort(enemyStacks.begin(), enemyStacks.end(), [](const CStack * lhs, const CStack * rhs)
		{
			if(lhs == nullptr || rhs == nullptr)
				return lhs != nullptr;
			return lhs->unitId() < rhs->unitId();
		});

		int shootCount = 0;
		for(const auto * enemy : enemyStacks)
		{
			if(enemy == nullptr)
				continue;
			if(shootCount >= MAX_SHOOT_OPTIONS)
				break;
			if(!battleCb->battleCanShoot(stack, enemy->getPosition()))
				continue;

			CandidateAction shootAction;
			shootAction.type = "SHOOT";
			shootAction.targetId = unitToken(enemy);
			shootAction.optionId = "s_" + shootAction.targetId;
			shootAction.action = BattleAction::makeShotAttack(stack, enemy);
			addAction(shootAction);
			shootCount++;
		}

		int meleeCount = 0;
		for(const auto * enemy : enemyStacks)
		{
			if(enemy == nullptr)
				continue;
			if(meleeCount >= MAX_MELEE_OPTIONS)
				break;

			for(const auto & hex : availableHexes)
			{
				if(meleeCount >= MAX_MELEE_OPTIONS)
					break;
				if(!CStack::isMeleeAttackPossible(stack, enemy, hex))
					continue;

				CandidateAction meleeAction;
				meleeAction.type = "MELEE";
				meleeAction.targetId = unitToken(enemy);
				meleeAction.approach = approachToken(enemy->getPosition(), hex);
				meleeAction.optionId = "me_" + meleeAction.targetId + "_h_" + std::to_string(hex.toInt());
				meleeAction.action = BattleAction::makeMeleeAttack(stack, enemy->getPosition(), hex);
				addAction(meleeAction);
				meleeCount++;
			}
		}
	}

	return result;
}

BattleAction CBattleLLM::chooseFallbackAction(const std::vector<CandidateAction> & candidates, const CStack * stack) const
{
	if(candidates.empty())
		return BattleAction::makeDefend(stack);

	const CandidateAction * best = nullptr;
	for(const auto & candidate : candidates)
	{
		if(best == nullptr)
		{
			best = &candidate;
			continue;
		}
		const int lhsPriority = actionPriority(candidate.type);
		const int rhsPriority = actionPriority(best->type);
		if(lhsPriority < rhsPriority || (lhsPriority == rhsPriority && candidate.actionId < best->actionId))
			best = &candidate;
	}

	if(best != nullptr)
		return best->action;
	return BattleAction::makeDefend(stack);
}

JsonNode CBattleLLM::buildTurnRequestPayload(const BattleID & battleID, const CStack * stack, const std::vector<CandidateAction> & candidates) const
{
	JsonNode payload;
	payload["schema_version"].String() = "legal_action_contract.v1";
	payload["arena_spec_version"].String() = "combat-native-v1.0.0";
	payload["engine_ruleset"].String() = "vcmi-native-battle";
	payload["adapter_version"].String() = "battle-llm-plugin-0.1.0";
	payload["turn_id"].String() =
		matchId + "-b" + std::to_string(battleID.getNum()) + "-r" + std::to_string(roundIndex) + "-t" + std::to_string(turnIndex);
	payload["match_id"].String() = matchId;
	payload["game_id"].String() = "battle_" + std::to_string(battleID.getNum());
	payload["turn_index"].Integer() = turnIndex;
	payload["round_index"].Integer() = roundIndex;
	payload["acting_side"].String() = sideName();
	payload["is_attacker"].Bool() = (side == BattleSide::ATTACKER);
	payload["approach_frame"].String() = "vcmi_hex_neighbors_v1";
	payload["legal_actions_hash"].String() =
		matchId + ":" + std::to_string(battleID.getNum()) + ":" + std::to_string(roundIndex) + ":" + std::to_string(turnIndex);
	payload["legal_actions_hash_spec"].String() = "battle_live_placeholder.v1";
	payload["output_schema_ref"].String() = "llm_decision.v1.json";

	payload["active_unit"]["id"].String() = unitToken(stack);
	payload["active_unit"]["name"].String() = unitDisplayName(stack);
	payload["active_unit"]["hex"]["x"].Integer() = stack ? stack->getPosition().getX() : -1;
	payload["active_unit"]["hex"]["y"].Integer() = stack ? stack->getPosition().getY() : -1;

	payload["decision_budget"]["timeout_ms"].Integer() = decisionTimeoutMs;
	payload["decision_budget"]["max_output_tokens"].Integer() = 256;
	payload["decision_budget"]["max_actions_per_turn"].Integer() = 1;

	auto battleCb = cb ? cb->getBattle(battleID) : nullptr;

	JsonNode friendlyStacks;
	friendlyStacks.setType(JsonNode::JsonType::DATA_VECTOR);
	JsonNode enemyStacks;
	enemyStacks.setType(JsonNode::JsonType::DATA_VECTOR);

	if(battleCb)
	{
		auto allStacks = battleCb->battleGetStacks(CBattleInfoEssentials::MINE_AND_ENEMY);
		std::sort(allStacks.begin(), allStacks.end(), [](const CStack * lhs, const CStack * rhs)
		{
			if(lhs == nullptr || rhs == nullptr)
				return lhs != nullptr;
			return lhs->unitId() < rhs->unitId();
		});

		for(const auto * unit : allStacks)
		{
			if(unit == nullptr)
				continue;
			if(unit->unitSide() == side)
			{
				if(static_cast<int>(friendlyStacks.Vector().size()) < MAX_UNITS_PER_SIDE)
					appendUnitSnapshot(friendlyStacks, unit);
			}
			else
			{
				if(static_cast<int>(enemyStacks.Vector().size()) < MAX_UNITS_PER_SIDE)
					appendUnitSnapshot(enemyStacks, unit);
			}
		}
	}

	payload["battlefield_summary"]["summary_version"].String() = "battlefield_summary.v1";
	payload["battlefield_summary"]["round"].Integer() = roundIndex;
	payload["battlefield_summary"]["friendly_stacks"] = friendlyStacks;
	payload["battlefield_summary"]["enemy_stacks"] = enemyStacks;
	payload["battlefield_summary"]["obstacles"].setType(JsonNode::JsonType::DATA_VECTOR);
	payload["battlefield_summary"]["notes"].setType(JsonNode::JsonType::DATA_VECTOR);
	payload["battlefield_summary"]["notes"].Vector().push_back(JsonNode("Use legal_actions as sole legality source."));

	JsonNode legalActions;
	legalActions.setType(JsonNode::JsonType::DATA_VECTOR);

	JsonNode actionIndex;
	actionIndex.setType(JsonNode::JsonType::DATA_VECTOR);

	JsonNode actionIds;
	actionIds.setType(JsonNode::JsonType::DATA_VECTOR);

	std::vector<const CandidateAction *> moveOptions;
	std::vector<const CandidateAction *> shootOptions;
	std::vector<const CandidateAction *> meleeOptions;

	for(const auto & candidate : candidates)
	{
		JsonNode row;
		row["action_id"].Integer() = candidate.actionId;
		row["type"].String() = candidate.type;
		if(!candidate.optionId.empty())
			row["option_id"].String() = candidate.optionId;
		if(candidate.x >= 0)
			row["x"].Integer() = candidate.x;
		if(candidate.y >= 0)
			row["y"].Integer() = candidate.y;
		if(!candidate.targetId.empty())
			row["target_id"].String() = candidate.targetId;
		if(!candidate.approach.empty())
			row["approach"].String() = candidate.approach;
		actionIndex.Vector().push_back(row);
		actionIds.Vector().push_back(JsonNode(static_cast<si64>(candidate.actionId)));

		if(candidate.type == "WAIT")
		{
			JsonNode waitGroup;
			waitGroup["type"].String() = "WAIT";
			waitGroup["action_id"].Integer() = candidate.actionId;
			legalActions.Vector().push_back(waitGroup);
			continue;
		}
		if(candidate.type == "RETREAT")
		{
			JsonNode retreatGroup;
			retreatGroup["type"].String() = "RETREAT";
			retreatGroup["action_id"].Integer() = candidate.actionId;
			legalActions.Vector().push_back(retreatGroup);
			continue;
		}
		if(candidate.type == "MOVE")
			moveOptions.push_back(&candidate);
		else if(candidate.type == "SHOOT")
			shootOptions.push_back(&candidate);
		else if(candidate.type == "MELEE")
			meleeOptions.push_back(&candidate);
	}

	auto appendOptionsGroup = [&](const std::string & typeName, const std::vector<const CandidateAction *> & options)
	{
		if(options.empty())
			return;
		JsonNode group;
		group["type"].String() = typeName;
		JsonNode groupOptions;
		groupOptions.setType(JsonNode::JsonType::DATA_VECTOR);
		for(const auto * option : options)
		{
			JsonNode item;
			item["option_id"].String() = option->optionId;
			item["action_id"].Integer() = option->actionId;
			if(option->x >= 0)
				item["x"].Integer() = option->x;
			if(option->y >= 0)
				item["y"].Integer() = option->y;
			if(!option->targetId.empty())
				item["target_id"].String() = option->targetId;
			if(!option->approach.empty())
			{
				JsonNode approaches;
				approaches.setType(JsonNode::JsonType::DATA_VECTOR);
				approaches.Vector().push_back(JsonNode(option->approach));
				item["approaches"] = approaches;
				item["approach"].String() = option->approach;
			}
			groupOptions.Vector().push_back(item);
		}
		group["options"] = groupOptions;
		legalActions.Vector().push_back(group);
	};

	appendOptionsGroup("MOVE", moveOptions);
	appendOptionsGroup("SHOOT", shootOptions);
	appendOptionsGroup("MELEE", meleeOptions);

	payload["legal_actions"] = legalActions;
	payload["legal_action_index"] = actionIndex;
	payload["legal_action_ids"] = actionIds;
	return payload;
}

bool CBattleLLM::sendEnvelope(const std::string & msgType, const JsonNode & payload, JsonNode & responseOut)
{
	if(!bridgeEnabled || socketPath.empty())
		return false;
	if(socketPath.size() >= sizeof(sockaddr_un::sun_path))
		return false;

	JsonNode envelope;
	envelope["schema_version"].String() = BRIDGE_SCHEMA_VERSION;
	envelope["match_id"].String() = matchId;
	envelope["side"].String() = sideName();
	envelope["seq"].Integer() = static_cast<si64>(seqCounter++);
	envelope["msg_type"].String() = msgType;
	envelope["ts_utc"].String() = nowUtcSeconds();
	envelope["payload"] = payload;

	const std::string requestLine = envelope.toCompactString() + "\n";

	int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
	if(fd < 0)
		return false;

	timeval tv{};
	tv.tv_sec = decisionTimeoutMs / 1000;
	tv.tv_usec = (decisionTimeoutMs % 1000) * 1000;
	::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

	sockaddr_un address{};
	address.sun_family = AF_UNIX;
	std::snprintf(address.sun_path, sizeof(address.sun_path), "%s", socketPath.c_str());
	if(::connect(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) < 0)
	{
		::close(fd);
		return false;
	}

	const char * dataPtr = requestLine.c_str();
	size_t remaining = requestLine.size();
	while(remaining > 0)
	{
		const ssize_t sent = ::send(fd, dataPtr, remaining, 0);
		if(sent <= 0)
		{
			::close(fd);
			return false;
		}
		dataPtr += sent;
		remaining -= static_cast<size_t>(sent);
	}

	std::string responseLine;
	char buffer[4096];
	for(;;)
	{
		const ssize_t got = ::recv(fd, buffer, sizeof(buffer), 0);
		if(got <= 0)
			break;
		responseLine.append(buffer, buffer + got);
		if(responseLine.find('\n') != std::string::npos)
			break;
	}
	::close(fd);

	if(responseLine.empty())
		return false;
	if(const auto newlinePos = responseLine.find('\n'); newlinePos != std::string::npos)
		responseLine.resize(newlinePos);

	try
	{
		JsonParsingSettings parserSettings;
		parserSettings.strict = false;
		JsonNode parsed(responseLine.c_str(), responseLine.size(), parserSettings, "battle_llm_bridge_response");
		if(!parsed.isStruct())
			return false;
		if(parsed["msg_type"].isString() && parsed["msg_type"].String() == "error")
			return false;
		responseOut = parsed;
		return true;
	}
	catch(...)
	{
		return false;
	}
}

std::optional<int> CBattleLLM::jsonInt(const JsonNode & node) const
{
	if(node.getType() == JsonNode::JsonType::DATA_INTEGER)
		return static_cast<int>(node.Integer());
	if(node.getType() == JsonNode::JsonType::DATA_FLOAT)
		return static_cast<int>(node.Float());
	return std::nullopt;
}

std::optional<int> CBattleLLM::parseSelectedActionId(const JsonNode & responseEnvelope) const
{
	if(!responseEnvelope.isStruct())
		return std::nullopt;

	const JsonNode & payload = responseEnvelope["payload"];
	if(!payload.isStruct())
		return std::nullopt;

	if(const auto selectedId = jsonInt(payload["selected_action_id"]))
		return selectedId;

	const JsonNode & selectedAction = payload["selected_action"];
	if(selectedAction.isStruct())
	{
		if(const auto selectedId = jsonInt(selectedAction["action_id"]))
			return selectedId;
	}

	const JsonNode & resolved = payload["resolved_decision"];
	if(resolved.isStruct())
	{
		if(const auto selectedId = jsonInt(resolved["resolved_action_id"]))
			return selectedId;
	}

	return std::nullopt;
}

void CBattleLLM::activeStack(const BattleID & battleID, const CStack * stack)
{
	if(!cb || !stack)
		return;

	turnIndex += 1;
	const auto candidates = buildCandidates(battleID, stack);
	const BattleAction fallback = chooseFallbackAction(candidates, stack);

	if(candidates.empty() || !bridgeEnabled)
	{
		cb->battleMakeUnitAction(battleID, fallback);
		return;
	}

	JsonNode responseEnvelope;
	if(sendEnvelope("turn_request", buildTurnRequestPayload(battleID, stack, candidates), responseEnvelope))
	{
		if(const auto selectedActionId = parseSelectedActionId(responseEnvelope))
		{
			for(const auto & candidate : candidates)
			{
				if(candidate.actionId == *selectedActionId)
				{
					cb->battleMakeUnitAction(battleID, candidate.action);
					return;
				}
			}
		}
	}

	cb->battleMakeUnitAction(battleID, fallback);
}

void CBattleLLM::yourTacticPhase(const BattleID & battleID, int distance)
{
	(void)distance;
	if(cb)
		cb->battleMakeTacticAction(battleID, BattleAction::makeEndOFTacticPhase(cb->getBattle(battleID)->battleGetTacticsSide()));
}

void CBattleLLM::actionFinished(const BattleID & battleID, const BattleAction & action)
{
	(void)battleID;
	(void)action;
}

void CBattleLLM::actionStarted(const BattleID & battleID, const BattleAction & action)
{
	(void)battleID;
	(void)action;
}

void CBattleLLM::battleAttack(const BattleID & battleID, const BattleAttack * ba)
{
	(void)battleID;
	(void)ba;
}

void CBattleLLM::battleStacksAttacked(const BattleID & battleID, const std::vector<BattleStackAttacked> & bsa, bool ranged)
{
	(void)battleID;
	(void)bsa;
	(void)ranged;
}

void CBattleLLM::battleEnd(const BattleID & battleID, const BattleResult * br, QueryID queryID)
{
	(void)battleID;
	(void)br;
	(void)queryID;
}

void CBattleLLM::battleNewRoundFirst(const BattleID & battleID)
{
	(void)battleID;
}

void CBattleLLM::battleNewRound(const BattleID & battleID)
{
	(void)battleID;
	roundIndex += 1;
}

void CBattleLLM::battleStackMoved(const BattleID & battleID, const CStack * stack, const BattleHexArray & dest, int distance, bool teleport)
{
	(void)battleID;
	(void)stack;
	(void)dest;
	(void)distance;
	(void)teleport;
}

void CBattleLLM::battleSpellCast(const BattleID & battleID, const BattleSpellCast * sc)
{
	(void)battleID;
	(void)sc;
}

void CBattleLLM::battleStacksEffectsSet(const BattleID & battleID, const SetStackEffect & sse)
{
	(void)battleID;
	(void)sse;
}

void CBattleLLM::battleStart(const BattleID & battleID, const CCreatureSet * army1, const CCreatureSet * army2, int3 tile, const CGHeroInstance * hero1, const CGHeroInstance * hero2, BattleSide side_, bool replayAllowed)
{
	(void)battleID;
	(void)army1;
	(void)army2;
	(void)tile;
	(void)hero1;
	(void)hero2;
	(void)replayAllowed;
	side = side_;
	roundIndex = 0;
	turnIndex = 0;
}

void CBattleLLM::battleCatapultAttacked(const BattleID & battleID, const CatapultAttack & ca)
{
	(void)battleID;
	(void)ca;
}

void CBattleLLM::print(const std::string & text) const
{
	logAi->trace("CBattleLLM [%p]: %s", this, text.c_str());
}
