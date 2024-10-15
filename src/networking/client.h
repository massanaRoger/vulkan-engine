#pragma once

#include "steam/isteamnetworkingsockets.h"
#include "steam/steamnetworkingtypes.h"
#include <iostream>
#include <thread>

namespace Engine {

static constexpr uint16_t SERVER_PORT = 27020;
class Client {
public:
	enum class ErrorCode {
		Success,
		NetworkingInitError,
	};

	struct Error {
		ErrorCode code;
		std::string message;

		Error(ErrorCode c, const std::string& msg) : code(c), message(msg) {}

		void print() const {
			std::cout << "Error Code: " << static_cast<int>(code) << " - " << message << std::endl;
		}
	};

	Client() = default;
	Error init();
	void destroy();
	void run(const char* serverAddrStr);
private:
	static ISteamNetworkingSockets* s_networkingSockets;
	static HSteamNetConnection s_connection;

	std::unique_ptr<std::thread> m_clientThread;

	static void run_client_thread(const char* serverAddrStr);
	static void on_steam_net_connection_status_changed(SteamNetConnectionStatusChangedCallback_t *pInfo);
};

}
