#include "steam/steamnetworkingtypes.h"
#include <cstdint>
#include <iostream>
#include <ostream>
#include <steam/steamnetworkingsockets.h>
#include <thread>

static constexpr uint16_t SERVER_PORT = 27020;

ISteamNetworkingSockets* g_networkingSockets = nullptr;
HSteamListenSocket g_listenSocket;
HSteamNetPollGroup g_pollGroup;

void OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t *pInfo)
{
    if (pInfo->m_info.m_eState == k_ESteamNetworkingConnectionState_Connected) {
        std::cout << "Client connected!" << std::endl;
    } else if (pInfo->m_info.m_eState == k_ESteamNetworkingConnectionState_ClosedByPeer || pInfo->m_info.m_eState == k_ESteamNetworkingConnectionState_ProblemDetectedLocally) {
        std::cout << "Client disconnected!" << std::endl;
        g_networkingSockets->CloseConnection(pInfo->m_hConn, 0, nullptr, false);
    }
}

int main()
{
	SteamDatagramErrMsg errMsg;
	if (!GameNetworkingSockets_Init(nullptr, errMsg)) {
		std::cerr << "Failed to initialize networking: " << errMsg << std::endl;
		return 1;
	}

	g_networkingSockets = SteamNetworkingSockets();

	SteamNetworkingIPAddr serverLocalAddr;
	serverLocalAddr.Clear();
	serverLocalAddr.m_port = SERVER_PORT;

	SteamNetworkingConfigValue_t opt;
	opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void*)OnSteamNetConnectionStatusChanged);

	g_listenSocket = g_networkingSockets->CreateListenSocketIP(serverLocalAddr, 1, &opt);
	g_pollGroup = g_networkingSockets->CreatePollGroup();

	if (!g_listenSocket || !g_pollGroup) {
		std::cerr << "Failed to create listen socket or poll group!" << std::endl;
		return 1;
	}

	std::cout << "Server started, listening on port " << SERVER_PORT << "..." << std::endl;

	while(true) {
		ISteamNetworkingMessage* incomingMsg = nullptr;
		int numMsgs = g_networkingSockets->ReceiveMessagesOnPollGroup(g_pollGroup, &incomingMsg, 1);
		if (numMsgs > 0 && incomingMsg) {
			std::cout << "Received message: " << std::string((char*)incomingMsg->m_pData, incomingMsg->m_cbSize) << std::endl;
			incomingMsg->Release();
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

}
