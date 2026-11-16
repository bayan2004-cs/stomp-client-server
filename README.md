[README.md](https://github.com/user-attachments/files/34918508/README.md)
# World Cup 2026 Informer

A community-driven World Cup 2026 match updates application built with a **Java STOMP server** and a **C++ client**. Users can connect to the server, subscribe to individual match channels, publish match reports, receive updates from other subscribers, and generate summaries of reported match events.

The server supports two connection-handling models—**Thread-Per-Client (TPC)** and **Reactor**—and uses **SQLite** for persistent user, login-session, and uploaded-report tracking.

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Architecture](#architecture)
- [STOMP Protocol](#stomp-protocol)
- [Client Commands](#client-commands)
- [Game Events and Summaries](#game-events-and-summaries)
- [Database Integration](#database-integration)
- [Project Structure](#project-structure)
- [Requirements](#requirements)
- [Build and Run](#build-and-run)
- [Example Usage](#example-usage)

## Overview

World Cup 2026 Informer allows users to share live-style match updates through topic-based messaging.

Each match has its own channel. A user can join a channel to receive updates, report events from a JSON file, and create a summary of the events and statistics reported by a particular user. Reports are distributed by the server to all clients subscribed to the relevant match channel.

The project consists of:

- A **Java server** implementing the STOMP messaging service.
- A **C++ command-line client** through which users interact with the service.
- A **SQLite database integration** for storing user registration, login/logout history, and uploaded report filenames.

## Features

- User registration on first successful login.
- User authentication using a username and password.
- Prevention of simultaneous active logins for the same user.
- Match-specific topic subscriptions.
- Publishing match events to a channel and receiving events from other subscribers.
- Reading match events from JSON files.
- Tracking game statistics and event reports separately for each reporting user.
- Generating match summaries for a selected game and user.
- Graceful logout using STOMP receipts.
- Two server concurrency models:
  - Thread-Per-Client (TPC)
  - Reactor
- SQLite-backed tracking of users, login sessions, logout times, and uploaded report filenames.
- A database report that retrieves and prints the stored information using SQL queries.

## Architecture

### Server

The server is implemented in Java and provides the STOMP messaging service. It can be started in either TPC or Reactor mode.

The server is divided conceptually into two parts:

1. **Server pattern / connection management**
   - Manages active client connections.
   - Associates each active connection with a unique connection ID.
   - Sends messages to a specific client or to all clients subscribed to a channel.
   - Disconnects clients and removes their active connections.

2. **STOMP protocol implementation**
   - Processes frames received from clients.
   - Handles connection, subscription, message publishing, unsubscription, and disconnection.
   - Uses the connections abstraction to send responses and broadcast messages.

The server's messaging layer is independent of the selected server pattern. Both TPC and Reactor use the same STOMP protocol behavior.

### Client

The client is implemented in C++. It provides a terminal-based interface for users to enter commands.

The client uses two threads:

- **Keyboard thread:** reads and processes commands entered by the user.
- **Socket thread:** reads incoming frames and messages from the server.

This allows the client to continue receiving match updates while the user is entering commands.

### Database service

The server communicates with a Python SQL service that executes SQL statements against an SQLite database. The Java server sends SQL operations to this service for the required persistence and reporting tasks.

## STOMP Protocol

Communication between the client and server uses **STOMP 1.2 (Simple Text Oriented Messaging Protocol)** over a TCP connection.

A STOMP frame consists of:

1. A command line.
2. Zero or more headers in `key:value` format.
3. A blank line separating the headers from the body.
4. A body, which may be empty.
5. A null-character terminator (`\0`).

### Supported server frames

| Frame | Purpose |
|---|---|
| `CONNECTED` | Confirms a successful client connection. |
| `MESSAGE` | Delivers a message published to a subscribed channel. |
| `RECEIPT` | Acknowledges successful processing of a frame requesting a receipt. |
| `ERROR` | Reports a protocol or processing error. |

### Supported client frames

| Frame | Purpose |
|---|---|
| `CONNECT` | Connects and authenticates a client. |
| `SEND` | Publishes a message to a destination channel. |
| `SUBSCRIBE` | Subscribes to a channel. |
| `UNSUBSCRIBE` | Removes a subscription. |
| `DISCONNECT` | Requests a graceful disconnection. |

A client must be subscribed to a channel before it can publish messages to that channel. Messages sent to a channel are forwarded to its subscribed clients.

## Client Commands

All commands except `login` require the user to be logged in.

### Login

```text
login {host:port} {username} {password}
```

Connects to the server and authenticates the user. If the username is new, the server registers it with the supplied password. Existing users must provide the correct password, and a user who is already logged in cannot establish another active session.

On success, the client displays:

```text
Login successful
```

### Join a match channel

```text
join {game_name}
```

Subscribes to a match channel. The game name is formed from the two team names:

```text
{team_a_name}_{team_b_name}
```

For example:

```text
join usa_mexico
```

After the subscription is processed and acknowledged, the client displays:

```text
Joined channel usa_mexico
```

The client then receives messages published to that channel.

### Exit a match channel

```text
exit {game_name}
```

Unsubscribes from the specified match channel.

Example:

```text
exit usa_mexico
```

After the unsubscription is acknowledged, the client displays:

```text
Exited channel usa_mexico
```

### Report match events

```text
report {file}
```

Reads a game-events JSON file, extracts the teams and events, saves the events locally as reports by the currently logged-in user, and publishes each event to the corresponding match channel.

The game name is derived from the JSON file's `team a` and `team b` fields. Each published event includes the reporter's username, the teams, the event name, the event time, the general and team-specific updates, and the event description.

The client sends each event as a STOMP `SEND` frame to the match channel.

### Summarize a game

```text
summary {game_name} {user} {file}
```

Creates a summary file containing the updates received for the specified match from the specified user.

The summary includes:

- The two teams playing.
- General game statistics.
- Team A statistics.
- Team B statistics.
- Game event reports, ordered by their game time.

Statistics are printed in lexicographical order by statistic name. If the output file does not exist, it is created; if it already exists, its contents are overwritten.

### Logout

```text
logout
```

Sends a STOMP `DISCONNECT` frame with a receipt request. The client waits for the matching `RECEIPT`, closes the socket, and returns to the command prompt. The server removes the client from its active subscriptions and records the logout time.

## Game Events and Summaries

Game events are read from JSON files. A game-events file contains the names of the two teams and an array of events.

A typical event has the following fields:

| Field | Description |
|---|---|
| `event name` | Name of the event, such as kickoff or goal. |
| `time` | Time of the event in seconds from the start of the game. |
| `general game updates` | Statistics or updates that apply to the game as a whole. |
| `team a updates` | Statistics or updates for the first team. |
| `team b updates` | Statistics or updates for the second team. |
| `description` | Text describing the event. |

The client maintains updates separately for each reporter. When a new update for an existing statistic is received, the stored value for that statistic is updated. New statistics are added when they are first encountered.

Event reports in a summary are ordered by their game time. The implementation also accounts for the halftime indicator when ordering events around the transition between halves, since first-half stoppage time can produce event times that are greater than the nominal start time of the second half.

### Example game-events JSON structure

```json
{
  "team a": "USA",
  "team b": "Canada",
  "events": [
    {
      "event name": "kickoff",
      "time": 0,
      "general game updates": {
        "active": true,
        "before halftime": true
      },
      "team a updates": {},
      "team b updates": {},
      "description": "The game has started!"
    },
    {
      "event name": "goal",
      "time": 1980,
      "general game updates": {},
      "team a updates": {
        "goals": "1",
        "possession": "90%"
      },
      "team b updates": {
        "possession": "10%"
      },
      "description": "USA take the lead!"
    }
  ]
}
```

The game channel for this example is `usa_canada`.

## Database Integration

The server uses SQLite through the provided Python SQL service. The Java server sends SQL commands and queries to the service, which executes them against the database and returns the operation result.

The database stores at least the following information:

- **Users:** a record for each registered username.
- **Login history:** a record for each successful login, including the username and login date/time.
- **Logout time:** the corresponding login record is updated with the logout date/time when the user disconnects.
- **Uploaded files:** every filename uploaded through the `report` command, associated with the username.

The server also provides a report function that uses SQL queries to print:

- All registered users.
- Each user's login history, including login and logout times.
- The filenames uploaded by each user through the report command.

The report is based on database query results rather than scanning in-memory application structures.

## Project Structure

The project is organized into separate server and client directories:

```text
.
├── Server/
│   ├── pom.xml
│   └── src/
│       └── ...
│
├── Client/
│   ├── src/
│   ├── include/
│   ├── bin/
│   └── makefile
│
└── README.md
```

The server is built with Maven. The client is built with the provided Makefile.

## Requirements

- Java Development Kit (JDK)
- Apache Maven
- C++ compiler with C++11 support
- `make`
- Python 3
- SQLite support for the Python SQL service
- Linux environment (the assignment is intended to be built and tested in the course Linux labs or VM)

## Build and Run

Run the following commands from the relevant project directories.

### Build the server

```bash
cd Server
mvn compile
```

### Start the server in TPC mode

Replace `<port>` with the port on which the server should listen:

```bash
mvn exec:java \
  -Dexec.mainClass="bgu.spl.net.impl.stomp.StompServer" \
  -Dexec.args="<port> tpc"
```

### Start the server in Reactor mode

```bash
mvn exec:java \
  -Dexec.mainClass="bgu.spl.net.impl.stomp.StompServer" \
  -Dexec.args="<port> reactor"
```

### Build the C++ client

```bash
cd Client
make
```

The client executable is produced at:

```text
Client/bin/StompWCIClient
```

### Run the client

```bash
./bin/StompWCIClient
```

After starting the client, use the `login` command with the server address, port, username, and password. Once logged in, the other client commands become available.

> Start the Python SQL service as required by the project setup so that the Java server can send it database operations. The exact launch command depends on the location and name of the supplied Python service script in the project files.

## Example Usage

The following is an example command sequence in the client:

```text
login 127.0.0.1:7777 meni films
join usa_canada
report data/events1_partial.json
summary usa_canada meni summary.txt
exit usa_canada
logout
```

This sequence:

1. Connects to the server and logs in.
2. Subscribes to the USA–Canada match channel.
3. Reads and publishes the events from the JSON file.
4. Writes a summary of the selected user's updates to `summary.txt`.
5. Leaves the match channel.
6. Logs out gracefully.

To receive reports from other users, those users must also be connected and subscribed to the same match channel.

---

## Academic Context

This project was developed as **Assignment 3: World Cup 2026 Informer** for the SPL course. It applies network programming, concurrent server design, protocol implementation, client-server communication, and relational database integration.

