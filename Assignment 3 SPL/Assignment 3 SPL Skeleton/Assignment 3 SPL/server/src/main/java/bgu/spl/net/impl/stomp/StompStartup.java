package bgu.spl.net.impl.stomp;

import bgu.spl.net.srv.Connections;

public interface StompStartup<T> {
    void start(int connectionId, Connections<T> connections);
}
