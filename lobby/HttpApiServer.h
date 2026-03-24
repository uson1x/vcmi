/*
 * HttpApiServer.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include "LobbyDefines.h"

class LobbyDatabase;

class HttpApiServer
{
public:
	static constexpr int CACHE_TTL_SECONDS = 30;

	HttpApiServer(boost::asio::io_context & ioc, LobbyDatabase & database, unsigned short port, bool localhostOnly);
	~HttpApiServer();

	void start();
	void stop();

private:
	struct CacheEntry
	{
		std::string json;
		std::chrono::system_clock::time_point timestamp;
	};

	bool isCacheValid(const CacheEntry & entry) const;

	void doAccept();
	boost::beast::http::response<boost::beast::http::string_body> handleRequest(boost::beast::http::request<boost::beast::http::string_body> && req, boost::beast::tcp_stream & stream);
	std::string formatTimestamp(std::chrono::system_clock::time_point timePoint);

	std::string getStats();
	std::string getChats(const std::string & channelName);
	std::string getRooms(int hours, int limit);
	std::string serializeRooms(const std::vector<LobbyGameRoom> & rooms, int hours, int limit);

	LobbyDatabase & database;

	unsigned short port;
	bool localhostOnly;
	boost::asio::io_context & ioc;
	std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor;
	std::chrono::system_clock::time_point startTime;

	mutable std::mutex cacheMutex;
	std::optional<CacheEntry> statsCache;
	std::map<std::string, CacheEntry> chatsCache;

	struct RoomsCacheEntry
	{
		std::vector<LobbyGameRoom> rooms;
		int fetchedHours;   // -1 means all time
		int fetchedLimit;
		std::chrono::system_clock::time_point timestamp;
	};
	std::optional<RoomsCacheEntry> roomsCache;
};
