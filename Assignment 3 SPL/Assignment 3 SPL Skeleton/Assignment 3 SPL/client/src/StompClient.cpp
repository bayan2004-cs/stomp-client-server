#include "../include/ConnectionHandler.h"
#include "../include/GameManager.h"
#include "../include/StompProtocol.h"
#include "../include/SubscriptionManager.h"
#include <atomic>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdlib.h>
#include <string>
#include <thread>
#include <vector>

// Global pointers to manage connection lifecycle
// Global pointers to manage connection lifecycle
std::unique_ptr<ConnectionHandler> connectionHandler;
std::unique_ptr<std::thread> socketThread;
std::atomic<bool> shouldTerminate{false};
std::string currentUser = ""; // Tracks logged in user

// Socket thread function
void socketThreadLoop(StompProtocol *protocol) {
  while (!shouldTerminate) {
    std::string answer;
    if (!connectionHandler->getFrameAscii(answer, '\0')) {
      // Socket error or disconnected
      std::cout << "Disconnected from server" << std::endl;
      shouldTerminate = true;
      protocol->setConnected(false);
      break;
    }

    // Only remove the last \n if it exists (getLine reads until delimiter but
    // buffer might need cleaning)
    int len = answer.length();
    if (len > 0 && answer[len - 1] == '\n')
      answer.resize(len - 1);

    if (!protocol->process(answer)) {
      // Protocol says we should disconnect or error occurred
      shouldTerminate = true;
    }
  }
}

// Cleanup helper
// Cleanup helper
void cleanupClient(StompProtocol *&protocol) {
  if (socketThread && socketThread->joinable()) {
    socketThread->join();
    socketThread.reset();
  }
  if (connectionHandler) {
    connectionHandler.reset();
  }
  if (protocol) {
    delete protocol;
    protocol = nullptr;
  }
  shouldTerminate = false;
}

int main(int argc, char *argv[]) {
  // Shared resources
  GameManager gameManager;
  SubscriptionManager subscriptionManager;
  StompProtocol *protocol = nullptr;

  // 1. Loop for user input
  while (1) {
    if (shouldTerminate) {
      cleanupClient(protocol);
    }

    const short bufsize = 1024;
    char buf[bufsize];
    std::cin.getline(buf, bufsize);
    std::string line(buf);

    // Check cleanup again after blocking I/O
    if (shouldTerminate) {
      cleanupClient(protocol);
    }

    // Split line into command and args
    std::vector<std::string> args;
    std::string current_arg;
    for (char c : line) {
      if (c == ' ') {
        if (!current_arg.empty()) {
          args.push_back(current_arg);
          current_arg = "";
        }
      } else {
        current_arg += c;
      }
    }
    if (!current_arg.empty())
      args.push_back(current_arg);

    if (args.empty())
      continue;

    std::string command = args[0];

    if (command == "login") {
      // Structure: login {host:port} {username} {password}
      if (connectionHandler != nullptr) {
        std::cout
            << "The client is already logged in, log out before trying again"
            << std::endl;
        continue;
      }
      if (args.size() < 4) {
        std::cout << "Usage: login {host:port} {username} {password}"
                  << std::endl;
        continue;
      }

      std::string hostPort = args[1];
      std::string username = args[2];
      std::string password = args[3];
      currentUser = username;

      // Parse host:port
      size_t colonPos = hostPort.find(':');
      if (colonPos == std::string::npos) {
        std::cout << "Invalid host:port format" << std::endl;
        continue;
      }
      std::string host = hostPort.substr(0, colonPos);
      short port = (short)stoi(hostPort.substr(colonPos + 1));

      // Create connection
      // Create connection
      connectionHandler.reset(new ConnectionHandler(host, port));
      if (!connectionHandler->connect()) {
        std::cout << "Could not connect to server" << std::endl;
        connectionHandler.reset();
        continue;
      }

      // Init protocol
      protocol = new StompProtocol(gameManager, subscriptionManager);
      protocol->setCurrentUser(username);

      // Start socket thread
      shouldTerminate = false;
      socketThread.reset(new std::thread(socketThreadLoop, protocol));

      // Send CONNECT frame
      std::stringstream ss;
      ss << "CONNECT" << "\n";
      ss << "accept-version:1.2" << "\n";
      ss << "host:stomp.cs.bgu.ac.il" << "\n";
      ss << "login:" << username << "\n";
      ss << "passcode:" << password << "\n";
      ss << "\n";

      if (!connectionHandler->sendFrameAscii(ss.str(), '\0')) {
        std::cout << "Failed to send CONNECT frame" << std::endl;
        shouldTerminate = true; // Signal thread to stop
      }
    } else if (command == "join") {
      if (connectionHandler == nullptr) {
        std::cout << "Not connected" << std::endl;
        continue;
      }
      if (args.size() < 2) {
        std::cout << "Usage: join {game_name}" << std::endl;
        continue;
      }
      std::string game_name = args[1];

      int subId = subscriptionManager.getNextSubId();
      int receiptId = subscriptionManager.getNextReceiptId();

      subscriptionManager.addSubscription(game_name, subId);

      std::stringstream ss;
      ss << "SUBSCRIBE" << "\n";
      ss << "destination:/" << game_name << "\n";
      ss << "id:" << subId << "\n";
      ss << "receipt:" << receiptId << "\n";
      ss << "\n";

      if (connectionHandler->sendFrameAscii(ss.str(), '\0')) {
        protocol->addReceiptAction(receiptId, "Joined channel " + game_name);
      } else {
        std::cout << "Could not join channel " << game_name << std::endl;
        subscriptionManager.removeSubscription(game_name);
      }
    } else if (command == "exit") {
      if (connectionHandler == nullptr) {
        std::cout << "Not connected" << std::endl;
        continue;
      }
      if (args.size() < 2) {
        std::cout << "Usage: exit {game_name}" << std::endl;
        continue;
      }
      std::string game_name = args[1];
      int subId = subscriptionManager.getSubscriptionId(game_name);
      if (subId == -1) {
        std::cout << "Not subscribed to " << game_name << std::endl;
        continue;
      }

      int receiptId = subscriptionManager.getNextReceiptId();

      std::stringstream ss;
      ss << "UNSUBSCRIBE" << "\n";
      ss << "id:" << subId << "\n";
      ss << "receipt:" << receiptId << "\n";
      ss << "\n";

      if (connectionHandler->sendFrameAscii(ss.str(), '\0')) {
        protocol->addReceiptAction(receiptId, "Exited channel " + game_name);
        subscriptionManager.removeSubscription(game_name);
      } else {
        std::cout << "Could not exit channel " << game_name << std::endl;
      }
    } else if (command == "logout") {
      if (connectionHandler == nullptr) {
        std::cout << "Not connected" << std::endl;
        continue;
      }
      int receiptId = subscriptionManager.getNextReceiptId();
      std::stringstream ss;
      ss << "DISCONNECT" << "\n";
      ss << "receipt:" << receiptId << "\n";
      ss << "\n";

      connectionHandler->sendFrameAscii(ss.str(), '\0');
    } else if (command == "report") {
      if (connectionHandler == nullptr) {
        std::cout << "Not connected" << std::endl;
        continue;
      }
      if (args.size() < 2) {
        std::cout << "Usage: report {file}" << std::endl;
        continue;
      }
      std::string file_path = args[1];

      names_and_events data;
      try {
        data = parseEventsFile(file_path);
      } catch (...) {
        std::cout << "Failed to parse file: " << file_path << std::endl;
        continue;
      }

      std::string game_name = data.team_a_name + "_" + data.team_b_name;

      for (const Event &event : data.events) {

        gameManager.update(game_name, event, currentUser);

        // 2. Send frame
        std::stringstream ss;
        ss << "SEND" << "\n";
        ss << "destination:/" << game_name << "\n";
        ss << "\n";

        // Body formatting according to PDF
        ss << "user:" << currentUser << "\n";

        ss << "team a:" << data.team_a_name << "\n";
        ss << "team b:" << data.team_b_name << "\n";
        ss << "event name:" << event.get_name() << "\n";
        ss << "time:" << event.get_time() << "\n";

        ss << "general game updates:" << "\n";
        for (auto const &pair : event.get_game_updates()) {
          ss << pair.first << ":" << pair.second << "\n";
        }

        ss << "team a updates:" << "\n";
        for (auto const &pair : event.get_team_a_updates()) {
          ss << pair.first << ":" << pair.second << "\n";
        }

        ss << "team b updates:" << "\n";
        for (auto const &pair : event.get_team_b_updates()) {
          ss << pair.first << ":" << pair.second << "\n";
        }

        ss << "description:" << "\n" << event.get_discription() << "\n";

        connectionHandler->sendFrameAscii(ss.str(), '\0');
      }
    } else if (command == "summary") {
      // summary {game_name} {user} {file}
      if (args.size() < 4) {
        std::cout << "Usage: summary {game_name} {user} {file}" << std::endl;
        continue;
      }
      std::string game_name = args[1];
      std::string user = args[2];
      std::string file_path = args[3];

      // Thread-safe logic using GameManager
      gameManager.writeGameSummaryToFile(game_name, user, file_path);
    } else {
      std::cout << "Unknown command" << std::endl;
    }
  }

  // Final cleanup if loop exits
  if (socketThread && socketThread->joinable()) {
    shouldTerminate = true;
    if (connectionHandler)
      connectionHandler->close();
    socketThread->join();
    socketThread.reset();
  }
  if (protocol)
    delete protocol;
  if (connectionHandler)
    connectionHandler.reset();

  return 0;
}