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
#include <memory>

VCMI_LIB_NAMESPACE_BEGIN
class JsonNode;
VCMI_LIB_NAMESPACE_END

class LobbyDatabase;

class HttpApiServer
{
public:
	HttpApiServer(boost::asio::io_context & ioc, LobbyDatabase & database, unsigned short port, bool localhostOnly);
	~HttpApiServer();

	void start();
	void stop();

private:
	void doAccept();
	boost::beast::http::response<boost::beast::http::string_body> handleRequest(boost::beast::http::request<boost::beast::http::string_body> && req, boost::beast::tcp_stream & stream);
	std::string formatTimestamp(std::chrono::system_clock::time_point timePoint);

	JsonNode getStats();
	JsonNode getChats(const std::string & channelName);
	JsonNode getRooms(int hours, int limit);

	LobbyDatabase & database;

	unsigned short port;
	bool localhostOnly;
	boost::asio::io_context & ioc;
	std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor;
	std::chrono::system_clock::time_point startTime;
};
