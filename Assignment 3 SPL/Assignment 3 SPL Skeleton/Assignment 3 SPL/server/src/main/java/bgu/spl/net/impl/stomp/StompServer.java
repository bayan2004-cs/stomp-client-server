package bgu.spl.net.impl.stomp;

import bgu.spl.net.srv.Server;

public class StompServer {

    public static void main(String[] args) {
        if (args.length < 2) {
            System.out.println("Usage: StompServer <port> <server_type>");
            System.out.println("       server_type: tpc | reactor");
            return;
        }

        int port = Integer.parseInt(args[0]);
        String serverType = args[1];

        if (serverType.equals("tpc")) {
            Server.<String>threadPerClient(
                    port,
                    () -> new StompMessagingProtocolImpl(), // Protocol factory
                    () -> new StompEncoderDecoder() // Encoder factory
            ).serve();
        } else if (serverType.equals("reactor")) {
            Server.<String>reactor(
                    Runtime.getRuntime().availableProcessors(),
                    port,
                    () -> new StompMessagingProtocolImpl(), // Protocol factory
                    () -> new StompEncoderDecoder() // Encoder factory
            ).serve();
        } else {
            System.out.println("Unknown server type: " + serverType);
        }
    }
}
