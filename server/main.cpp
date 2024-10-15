#include "steam/steamnetworkingtypes.h"
#include <cstdint>
#include <iostream>
#include <ostream>
#include <steam/steamnetworkingsockets.h>
#include <thread>

static constexpr uint16_t SERVER_PORT = 27020;

static ISteamNetworkingSockets* g_networkingSockets = nullptr;
static HSteamListenSocket g_listenSocket;
static HSteamNetPollGroup g_pollGroup;

void OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t *pInfo)
{
	if (pInfo->m_info.m_eState == k_ESteamNetworkingConnectionState_Connected) {
		std::cout << "Client connected!" << std::endl;
	} else if (pInfo->m_info.m_eState == k_ESteamNetworkingConnectionState_ClosedByPeer || pInfo->m_info.m_eState == k_ESteamNetworkingConnectionState_ProblemDetectedLocally) {
		std::cout << "Client disconnected!" << std::endl;
		g_networkingSockets->CloseConnection(pInfo->m_hConn, 0, nullptr, false);
	} else if (pInfo->m_info.m_eState == k_ESteamNetworkingConnectionState_Connecting)
	{
		std::cout << "Connection request from" << pInfo->m_info.m_szConnectionDescription;

		// A client is attempting to connect
		// Try to accept the connection.
		if (g_networkingSockets->AcceptConnection(pInfo->m_hConn ) != k_EResultOK)
		{
			// This could fail.  If the remote host tried to connect, but then
			// disconnected, the connection may already be half closed.  Just
			// destroy whatever we have on our side.
			g_networkingSockets->CloseConnection(pInfo->m_hConn, 0, nullptr, false);
			std::cerr << "Can't accept connection. (It was already closed?)";
			return;
		}
		// Assign the poll group
		if (!g_networkingSockets->SetConnectionPollGroup(pInfo->m_hConn, g_pollGroup))
		{
			g_networkingSockets->CloseConnection(pInfo->m_hConn, 0, nullptr, false);
			std::cerr << "Failed to set poll group?";
		}
	}
}

void run_server()
{
	SteamNetworkingIPAddr serverLocalAddr;
	serverLocalAddr.Clear();
	serverLocalAddr.m_port = SERVER_PORT;

	SteamNetworkingConfigValue_t opt;
	opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void*)OnSteamNetConnectionStatusChanged);

	g_listenSocket = g_networkingSockets->CreateListenSocketIP(serverLocalAddr, 1, &opt);
	if (!g_listenSocket) {
		std::cerr << "Failed to create listen socket! Error: " << g_networkingSockets << std::endl;
		return;
	}
	g_pollGroup = g_networkingSockets->CreatePollGroup();

	if (!g_pollGroup) {
		std::cerr << "Failed to create listen poll group!" << std::endl;
		return;
	}

	std::cout << "Server started, listening on port " << SERVER_PORT << "..." << std::endl;

	while(true) {
		g_networkingSockets->RunCallbacks();
		ISteamNetworkingMessage* incomingMsg = nullptr;
		int numMsgs = g_networkingSockets->ReceiveMessagesOnPollGroup(g_pollGroup, &incomingMsg, 1);
		if (numMsgs > 0 && incomingMsg) {
			std::cout << "Received message: " << std::string((char*)incomingMsg->m_pData, incomingMsg->m_cbSize) << std::endl;
		incomingMsg->Release();
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
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

	std::thread serverThread(run_server);
	serverThread.join();
}
