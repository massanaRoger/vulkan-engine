#pragma once

#include "steam/steamnetworkingtypes.h"

namespace Engine {

class Client {
public:
	Client();
private:
	ISteamNetworkingMessage* m_networkingSockets = nullptr;
};

}
