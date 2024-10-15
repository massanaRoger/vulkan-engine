#include "client.h"
#include "steam/isteamnetworkingsockets.h"
#include "steam/steamnetworkingsockets.h"
#include <memory>
#include <steam/isteamnetworkingutils.h>
#include <format>
#include <iostream>
#include <thread>

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

Client::Error Client::init() {
	// Ensure enough time for server to start
	std::this_thread::sleep_for(std::chrono::seconds(1));

	SteamDatagramErrMsg errMsg;
	if (!GameNetworkingSockets_Init(nullptr, errMsg)) {
		return Error(ErrorCode::NetworkingInitError, std::format("Failed to initialize networking: {}", errMsg));
	}

	s_networkingSockets = SteamNetworkingSockets();

	return Error(ErrorCode::Success, "");
}

void Client::destroy()
{
	m_clientThread->join();
	GameNetworkingSockets_Kill();
}

void Client::run(const char* serverAddrStr) 
{
	m_clientThread = std::make_unique<std::thread>(run_client_thread, serverAddrStr);
}

void Client::run_client_thread(const char* serverAddrStr)
{
	SteamNetworkingIPAddr serverAddr;
	serverAddr.Clear();
	serverAddr.ParseString(serverAddrStr);
	serverAddr.m_port = SERVER_PORT;

	SteamNetworkingConfigValue_t opt;
	opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void*)on_steam_net_connection_status_changed);

	s_connection = s_networkingSockets->ConnectByIPAddress(serverAddr, 1, &opt);

	while (true) {
		s_networkingSockets->RunCallbacks();
		ISteamNetworkingMessage* incomingMsg = nullptr;
		int numMsgs = s_networkingSockets->ReceiveMessagesOnConnection(s_connection, &incomingMsg, 1);
		if (numMsgs > 0 && incomingMsg) {
			std::cout << "Received message: " << std::string((char*)incomingMsg->m_pData, incomingMsg->m_cbSize) << std::endl;
		incomingMsg->Release();
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		const char* message = "Hello from the client!";
		s_networkingSockets->SendMessageToConnection(s_connection, message, strlen(message), k_nSteamNetworkingSend_Reliable, nullptr);
	}
}

}
