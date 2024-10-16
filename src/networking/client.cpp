#include "client.h"
#include "core/camera.h"
#include "glm/ext/vector_relational.hpp"
#include "steam/isteamnetworkingsockets.h"
#include "steam/steamnetworkingsockets.h"
#include "steam/steamnetworkingtypes.h"
#include <steam/isteamnetworkingutils.h>
#include <format>
#include <iostream>

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

void Client::poll_messages()
{
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
		std::cout << "Received message: " << std::string((char*)incomingMessage->m_pData, incomingMessage->m_cbSize) << std::endl;
		incomingMessage->Release();
	}
}

void Client::send_pos_update(Camera& camera)
{
	static glm::vec3 prevPos;
	if (!glm::all(glm::equal(prevPos, camera.position))) {
		prevPos = camera.position;
		SendData data = { static_cast<uint8_t>(MessageType::UpdatePos), camera.position.x, camera.position.y, camera.position.z };
		s_networkingSockets->SendMessageToConnection(s_connection, &data, sizeof(SendData), k_nSteamNetworkingSend_Reliable, nullptr);
	}
}

}
