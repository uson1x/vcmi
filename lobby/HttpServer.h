/*
 * HttpServer.h, part of VCMI engine
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

/// Interface that must be implemented to handle the concrete API endpoints
class ILobbyHttpHandler
{
public:
	virtual ~ILobbyHttpHandler() = default;

	virtual std::string getApiStats() = 0;
	virtual std::string getApiChats(const std::string & channelName) = 0;
	virtual std::string getApiRooms(int hours, int limit) = 0;
};

/// Generic HTTP/REST server built on Boost.Beast.
/// Handles the TCP accept loop, request parsing, routing, and response writing.
/// Delegates concrete API data to ILobbyHttpHandler.
class HttpServer
{
public:
	HttpServer(boost::asio::io_context & ioc, ILobbyHttpHandler & handler, unsigned short port, bool localhostOnly);
	~HttpServer();

	void start();
	void stop();

private:
	void doAccept();
	boost::beast::http::response<boost::beast::http::string_body> handleRequest(
		boost::beast::http::request<boost::beast::http::string_body> && req,
		boost::beast::tcp_stream & stream);

	ILobbyHttpHandler & handler;
	unsigned short port;
	bool localhostOnly;
	boost::asio::io_context & ioc;
	std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor;
};
