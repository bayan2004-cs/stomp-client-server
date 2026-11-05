#pragma once

#include <string>
#include <map>
#include <mutex>

class SubscriptionManager {
private:
    std::map<std::string, int> channelToSubId;
    std::map<int, std::string> subIdToChannel;
    int subIdCounter;
    int receiptIdCounter;
    std::mutex mutex;

public:
    SubscriptionManager() : subIdCounter(0), receiptIdCounter(0) {}

    int getNextSubId() {
        std::lock_guard<std::mutex> lock(mutex);
        return subIdCounter++;
    }

    int getNextReceiptId() {
        std::lock_guard<std::mutex> lock(mutex);
        return receiptIdCounter++;
    }

    void addSubscription(const std::string& channel, int subId) {
        std::lock_guard<std::mutex> lock(mutex);
        channelToSubId[channel] = subId;
        subIdToChannel[subId] = channel;
    }

    void removeSubscription(const std::string& channel) {
        std::lock_guard<std::mutex> lock(mutex);
        if (channelToSubId.count(channel)) {
            int id = channelToSubId[channel];
            channelToSubId.erase(channel);
            subIdToChannel.erase(id);
        }
    }

    int getSubscriptionId(const std::string& channel) {
        std::lock_guard<std::mutex> lock(mutex);
        if (channelToSubId.count(channel)) {
            return channelToSubId[channel];
        }
        return -1;
    }
    
    bool isSubscribed(const std::string& channel) {
        std::lock_guard<std::mutex> lock(mutex);
        return channelToSubId.count(channel) > 0;
    }
};