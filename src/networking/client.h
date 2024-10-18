#pragma once

#include "core/camera.h"
#include "steam/isteamnetworkingsockets.h"
#include "steam/steamnetworkingtypes.h"
#include <cstdint>
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
		UpdatePos,
		ClientCreateEntity,
		EntityCreatedAck
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
	uint64_t wait_for_uuid();
	std::unordered_map<uint64_t, glm::vec3> get_current_positions();
	void send_pos_update(Camera& camera, uint64_t uuid);
private:
	static ISteamNetworkingSockets* s_networkingSockets;
	static HSteamNetConnection s_connection;

	static void on_steam_net_connection_status_changed(SteamNetConnectionStatusChangedCallback_t *pInfo);

	void send_server_ack();
};

}
