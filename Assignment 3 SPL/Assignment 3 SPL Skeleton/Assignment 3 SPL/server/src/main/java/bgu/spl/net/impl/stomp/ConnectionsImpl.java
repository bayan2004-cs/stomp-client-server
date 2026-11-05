.package bgu.spl.net.impl.stomp;

import bgu.spl.net.srv.Connections;
import bgu.spl.net.srv.ConnectionHandler;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ConcurrentLinkedQueue;

public class ConnectionsImpl<T> implements Connections<T> {

    // Map: Connection ID -> Connection Handler
    private final ConcurrentHashMap<Integer, ConnectionHandler<T>> handlers = new ConcurrentHashMap<>();

    // Map: Channel Name -> List of Subscriptions
    private final ConcurrentHashMap<String, ConcurrentLinkedQueue<Subscription>> channelSubscribers = new ConcurrentHashMap<>();

    private static class Subscription {
        int connectionId;
        String subscriptionId;

        public Subscription(int connectionId, String subscriptionId) {
            this.connectionId = connectionId;
            this.subscriptionId = subscriptionId;
        }
    }

    public void addConnection(int connectionId, ConnectionHandler<T> handler) {
        handlers.put(connectionId, handler);
    }

    public void subscribe(String channel, int connectionId, String subscriptionId) {
        channelSubscribers.computeIfAbsent(channel, k -> new ConcurrentLinkedQueue<>())
                .add(new Subscription(connectionId, subscriptionId));
    }

    public void unsubscribe(String subscriptionId, int connectionId) {
        for (ConcurrentLinkedQueue<Subscription> subscribers : channelSubscribers.values()) {
            subscribers.removeIf(sub -> sub.subscriptionId.equals(subscriptionId) && sub.connectionId == connectionId);
        }
    }

    @Override
    public boolean send(int connectionId, T msg) {
        ConnectionHandler<T> handler = handlers.get(connectionId);
        if (handler == null)
            return false;

        handler.send(msg);
        return true;
    }

    @Override
    public void send(String channel, T msg) {
        ConcurrentLinkedQueue<Subscription> subscribers = channelSubscribers.get(channel);
        if (subscribers == null)
            return;

        for (Subscription sub : subscribers) {
            String frame = (String) msg;
            // Inject subscription header
            // Assuming frame has "\n\n" separating headers and body
            int bodyStart = frame.indexOf("\n\n");
            if (bodyStart != -1) {
                String newFrame = frame.substring(0, bodyStart) + "\nsubscription:" + sub.subscriptionId
                        + frame.substring(bodyStart);
                send(sub.connectionId, (T) newFrame);
            } else {
                // Fallback if formatting is weird
                send(sub.connectionId, msg);
            }
        }
    }

    public boolean isSubscribed(String channel, int connectionId) {
        ConcurrentLinkedQueue<Subscription> subscribers = channelSubscribers.get(channel);
        if (subscribers == null)
            return false;
        for (Subscription sub : subscribers) {
            if (sub.connectionId == connectionId)
                return true;
        }
        return false;
    }

    @Override
    public void disconnect(int connectionId) {
        handlers.remove(connectionId);
        for (ConcurrentLinkedQueue<Subscription> subscribers : channelSubscribers.values()) {
            subscribers.removeIf(sub -> sub.connectionId == connectionId);
        }
    }

}
