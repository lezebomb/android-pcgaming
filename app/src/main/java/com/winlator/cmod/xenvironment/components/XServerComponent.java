package com.winlator.cmod.xenvironment.components;

import com.winlator.cmod.xenvironment.EnvironmentComponent;
import com.winlator.cmod.xconnector.XConnectorEpoll;
import com.winlator.cmod.xconnector.UnixSocketConfig;
import com.winlator.cmod.xserver.XClientConnectionHandler;
import com.winlator.cmod.xserver.XClientRequestHandler;
import com.winlator.cmod.xserver.XServer;
import com.winlator.cmod.xserver.extensions.SyncExtension;

public class XServerComponent extends EnvironmentComponent {
    private XConnectorEpoll connector;
    private final XServer xServer;
    private final UnixSocketConfig socketConfig;

    public XServerComponent(XServer xServer, UnixSocketConfig socketConfig) {
        this.xServer = xServer;
        this.socketConfig = socketConfig;
    }

    @Override
    public void start() {
        if (connector != null) return;
        SyncExtension sync = xServer.getExtension(SyncExtension.MAJOR_OPCODE);
        if (sync != null) sync.open();
        connector = new XConnectorEpoll(socketConfig, new XClientConnectionHandler(xServer), new XClientRequestHandler());
        connector.setInitialInputBufferCapacity(262144);
        connector.setCanReceiveAncillaryMessages(true);
        connector.start();
    }

    @Override
    public void stop() {
        if (connector != null) {
            SyncExtension sync = xServer.getExtension(SyncExtension.MAJOR_OPCODE);
            if (sync != null) sync.close();
            connector.stop();
            connector = null;
        }
    }

    public XServer getXServer() {
        return xServer;
    }
}
