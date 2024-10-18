#include "steam/steamnetworkingtypes.h"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <ostream>
#include <random>
#include <steam/steamnetworkingsockets.h>
#include <string.h>
#include <thread>
#include <unordered_map>
#include <unordered_set>

enum class MessageType: uint8_t {
	UpdatePos,
	ClientCreateEntity,
	EntityCreatedAck
};

struct ClientCreateEntity {
	uint8_t messageType;
	uint64_t entity;
};

struct EntityCreatedAck {
	uint8_t messageType;
	uint8_t created;
};

struct Vec3 {
	float x;
	float y;
	float z;
};

struct UpdatePos {
	uint8_t messageType;
	uint64_t uuid;
	float posX;
	float posY;
	float posZ;
};

struct UpdatePosData {
	Vec3 pos;
};

struct ClientData {
	Vec3 currentPos;
	Vec3 prevPos;
	uint64_t uuid;
	bool dataChanged = false;
};

static constexpr uint16_t SERVER_PORT = 27020;

static ISteamNetworkingSockets* g_networkingSockets = nullptr;
static HSteamListenSocket g_listenSocket;
static HSteamNetPollGroup g_pollGroup;

static std::unordered_map<HSteamNetConnection, ClientData> g_connectedClients;
static std::unordered_map<HSteamNetConnection, ClientData> g_pendingToConnectClients;

static std::unordered_set<uint64_t> g_generatedIds;


uint64_t generateUniqueId() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;

    uint64_t newId;
    do {
        newId = dis(gen);
    } while (g_generatedIds.find(newId) != g_generatedIds.end());

    g_generatedIds.insert(newId);
    return newId;
}

void set_client_nick(HSteamNetConnection conn, const char* nick)
{
	g_networkingSockets->SetConnectionName(conn, nick);
}

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
			return;
		}

		char nick[ 64 ];
		sprintf(nick, "BraveWarrior%d", 10000 + (rand() % 100000));

		uint64_t uuid = generateUniqueId();

		g_pendingToConnectClients[pInfo->m_hConn];
		g_pendingToConnectClients[pInfo->m_hConn].uuid = uuid;

		ClientCreateEntity createEntityRequest {};
		createEntityRequest.messageType = static_cast<uint8_t>(MessageType::ClientCreateEntity);
		createEntityRequest.entity = uuid;
		g_networkingSockets->SendMessageToConnection(pInfo->m_hConn, &createEntityRequest, sizeof(ClientCreateEntity), k_nSteamNetworkingSend_Reliable, nullptr);

		set_client_nick(pInfo->m_hConn, nick);
	}
}

void send_string_to_client(HSteamNetConnection conn, const char* str)
{
	g_networkingSockets->SendMessageToConnection(conn, str, (uint32_t)strlen(str), k_nSteamNetworkingSend_Reliable, nullptr);
}

void send_updates_to_all_clients()
{
	HSteamNetConnection except = k_HSteamNetConnection_Invalid;
	for (auto& [k1, v1] : g_connectedClients) {
		for (auto& [k2, v2] : g_connectedClients) {
			if (k1 != except) {
				UpdatePos sendData{};
				sendData.messageType = static_cast<uint8_t>(MessageType::UpdatePos);
				sendData.posX = v2.currentPos.x;
				sendData.posY = v2.currentPos.y;
				sendData.posZ = v2.currentPos.z;
				sendData.uuid = v2.uuid;
				g_networkingSockets->SendMessageToConnection(k1, &sendData, sizeof(UpdatePos), k_nSteamNetworkingSend_Reliable, nullptr);
			}
		}
	}
}

void poll_messages()
{
	while (true) {
		ISteamNetworkingMessage *incomingMessage = nullptr;
		int numMsgs = g_networkingSockets->ReceiveMessagesOnPollGroup(g_pollGroup, &incomingMessage, 1);
		if (numMsgs == 0) {
			break;
		}
		if (numMsgs < 0) {
			std::cerr << "Error checking for messages" << std::endl;
			return;
		}

		assert(numMsgs == 1 && incomingMessage);

		uint8_t messageType = *((uint8_t*)incomingMessage->m_pData);
		if (messageType == static_cast<uint8_t>(MessageType::UpdatePos)) {
			auto itClient = g_connectedClients.find(incomingMessage->m_conn);
			assert(itClient != g_connectedClients.end());

			UpdatePos* playerUpdate = reinterpret_cast<UpdatePos*>(incomingMessage->m_pData);
			Vec3 newPos = { playerUpdate->posX, playerUpdate->posY, playerUpdate->posZ };
			g_connectedClients[incomingMessage->m_conn].currentPos = newPos;
			g_connectedClients[incomingMessage->m_conn].dataChanged = true;
		} else if (messageType == static_cast<uint8_t>(MessageType::EntityCreatedAck)) {

			auto itClient = g_pendingToConnectClients.find(incomingMessage->m_conn);
			assert(itClient != g_pendingToConnectClients.end());

			EntityCreatedAck* entityCreated = reinterpret_cast<EntityCreatedAck*>(incomingMessage->m_pData);
			if (!g_pendingToConnectClients.contains(incomingMessage->m_conn) || !entityCreated->created) {
				std::cerr << "Error creating the entity" << std::endl;
				continue;
			}
			g_connectedClients[incomingMessage->m_conn] = std::move(g_pendingToConnectClients[incomingMessage->m_conn]);
			g_pendingToConnectClients.erase(incomingMessage->m_conn);
		}

		incomingMessage->Release();
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
		
		poll_messages();
		send_updates_to_all_clients();

		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	std::cout << "Closing connections..." << std::endl;
	for (auto [k, _]: g_connectedClients)
	{
		// Send them one more goodbye message.  Note that we also have the
		// connection close reason as a place to send final data.  However,
		// that's usually best left for more diagnostic/debug text not actual
		// protocol strings.
		send_string_to_client(k, "Server is shutting down.  Goodbye.");

		// Close the connection.  We use "linger mode" to ask SteamNetworkingSockets
		// to flush this out and close gracefully.
		g_networkingSockets->CloseConnection(k, 0, "Server Shutdown", true );
	}

	g_connectedClients.clear();

	g_networkingSockets->CloseListenSocket(g_listenSocket);
	g_listenSocket = k_HSteamListenSocket_Invalid;

	g_networkingSockets->DestroyPollGroup(g_pollGroup);
	g_pollGroup = k_HSteamNetPollGroup_Invalid;

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
