/*
 * HttpApiServer.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "HttpApiServer.h"
#include "LobbyDatabase.h"
#include "EmbeddedWebAssets.h"

#include "../lib/json/JsonNode.h"
#include "../lib/logging/CLogger.h"

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

/// Extracts the value of a URL query parameter by key (e.g. "hours" from "/api?hours=24")
static std::string extractQueryParameter(boost::beast::string_view target, const std::string & key)
{
	std::string t(target);
	auto qpos = t.find('?');
	if (qpos == std::string::npos)
		return {};
	std::string query = t.substr(qpos + 1);
	const std::string prefix = key + '=';
	for (std::string::size_type pos = 0; pos < query.size(); )
	{
		auto amp = query.find('&', pos);
		std::string part = query.substr(pos, amp == std::string::npos ? std::string::npos : amp - pos);
		if (part.substr(0, prefix.size()) == prefix)
			return part.substr(prefix.size());
		if (amp == std::string::npos) break;
		pos = amp + 1;
	}
	return {};
}

HttpApiServer::HttpApiServer(boost::asio::io_context & ioc, LobbyDatabase & database, unsigned short port, bool localhostOnly)
	: database(database)
	, port(port)
	, localhostOnly(localhostOnly)
	, ioc(ioc)
{
}

HttpApiServer::~HttpApiServer()
{
	stop();
}

void HttpApiServer::start()
{
	startTime = std::chrono::system_clock::now();

	acceptor = std::make_unique<tcp::acceptor>(ioc);
	if (localhostOnly)
	{
		tcp::endpoint ep{boost::asio::ip::address_v4::loopback(), port};
		acceptor->open(ep.protocol());
		acceptor->set_option(tcp::acceptor::reuse_address(true));
		acceptor->bind(ep);
	}
	else
	{
		tcp::endpoint ep{tcp::v6(), port};
		acceptor->open(ep.protocol());
		acceptor->set_option(tcp::acceptor::reuse_address(true));
		acceptor->set_option(boost::asio::ip::v6_only(false));
		acceptor->bind(ep);
	}
	acceptor->listen();

	doAccept();
	logGlobal->info("HTTP API Server started on port %d", port);
}

void HttpApiServer::stop()
{
	if (acceptor && acceptor->is_open())
	{
		acceptor->close();
		logGlobal->info("HTTP API Server stopped");
	}
}

void HttpApiServer::doAccept()
{
	acceptor->async_accept([this](boost::system::error_code ec, tcp::socket socket)
	{
		if (!ec)
		{
			auto stream = std::make_shared<beast::tcp_stream>(std::move(socket));
			auto buffer = std::make_shared<beast::flat_buffer>();
			auto req = std::make_shared<http::request<http::string_body>>();

			http::async_read(*stream, *buffer, *req,
				[this, stream, buffer, req](boost::system::error_code readEc, std::size_t) mutable
				{
					if (readEc)
					{
						logGlobal->error("HTTP read error: %s", readEc.message());
						return;
					}
					try
					{
						auto res = std::make_shared<http::response<http::string_body>>(handleRequest(std::move(*req), *stream));
						http::async_write(*stream, *res,
							[stream, res](boost::system::error_code, std::size_t)
							{
								beast::error_code shutdownEc;
								stream->socket().shutdown(tcp::socket::shutdown_send, shutdownEc);
							});
					}
					catch (const std::exception & e)
					{
						logGlobal->error("HTTP session error: %s", e.what());
					}
				});
		}
		if (acceptor && acceptor->is_open())
			doAccept();
	});
}

http::response<http::string_body> HttpApiServer::handleRequest(http::request<http::string_body> && req, beast::tcp_stream & stream)
{
	// Log the request
	std::string clientIP = "unknown";
	try {
		auto endpoint = stream.socket().remote_endpoint();
		clientIP = endpoint.address().to_string();
	}
	catch(const boost::system::system_error & e)
	{
		logGlobal->warn("HTTP API: could not get client IP: %s", e.what());
	}
	
	std::string userAgent = std::string(req[http::field::user_agent]);
	if (userAgent.empty()) userAgent = "unknown";
	
	logGlobal->info("HTTP API Request: %s %s from %s (User-Agent: %s)", 
		req.method_string().data(), 
		req.target().data(), 
		clientIP.c_str(), 
		userAgent.c_str());

	auto const createResponse = [&req](http::status status, const std::string & body, const std::string & contentType = "application/json")
	{
		http::response<http::string_body> res{status, req.version()};
		res.set(http::field::server, "VCMI-Lobby-API");
		res.set(http::field::content_type, contentType);
		res.keep_alive(req.keep_alive());
		res.body() = body;
		res.prepare_payload();
		return res;
	};

	try
	{
		if (req.target() == "/api/v1/stats")
		{
			return createResponse(http::status::ok, getStats());
		}
		else if (req.target().starts_with("/api/v1/chats"))
		{
			std::string channelName = "english";
			if (auto val = extractQueryParameter(req.target(), "channelName"); !val.empty())
				channelName = val;

			return createResponse(http::status::ok, getChats(channelName));
		}
		else if (req.target().starts_with("/api/v1/rooms"))
		{
			int hours = -1;
			int limit = 50;
			if (auto val = extractQueryParameter(req.target(), "hours"); !val.empty())
			{
				try { hours = std::stoi(val); }
				catch (const std::invalid_argument &) { return createResponse(http::status::bad_request, R"({"error":"Parameter 'hours' must be an integer"})"); }
				catch (const std::out_of_range &) { return createResponse(http::status::bad_request, R"({"error":"Parameter 'hours' is out of range"})"); }
			}
			if (auto val = extractQueryParameter(req.target(), "limit"); !val.empty())
			{
				try
				{
					limit = std::stoi(val);
					if (limit < 1 || limit > 250)
						return createResponse(http::status::bad_request, R"({"error":"Parameter 'limit' must be between 1 and 250"})");
				}
				catch (const std::invalid_argument &) { return createResponse(http::status::bad_request, R"({"error":"Parameter 'limit' must be an integer"})"); }
				catch (const std::out_of_range &) { return createResponse(http::status::bad_request, R"({"error":"Parameter 'limit' must be between 1 and 250"})"); }
			}
			
			return createResponse(http::status::ok, getRooms(hours, limit));
		}
		else if (req.target() == "/api/docs" || req.target() == "/")
		{
			std::string html = EmbeddedFiles::SWAGGER_CONTENT;
			return createResponse(http::status::ok, html, "text/html");
		}
		else if (req.target() == "/api/openapi.yaml")
		{
			std::string spec = EmbeddedFiles::OPENAPI_CONTENT;
			return createResponse(http::status::ok, spec, "text/yaml");
		}
		else
		{
			// 404 Not Found
			std::string json = R"({ "error": "Not Found", "message": "The requested endpoint does not exist" })";
			return createResponse(http::status::not_found, json);
		}
	}
	catch (const std::exception & e)
	{
		logGlobal->error("Error handling HTTP request: %s", e.what());
		return createResponse(http::status::internal_server_error, R"({"error":"Internal Server Error"})");
	}
}

std::string HttpApiServer::formatTimestamp(std::chrono::system_clock::time_point timePoint)
{
	auto tt = std::chrono::system_clock::to_time_t(timePoint);
	std::tm tm{};
	localtime_r(&tt, &tm);
	std::ostringstream oss;
	oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S%z");
	return oss.str();
}

bool HttpApiServer::isCacheValid(const CacheEntry & entry) const
{
	return std::chrono::system_clock::now() - entry.timestamp < std::chrono::seconds(CACHE_TTL_SECONDS);
}

std::string HttpApiServer::getStats()
{
	{
		std::lock_guard<std::mutex> lock(cacheMutex);
		if (statsCache && isCacheValid(*statsCache))
			return statsCache->json;
	}
	JsonNode stats;
	stats["onlinePlayers"].Vector() = JsonVector();
	for (const auto & player : database.getActiveAccounts())
		stats["onlinePlayers"].Vector().push_back(JsonNode(player.displayName));
	auto activeCounts = database.getActiveAccountsCounts();
	stats["onlinePlayersCount"].Struct() = JsonMap{
		{"current", JsonNode(static_cast<int64_t>(stats["onlinePlayers"].Vector().size()))},
		{"lastHour", JsonNode(static_cast<int64_t>(activeCounts.h1))},
		{"lastDay", JsonNode(static_cast<int64_t>(activeCounts.h24))},
		{"lastWeek", JsonNode(static_cast<int64_t>(activeCounts.h168))},
		{"lastMonth", JsonNode(static_cast<int64_t>(activeCounts.h720))},
		{"lastYear", JsonNode(static_cast<int64_t>(activeCounts.h8760))}
	};
	auto registeredCounts = database.getRegisteredAccountsCounts();
	stats["registeredPlayersCount"].Struct() = JsonMap{
		{"total", JsonNode(static_cast<int64_t>(registeredCounts.total))},
		{"lastDay", JsonNode(static_cast<int64_t>(registeredCounts.h24))},
		{"lastWeek", JsonNode(static_cast<int64_t>(registeredCounts.h168))},
		{"lastMonth", JsonNode(static_cast<int64_t>(registeredCounts.h720))},
		{"lastYear", JsonNode(static_cast<int64_t>(registeredCounts.h8760))}
	};
	std::map<LobbyRoomState, int> lobbysCount;
	for (const auto & room : database.getActiveGameRooms())
		lobbysCount[room.roomState]++;
	auto closedCounts = database.getClosedGameRoomsCounts();
	stats["gameCount"].Struct() = JsonMap{
		{"current", JsonNode(lobbysCount[LobbyRoomState::BUSY])},
		{"total", JsonNode(static_cast<int64_t>(closedCounts.total))},
		{"lastDay", JsonNode(static_cast<int64_t>(closedCounts.h24))},
		{"lastWeek", JsonNode(static_cast<int64_t>(closedCounts.h168))},
		{"lastMonth", JsonNode(static_cast<int64_t>(closedCounts.h720))},
		{"lastYear", JsonNode(static_cast<int64_t>(closedCounts.h8760))}
	};
	stats["lobbyCount"].Struct() = JsonMap{
		{"current", JsonNode(static_cast<int64_t>(lobbysCount[LobbyRoomState::PUBLIC] + lobbysCount[LobbyRoomState::PRIVATE]))},
		{"public", JsonNode(static_cast<int64_t>(lobbysCount[LobbyRoomState::PUBLIC]))},
		{"private", JsonNode(static_cast<int64_t>(lobbysCount[LobbyRoomState::PRIVATE]))}
	};
	stats["lobbyStartTime"].String() = formatTimestamp(startTime);
	
	stats["server"].String() = "VCMI Lobby";
	stats["apiVersion"].String() = "1.0";

	std::string json = stats.toCompactString();
	{
		std::lock_guard<std::mutex> lock(cacheMutex);
		statsCache = CacheEntry{json, std::chrono::system_clock::now()};
	}
	return json;
}

std::string HttpApiServer::getChats(const std::string & channelName)
{
	{
		std::lock_guard<std::mutex> lock(cacheMutex);
		auto it = chatsCache.find(channelName);
		if (it != chatsCache.end() && isCacheValid(it->second))
			return it->second.json;
	}

	JsonNode chats;
	chats["messages"].Vector() = JsonVector();
	chats["channelName"].String() = channelName;
	
	auto messages = database.getRecentMessageHistory("global", channelName);
	
	for (const auto & msg : messages)
	{
		JsonNode message;
		message["displayName"].String() = msg.displayName;
		message["messageText"].String() = msg.messageText;
		message["ageSeconds"].Integer() = msg.age.count();
		
		auto messageTime = std::chrono::system_clock::now() - msg.age;
		message["timestamp"].String() = formatTimestamp(messageTime);
		
		chats["messages"].Vector().push_back(message);
	}
	
	chats["count"].Integer() = chats["messages"].Vector().size();

	std::string json = chats.toCompactString();
	{
		std::lock_guard<std::mutex> lock(cacheMutex);
		chatsCache[channelName] = CacheEntry{json, std::chrono::system_clock::now()};
	}
	return json;
}

std::string HttpApiServer::serializeRooms(const std::vector<LobbyGameRoom> & rooms, int hours, int limit)
{
	JsonNode result;
	result["rooms"].Vector() = JsonVector();
	result["hours"].Integer() = hours;
	result["limit"].Integer() = limit;

	int count = 0;
	for (const auto & room : rooms)
	{
		if (hours != -1 && room.age > std::chrono::hours(hours))
			continue;
		if (count >= limit)
			break;

		JsonNode roomNode;
		roomNode["description"].String() = room.description;
		roomNode["status"].Integer() = static_cast<int>(room.roomState);
		roomNode["playerLimit"].Integer() = room.playerLimit;
		roomNode["version"].String() = room.version;
		roomNode["secondsElapsed"].Integer() = room.age.count();

		auto creationTime = std::chrono::system_clock::now() - room.age;
		roomNode["createdAt"].String() = formatTimestamp(creationTime);

		// Parse mods JSON string
		try {
			JsonNode modsNode(reinterpret_cast<const std::byte*>(room.modsJson.data()), room.modsJson.size(), "");
			roomNode["mods"] = modsNode;
		} catch(const std::exception & e) {
			logGlobal->warn("HTTP API: failed to parse mods JSON: %s", e.what());
			roomNode["mods"].Struct() = JsonMap{};
		}

		result["rooms"].Vector().push_back(roomNode);
		++count;
	}

	result["count"].Integer() = result["rooms"].Vector().size();
	return result.toCompactString();
}

std::string HttpApiServer::getRooms(int hours, int limit)
{
	{
		std::lock_guard<std::mutex> lock(cacheMutex);
		if (roomsCache)
		{
			const auto & c = *roomsCache;
			const bool ttlOk    = std::chrono::system_clock::now() - c.timestamp < std::chrono::seconds(CACHE_TTL_SECONDS);
			const bool hoursOk  = c.fetchedHours == -1 || (hours != -1 && c.fetchedHours >= hours);
			const bool limitOk  = c.fetchedLimit >= limit;
			if (ttlOk && hoursOk && limitOk)
				return serializeRooms(c.rooms, hours, limit);
		}
	}

	auto fetchedRooms = database.getRooms(hours, limit);

	{
		std::lock_guard<std::mutex> lock(cacheMutex);
		roomsCache = RoomsCacheEntry{fetchedRooms, hours, limit, std::chrono::system_clock::now()};
	}

	return serializeRooms(fetchedRooms, hours, limit);
}
