#pragma once

#include "GameManager.h"
#include "SubscriptionManager.h"
#include <string>
#include <vector>

class StompProtocol
{
private:
    GameManager& gameManager;
    SubscriptionManager& subscriptionManager;
    bool isConnected;

public:
    StompProtocol(GameManager& gameManager, SubscriptionManager& subscriptionManager);
    
    // Process a frame received from the server
    // Returns true if the connection should continue, false if we should disconnect
    bool process(const std::string& frameBody);

    void setConnected(bool connected) { isConnected = connected; }
    bool getConnected() const { return isConnected; }

    // Register a callback/message to be printed when a receipt is received
    void addReceiptAction(int receiptId, std::string msg);

    void setCurrentUser(std::string user) { currentUser = user; }

private:
    std::string currentUser;
    std::map<int, std::string> receiptActions;
    std::mutex receiptMutex;
};
