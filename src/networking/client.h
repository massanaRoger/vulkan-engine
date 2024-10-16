#pragma once

#include "core/camera.h"
#include "steam/isteamnetworkingsockets.h"
#include "steam/steamnetworkingtypes.h"
#include <iostream>
#include <string>

namespace Engine {

static constexpr uint16_t SERVER_PORT = 27020;
class Client {
public:
	enum class ErrorCode {
		Success,
		NetworkingInitError,
	};

	enum class MessageType: uint8_t {
		UpdatePos
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
	~Client();
	Error init(const char* serverAddrStr);
	void poll_messages();
	void send_pos_update(Camera& camera);
private:

	#pragma pack(push, 1)
	struct SendData {
		uint8_t messageType;
		float posX;
		float posY;
		float posZ;
	};
	#pragma pack(pop)

	static ISteamNetworkingSockets* s_networkingSockets;
	static HSteamNetConnection s_connection;

	static void on_steam_net_connection_status_changed(SteamNetConnectionStatusChangedCallback_t *pInfo);
};

}
