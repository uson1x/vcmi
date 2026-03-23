/*
 * NetworkHandler.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "NetworkDefines.h"

VCMI_LIB_NAMESPACE_BEGIN

class DLL_LINKAGE NetworkHandler final : public INetworkHandler
{
	std::unique_ptr<NetworkContext> ownedContext;
	NetworkContext * context;

public:
	NetworkHandler();
	explicit NetworkHandler(NetworkContext & externalContext);

	std::unique_ptr<INetworkServer> createServerTCP(INetworkServerListener & listener) override;
	void connectToRemote(INetworkClientListener & listener, const std::string & host, uint16_t port) override;
	void createInternalConnection(INetworkClientListener & listener, INetworkServer & server) override;
	std::shared_ptr<INetworkConnection> createAsyncConnection(INetworkConnectionListener & listener) override;
	void createTimer(INetworkTimerListener & listener, std::chrono::milliseconds duration) override;

	void run() override;
	void stop() override;

	static std::unique_ptr<INetworkHandler> createHandlerWithContext(NetworkContext & context);
};

VCMI_LIB_NAMESPACE_END
