package bgu.spl.net.impl.stomp;

import bgu.spl.net.api.MessagingProtocol;
import bgu.spl.net.srv.Connections;
import bgu.spl.net.srv.DatabaseService;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.atomic.AtomicInteger;
import java.time.LocalDateTime;
import java.time.format.DateTimeFormatter;

public class StompMessagingProtocolImpl implements StompStartup<String>, MessagingProtocol<String> {

    private int connectionId;
    private Connections<String> connections;
    private boolean shouldTerminate = false;
    private String currentLogin = null;
    private int loginRecordId = -1; // To track the ID in Logins table

    // Global map to track active sessions - REMOVED for Persistence Requirement.
    // DB is now the Source of Truth.
    // private static final Map<String, Boolean> activeUsers = new
    // ConcurrentHashMap<>();

    private static final AtomicInteger messageIdCounter = new AtomicInteger(0);

    @Override
    public void start(int connectionId, Connections<String> connections) {
        this.connectionId = connectionId;
        this.connections = connections;
    }

    @Override
    public String process(String message) {
        String[] lines = message.split("\n");
        if (lines.length == 0)
            return null;

        String command = lines[0].trim();
        Map<String, String> headers = new HashMap<>();

        // Quick parse for headers
        int i = 1;
        while (i < lines.length && !lines[i].isEmpty()) {
            String[] parts = lines[i].split(":", 2);
            if (parts.length >= 2) {
                headers.put(parts[0].trim(), parts[1].trim());
            }
            i++;
        }

        // Extract body (everything after the empty line)
        StringBuilder bodyBuilder = new StringBuilder();
        i++; // skip the empty line
        while (i < lines.length) {
            bodyBuilder.append(lines[i]).append("\n");
            i++;
        }
        String body = bodyBuilder.toString();

        int emptyLineIndex = message.indexOf("\n\n");
        if (emptyLineIndex != -1) {
            body = message.substring(emptyLineIndex + 2);
        } else {
            emptyLineIndex = message.indexOf("\r\n\r\n");
            if (emptyLineIndex != -1) {
                body = message.substring(emptyLineIndex + 4);
            } else {
                body = ""; // No body
            }
        }
        // Remove null char
        if (body.endsWith("\u0000")) {
            body = body.substring(0, body.length() - 1);
        }

        if ("CONNECT".equals(command)) {
            String version = headers.get("accept-version");
            if (!"1.2".equals(version)) {
                sendError("Version mismatch", "Supported version is 1.2");
                return null;
            }

            String login = headers.get("login");
            String passcode = headers.get("passcode");

            if (login == null || passcode == null) {
                sendError("MalFormed Frame", "Missing login or passcode headers");
                return null;
            }

            // 1. Check if user already logged in (Persistence Fix)
            // Query DB: Is there a login record with LogoutTime IS NULL?
            String activeSession = DatabaseService
                    .sendSQL("SELECT ID FROM Logins WHERE Username='" + login + "' AND LogoutTime IS NULL;");
            if (activeSession != null && activeSession.trim().length() > 0 && !activeSession.startsWith("Error")) {
                sendError("User already logged in", "User " + login + " is already logged in");
                return null;
            }

            // 2. Query DB for User
            // Expecting response: "password" OR empty if not found
            String existingPass = DatabaseService.sendSQL("SELECT Password FROM Users WHERE Username='" + login + "';");
            existingPass = existingPass.trim();

            if (existingPass.startsWith("Error")) {
                sendError("Database Error", "Could not verify user");
                return null;
            }

            if (existingPass.isEmpty()) {
                // New User -> Register
                String result = DatabaseService.sendSQL(
                        "INSERT INTO Users (Username, Password) VALUES ('" + login + "', '" + passcode + "');");
                if (result.startsWith("Error")) {
                    sendError("Database Error", "Registration failed");
                    return null;
                }
                existingPass = passcode; // Auto-login
            }

            // 3. Verify Verify Password
            if (!existingPass.equals(passcode)) {
                sendError("Wrong password", "Password for user " + login + " is incorrect");
                return null;
            }

            // 4. Log the Login event
            String timestamp = LocalDateTime.now().format(DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm:ss"));
            String result = DatabaseService
                    .sendSQL("INSERT INTO Logins (Username, LoginTime) VALUES ('" + login + "', '" + timestamp + "');");
            // Ideally we get the ID back, but for now we assume simple logging.

            // Success
            // activeUsers.put(login, true); // Removed
            currentLogin = login;
            connections.send(connectionId, "CONNECTED\nversion:1.2\n\n\u0000");

        } else if ("SUBSCRIBE".equals(command)) {
            String destination = headers.get("destination");
            String id = headers.get("id");

            if (destination == null || id == null) {
                sendError("MalFormed Frame", "Missing destination or id header");
                return null;
            }

            try {
                int subId = Integer.parseInt(id);
                ((ConnectionsImpl<String>) connections).subscribe(destination, connectionId, String.valueOf(subId));
                handleReceipt(headers);
            } catch (NumberFormatException e) {
                sendError("MalFormed Frame", "id header must be an integer");
                return null;
            }
        } else if ("UNSUBSCRIBE".equals(command)) {
            String id = headers.get("id");
            if (id == null) {
                sendError("MalFormed Frame", "Missing id header");
                return null;
            }

            try {
                int subId = Integer.parseInt(id);
                ((ConnectionsImpl<String>) connections).unsubscribe(String.valueOf(subId), connectionId);
                handleReceipt(headers);
            } catch (NumberFormatException e) {
                sendError("MalFormed Frame", "id header must be an integer");
                return null;
            }
        } else if ("SEND".equals(command)) {
            String destination = headers.get("destination");
            if (destination == null) {
                sendError("MalFormed Frame", "Missing destination header");
                return null;
            }

            try {
                boolean isSubscribed = ((ConnectionsImpl<String>) connections).isSubscribed(destination, connectionId);
                if (!isSubscribed) {
                    sendError("Not Subscribed", "Client is not subscribed to " + destination);
                    return null;
                }
            } catch (Exception e) {
                // ConnectionsImpl cast might be unsafe if using Reactor's other impl, but
                // usually safe in SPL
            }

            // File Tracking Logic
            String[] bodyLines = body.split("\n");
            String filename = null;
            for (String line : bodyLines) {
                if (line.startsWith("file:")) {
                    filename = line.substring(5).trim();
                    break;
                }
            }

            if (filename != null && currentLogin != null) {
                String timestamp = java.time.format.DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm:ss")
                        .format(java.time.LocalDateTime.now());
                DatabaseService.sendSQL("INSERT INTO Files (Username, Filename, UploadTime) VALUES ('" + currentLogin
                        + "', '" + filename + "', '" + timestamp + "');");
            }

            int msgId = messageIdCounter.incrementAndGet();

            String frame = "MESSAGE\n" +
                    "destination:" + destination + "\n" +
                    "message-id:" + msgId + "\n" +
                    "\n" +
                    body + "\u0000";

            connections.send(destination, frame);
            handleReceipt(headers);
        } else if ("DISCONNECT".equals(command)) {
            String receipt = headers.get("receipt");
            if (receipt != null) {
                connections.send(connectionId, "RECEIPT\nreceipt-id:" + receipt + "\n\n\u0000");
            }
            if (currentLogin != null) {
                // activeUsers.remove(currentLogin); // Removed

                // Update Logout time
                String timestamp = LocalDateTime.now().format(DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm:ss"));
                // We update the active session (LogoutTime IS NULL)
                String sql = "UPDATE Logins SET LogoutTime='" + timestamp
                        + "' WHERE Username='" + currentLogin + "' AND LogoutTime IS NULL;";
                DatabaseService.sendSQL(sql);
            }
            connections.disconnect(connectionId);
            shouldTerminate = true;
        } else {
            handleReceipt(headers);
        }
        return null;
    }

    private void handleReceipt(Map<String, String> headers) {
        String receipt = headers.get("receipt");
        if (receipt != null) {
            connections.send(connectionId, "RECEIPT\nreceipt-id:" + receipt + "\n\n\u0000");
        }
    }

    private void sendError(String message, String description) {
        String frame = "ERROR\n" +
                "message:" + message + "\n" +
                "\n" +
                description + "\u0000";
        connections.send(connectionId, frame);
        connections.disconnect(connectionId);
        shouldTerminate = true;
        // activeUsers removing handled by DB state naturally (LogoutTime remains NULL
        // until fixed or proper logout)
        // But if error happens, we are disconnecting. Should we mark logout?
        if (currentLogin != null) {
            String timestamp = LocalDateTime.now().format(DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm:ss"));
            DatabaseService.sendSQL("UPDATE Logins SET LogoutTime='" + timestamp + "' WHERE Username='" + currentLogin
                    + "' AND LogoutTime IS NULL;");
        }
    }

    @Override
    public boolean shouldTerminate() {
        return shouldTerminate;
    }

    /**
     * Helper function to generate and print a server-side report using SQL queries.
     * Checks: Registered users, Login history, Uploaded files.
     */
    public void printServerStats() {
        System.out.println("---- Server Stats Report ----");
        System.out.println("1. Registered Users:");
        System.out.println(DatabaseService.sendSQL("SELECT * FROM Users;"));
        System.out.println("\n2. Login History:");
        System.out.println(DatabaseService.sendSQL("SELECT * FROM Logins;"));
        System.out.println("\n3. Uploaded Files:");
        System.out.println(DatabaseService.sendSQL("SELECT * FROM Files;"));
        System.out.println("-----------------------------");
    }
}