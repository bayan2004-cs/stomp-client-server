#pragma once

#include <string>
#include <map>
#include <vector>
#include <iostream>
#include <mutex>
#include <fstream>
#include <algorithm>
#include "event.h"

struct GameState {
    std::string team_a;
    std::string team_b;
    std::map<std::string, std::string> general_stats;
    std::map<std::string, std::string> team_a_stats;
    std::map<std::string, std::string> team_b_stats;
    
    // Changed from vector<string> to vector<Event>
    // Why: We need to print the description, time, and name separately in the summary file.
    // Storing the full object is much easier than parsing the string again later.
    std::vector<Event> events;
    
    GameState() : team_a(""), team_b("") {}
    GameState(std::string a, std::string b) : team_a(a), team_b(b) {}
};

class GameManager {
private:
    // Game Name -> User Name -> GameState
    std::map<std::string, std::map<std::string, GameState>> games;
    std::mutex gameMutex; 

public:
    GameManager() {}

    // Update method to handle incoming Events
    void update(std::string game_name, const Event& event, std::string user) {
        std::lock_guard<std::mutex> lock(gameMutex);
        
        // Ensure game/user state exists
        if (games[game_name].find(user) == games[game_name].end()) {
             games[game_name][user] = GameState(event.get_team_a_name(), event.get_team_b_name());
        }

        GameState& state = games[game_name][user];
        
        // Update stats maps from the event
        for (auto const& pair : event.get_game_updates()) {
            state.general_stats[pair.first] = pair.second;
        }
        for (auto const& pair : event.get_team_a_updates()) {
            state.team_a_stats[pair.first] = pair.second;
        }
        for (auto const& pair : event.get_team_b_updates()) {
            state.team_b_stats[pair.first] = pair.second;
        }
        
        state.events.push_back(event);
    }

    void writeGameSummaryToFile(const std::string& game_name, const std::string& user, const std::string& filename) {
        std::lock_guard<std::mutex> lock(gameMutex);
        
        if (games.find(game_name) == games.end() || games[game_name].find(user) == games[game_name].end()) {
            std::cerr << "No stats found for game: " << game_name << " and user: " << user << std::endl;
            return;
        }

        const GameState& state = games[game_name][user];
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Could not open file: " << filename << std::endl;
            return;
        }

        file << state.team_a << " vs " << state.team_b << "\n";
        file << "Game stats:\n";
        file << "General stats:\n";
        for (const auto& pair : state.general_stats) {
            file << pair.first << ": " << pair.second << "\n";
        }
        file << state.team_a << " stats:\n";
        for (const auto& pair : state.team_a_stats) {
            file << pair.first << ": " << pair.second << "\n";
        }
        file << state.team_b << " stats:\n";
        for (const auto& pair : state.team_b_stats) {
            file << pair.first << ": " << pair.second << "\n";
        }
        
        // Sort events by time/name before printing
        std::vector<Event> sortedEvents = state.events;
        std::sort(sortedEvents.begin(), sortedEvents.end(), [](const Event& a, const Event& b) {
            bool a_before_half = false;
            bool b_before_half = false;
            bool a_has_flag = false;
            bool b_has_flag = false;

            // Check flag for A
            if (a.get_game_updates().count("before halftime")) {
                std::string val = a.get_game_updates().at("before halftime");
                a_has_flag = true;
                a_before_half = (val == "true");
            }

            // Check flag for B
            if (b.get_game_updates().count("before halftime")) {
                std::string val = b.get_game_updates().at("before halftime");
                b_has_flag = true;
                b_before_half = (val == "true");
            }

            // Logic: strictly separating halves
            if (a_has_flag && b_has_flag) {
                if (a_before_half && !b_before_half) return true;  // A is 1st half, B is 2nd -> A comes first
                if (!a_before_half && b_before_half) return false; // B is 1st half, A is 2nd -> B comes first
            }
            
            // Fallback to normal time comparison
            return a.get_time() < b.get_time();
        });

        file << "Game event reports:\n";
        for (const auto& event : sortedEvents) {
            file << event.get_time() << " - " << event.get_name() << ":\n\n";
            file << event.get_discription() << "\n\n\n";
        }
        
        file.close();
    }
};