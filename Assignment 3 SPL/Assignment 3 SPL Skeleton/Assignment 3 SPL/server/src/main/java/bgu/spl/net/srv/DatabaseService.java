package bgu.spl.net.srv;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.Socket;
import java.nio.charset.StandardCharsets;

public class DatabaseService {

    private static final String DB_HOST = "127.0.0.1";
    private static final int DB_PORT = 7795;

    private static class SingletonHolder {
        private static final DatabaseService INSTANCE = new DatabaseService();
    }

    private Socket socket;
    private OutputStream out;
    private InputStream in;

    private DatabaseService() {
        connect();
    }

    private void connect() {
        try {
            socket = new Socket(DB_HOST, DB_PORT);
            out = socket.getOutputStream();
            in = socket.getInputStream();
        } catch (IOException e) {
            e.printStackTrace(); // Log error (in a real app, use a logger)
        }
    }

    public static String sendSQL(String command) {
        return SingletonHolder.INSTANCE.send(command);
    }

    private synchronized String send(String command) {
        try {
            if (socket == null || socket.isClosed() || !socket.isConnected()) {
                connect();
            }
            if (socket == null)
                return "Error: Could not connect to SQL server";

            // Send command terminated by null character
            out.write((command + "\u0000").getBytes(StandardCharsets.UTF_8));
            out.flush();

            // Read response terminated by null character
            StringBuilder response = new StringBuilder();
            int c;
            while ((c = in.read()) != -1) {
                if (c == '\u0000') {
                    break;
                }
                response.append((char) c);
            }
            return response.toString();

        } catch (IOException e) {
            e.printStackTrace();
            try {
                if (socket != null)
                    socket.close();
            } catch (IOException ignored) {
            }
            socket = null; // Force reconnect next time
            return "Error: Connection to SQL Server failed";
        }
    }
}