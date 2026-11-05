#include "../include/StompProtocol.h"
#include "../include/event.h"
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <vector>


StompProtocol::StompProtocol(GameManager &gameManager,
                             SubscriptionManager &subscriptionManager)
    : gameManager(gameManager), subscriptionManager(subscriptionManager),
      isConnected(false) {}

bool StompProtocol::process(const std::string &frameBody) {
  std::stringstream ss(frameBody);
  std::string command;
  std::getline(ss, command);
  if (!command.empty() && command.back() == '\r')
    command.pop_back();

  std::map<std::string, std::string> headers;
  std::string line;

  while (std::getline(ss, line) && !line.empty() && line != "\r") {
    if (!line.empty() && line.back() == '\r')
      line.pop_back(); // Fixed 'c' typo to '\r'
    size_t colonPos = line.find(':');
    if (colonPos != std::string::npos) {
      std::string key = line.substr(0, colonPos);
      std::string value = line.substr(colonPos + 1);
      headers[key] = value;
    }
  }

  std::string body;
  char c;
  while (ss.get(c)) {
    body += c;
  }

  if (command == "CONNECTED") {
    isConnected = true;
    std::cout << "Login successful" << std::endl;
  } else if (command == "MESSAGE") {
    // Use the constructor we implemented in event.cpp instead of duplication
    Event event(body);
    std::cout << body
              << std::endl; // Print event to screen for user/verification
    std::string game_name;

    // Debug - removed
    // std::cout << "DEBUG: Received MESSAGE. Owner: " <<
    // event.get_event_owner() << std::endl;

    if (headers.count("destination")) {
      std::string dest = headers["destination"];
      if (dest.size() > 7 && dest.substr(0, 7) == "/topic/") {
        game_name = dest.substr(7);
      } else if (dest.size() > 0 && dest[0] == '/') {
        game_name = dest.substr(1);
      } else {
        game_name = dest;
      }
    }

    if (event.get_event_owner() == currentUser) {
      return true; // Ignore echo from self
    }

    if (!game_name.empty()) {
      gameManager.update(game_name, event, event.get_event_owner());
    }
  } else if (command == "RECEIPT") {
    if (headers.count("receipt-id")) {
      int receiptId = stoi(headers["receipt-id"]);
      std::cout << "Receipt " << receiptId << " received" << std::endl;

      // Check for registered action
      std::lock_guard<std::mutex> lock(receiptMutex);
      if (receiptActions.count(receiptId)) {
        std::string msg = receiptActions[receiptId];
        if (!msg.empty()) {
          std::cout << msg << std::endl;
        }
        receiptActions.erase(receiptId);
      }
    }
  } else if (command == "ERROR") {
    std::string msg = "";
    if (headers.count("message")) {
      msg = headers["message"];
      std::cout << msg << std::endl;
    }

    // Strictly suppress body for specific assignment errors to match exact
    // output requirements
    bool isStandardError =
        (msg == "User already logged in" || msg == "Wrong password");

    // Only print body if it's not a standard error AND not identical to header
    if (!isStandardError && !body.empty() && body != msg &&
        body != msg + "\n") {
      std::cout << body << std::endl;
    }
    isConnected = false;
    // No need to clear actions explicitly; the protocol object is destroyed on
    // disconnect.
    return false;
  }

  return true;
}

void StompProtocol::addReceiptAction(int receiptId, std::string msg) {
  std::lock_guard<std::mutex> lock(receiptMutex);
  receiptActions[receiptId] = msg;
}
