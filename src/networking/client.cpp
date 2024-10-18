#include "client.h"
#include "core/camera.h"
#include "core/types.h"
#include "glm/ext/vector_float3.hpp"
#include "steam/isteamnetworkingsockets.h"
#include "steam/steamnetworkingsockets.h"
#include "steam/steamnetworkingtypes.h"
#include <cstdint>
#include <steam/isteamnetworkingutils.h>
#include <format>
#include <iostream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace Engine {

ISteamNetworkingSockets* Client::s_networkingSockets = nullptr;
HSteamNetConnection Client::s_connection = k_HSteamNetConnection_Invalid; 

void Client::on_steam_net_connection_status_changed(SteamNetConnectionStatusChangedCallback_t *pInfo)
{
	std::cout << "Connection state changed: " << pInfo->m_info.m_eState << std::endl;
	if (pInfo->m_info.m_eState == k_ESteamNetworkingConnectionState_Connected) {
		std::cout << "Successfully connected to server!" << std::endl;
	} else if (pInfo->m_info.m_eState == k_ESteamNetworkingConnectionState_ClosedByPeer || pInfo->m_info.m_eState == k_ESteamNetworkingConnectionState_ProblemDetectedLocally) {
		std::cout << "Connection closed!" << std::endl;
		s_networkingSockets->CloseConnection(pInfo->m_hConn, 0, nullptr, false);
	}
}

Client::Error Client::init(const char* serverAddrStr) {
	SteamDatagramErrMsg errMsg;
	if (!GameNetworkingSockets_Init(nullptr, errMsg)) {
		return Error(ErrorCode::NetworkingInitError, std::format("Failed to initialize networking: {}", errMsg));
	}

	s_networkingSockets = SteamNetworkingSockets();

	SteamNetworkingIPAddr serverAddr;
	serverAddr.Clear();
	serverAddr.ParseString(serverAddrStr);
	serverAddr.m_port = SERVER_PORT;

	SteamNetworkingConfigValue_t opt;
	opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void*)on_steam_net_connection_status_changed);

	s_connection = s_networkingSockets->ConnectByIPAddress(serverAddr, 1, &opt);

	return Error(ErrorCode::Success, "");
}

Client::~Client()
{
	GameNetworkingSockets_Kill();
}

std::unordered_map<uint64_t, glm::vec3> Client::get_current_positions()
{
	std::unordered_map<uint64_t, glm::vec3> positions;
	s_networkingSockets->RunCallbacks();
	while (true) {
		ISteamNetworkingMessage *incomingMessage = nullptr;
		int numMsgs = s_networkingSockets->ReceiveMessagesOnConnection(s_connection, &incomingMessage, 1);
		if (numMsgs == 0) {
			// No more messages, break the loop
			break;
		}
		if (numMsgs < 0) {
			// Handle error
			std::cerr << "Error checking for messages!" << std::endl;
			break;
		}

		assert(numMsgs == 1 && incomingMessage);

		struct UpdatePos {
			uint8_t messageType;
			uint64_t uuid;
			float posX;
			float posY;
			float posZ;
		};

		uint8_t messageType = *((uint8_t*)incomingMessage->m_pData);
		if (messageType == static_cast<uint8_t>(MessageType::UpdatePos)) {
			UpdatePos* playerUpdate = reinterpret_cast<UpdatePos*>(incomingMessage->m_pData);
			positions[playerUpdate->uuid] = glm::vec3(playerUpdate->posX, playerUpdate->posY, playerUpdate->posZ);
		}

		incomingMessage->Release();
	}
	return positions;
}

uint64_t Client::wait_for_uuid()
{
	bool entityReceived = false;
	uint64_t playerUUID;
	while (!entityReceived) {
		ISteamNetworkingMessage *pIncomingMessage = nullptr;
		int numMessages = s_networkingSockets->ReceiveMessagesOnConnection(s_connection, &pIncomingMessage, 1);

		if (numMessages > 0 && pIncomingMessage) {

			uint8_t messageType = *((uint8_t*)pIncomingMessage->m_pData);
			if (!(messageType == static_cast<uint8_t>(MessageType::ClientCreateEntity))) {
				continue;
			}
			struct ServerMessage {
				uint8_t messageType;
				uint64_t uuid;
			};

			ServerMessage *msg = reinterpret_cast<ServerMessage *>(pIncomingMessage->m_pData);

			playerUUID = msg->uuid;

			entityReceived = true;

			pIncomingMessage->Release();

			send_server_ack();
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	return playerUUID;
}

void Client::send_server_ack()
{
	struct EntityCreatedAck {
		uint8_t messageType;
		uint8_t created;
	};
	EntityCreatedAck ack {};
	ack.messageType = static_cast<uint8_t>(MessageType::EntityCreatedAck);
	ack.created = true;

	s_networkingSockets->SendMessageToConnection(s_connection, &ack, sizeof(EntityCreatedAck), 0, nullptr);
}

void Client::send_pos_update(Camera& camera, uint64_t uuid)
{
	static glm::vec3 prevPos;
	if (!glm::all(glm::equal(prevPos, camera.position))) {
		prevPos = camera.position;
		struct SendData {
			uint8_t messageType;
			uint64_t uuid;
			float x;
			float y;
			float z;
		};
		SendData data = { static_cast<uint8_t>(MessageType::UpdatePos), uuid, camera.position.x, camera.position.y, camera.position.z };
		s_networkingSockets->SendMessageToConnection(s_connection, &data, sizeof(SendData), k_nSteamNetworkingSend_Reliable, nullptr);
	}
}

}
