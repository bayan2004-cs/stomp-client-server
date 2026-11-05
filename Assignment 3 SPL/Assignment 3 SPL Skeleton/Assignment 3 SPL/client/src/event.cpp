#include "../include/event.h"
#include "../include/json.hpp"
#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <vector>
#include <sstream>
using json = nlohmann::json;

Event::Event(std::string team_a_name, std::string team_b_name, std::string name, int time,
             std::map<std::string, std::string> game_updates, std::map<std::string, std::string> team_a_updates,
             std::map<std::string, std::string> team_b_updates, std::string discription)
    : team_a_name(team_a_name), team_b_name(team_b_name), name(name),
      time(time), game_updates(game_updates), team_a_updates(team_a_updates),
      team_b_updates(team_b_updates), description(discription)
{
}

Event::~Event()
{
}

const std::string &Event::get_team_a_name() const
{
    return this->team_a_name;
}

const std::string &Event::get_team_b_name() const
{
    return this->team_b_name;
}

const std::string &Event::get_name() const
{
    return this->name;
}

int Event::get_time() const
{
    return this->time;
}

const std::map<std::string, std::string> &Event::get_game_updates() const
{
    return this->game_updates;
}

const std::map<std::string, std::string> &Event::get_team_a_updates() const
{
    return this->team_a_updates;
}

const std::map<std::string, std::string> &Event::get_team_b_updates() const
{
    return this->team_b_updates;
}

const std::string &Event::get_discription() const
{
    return this->description;
}

const std::string &Event::get_event_owner() const
{
    return this->event_owner;
}

Event::Event(const std::string &frame_body) : team_a_name(""), team_b_name(""), name(""), time(0), game_updates(), team_a_updates(), team_b_updates(), description(""), event_owner("")
{
    std::stringstream ss(frame_body);
    std::string line;
    std::string current_map = "";

    while (std::getline(ss, line)) {
        // 1. Clean the line: Remove carriage return '\r' if present (common in network protocols)
        if (line.size() > 0 && line.back() == '\r') {
            line.pop_back();
        }

        // 2. Parse Standard Headers
        if (line.find("user:") == 0) {
             event_owner = line.substr(5);
             current_map = "";
        } else if (line.find("team a:") == 0) {
            team_a_name = line.substr(7); // Jump over "team a:" (7 chars)
            current_map = "";             // Reset map state
        } else if (line.find("team b:") == 0) {
            team_b_name = line.substr(7);
            current_map = "";
        } else if (line.find("event name:") == 0) {
            name = line.substr(11);
            current_map = "";
        } else if (line.find("time:") == 0) {
            time = std::stoi(line.substr(5));
            current_map = "";
        } 
        
        // 3. Detect Section Markers
        // These lines indicate we are entering a block of key-value pairs
        else if (line == "general game updates:") {
            current_map = "general";
        } else if (line == "team a updates:") {
            current_map = "team_a";
        } else if (line == "team b updates:") {
            current_map = "team_b";
        } else if (line == "description:") {
            current_map = "description";
        } 
        
        // 4. Parse Content (Key-Values or Description) based on current section
        else {
            if (current_map == "description") {
                // description is multi-line, so we append
                description += line + "\n";
            } else if (current_map != "") {
                // Parse "Key:Value" lines
                size_t colonPos = line.find(':');
                if (colonPos != std::string::npos) {
                    std::string key = line.substr(0, colonPos);
                    std::string value = line.substr(colonPos + 1);
                    
                    // Insert into the correct map based on our state variable
                    if (current_map == "general") {
                        game_updates[key] = value;
                    } else if (current_map == "team_a") {
                        team_a_updates[key] = value;
                    } else if (current_map == "team_b") {
                        team_b_updates[key] = value;
                    }
                }
            }
        }
    }
}


names_and_events parseEventsFile(std::string json_path)
{
    std::ifstream f(json_path);
    json data = json::parse(f);

    std::string team_a_name = data["team a"];
    std::string team_b_name = data["team b"];

    // run over all the events and convert them to Event objects
    std::vector<Event> events;
    for (auto &event : data["events"])
    {
        std::string name = event["event name"];
        int time = event["time"];
        std::string description = event["description"];
        std::map<std::string, std::string> game_updates;
        std::map<std::string, std::string> team_a_updates;
        std::map<std::string, std::string> team_b_updates;
        for (auto &update : event["general game updates"].items())
        {
            if (update.value().is_string())
                game_updates[update.key()] = update.value();
            else
                game_updates[update.key()] = update.value().dump();
        }

        for (auto &update : event["team a updates"].items())
        {
            if (update.value().is_string())
                team_a_updates[update.key()] = update.value();
            else
                team_a_updates[update.key()] = update.value().dump();
        }

        for (auto &update : event["team b updates"].items())
        {
            if (update.value().is_string())
                team_b_updates[update.key()] = update.value();
            else
                team_b_updates[update.key()] = update.value().dump();
        }
        
        events.push_back(Event(team_a_name, team_b_name, name, time, game_updates, team_a_updates, team_b_updates, description));
    }
    names_and_events events_and_names{team_a_name, team_b_name, events};

    return events_and_names;
}