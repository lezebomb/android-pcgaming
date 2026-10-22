package com.winlator.cmod.xserver.events;

import com.winlator.cmod.xconnector.XOutputStream;
import com.winlator.cmod.xconnector.XStreamLock;
import com.winlator.cmod.xserver.Window;
import java.io.IOException;

/** Core X11 event 21, including the coordinates relative to the new parent. */
public final class ReparentNotify extends Event {
    private final int event, window, parent;
    private final short x, y;
    private final boolean overrideRedirect;

    public ReparentNotify(Window event, Window window, Window parent) {
        super(21);
        this.event = event.id;
        this.window = window.id;
        this.parent = parent.id;
        x = window.getX(); y = window.getY();
        overrideRedirect = window.attributes.isOverrideRedirect();
    }

    @Override public void send(short sequenceNumber, XOutputStream outputStream) throws IOException {
        try (XStreamLock ignored = outputStream.lock()) {
            outputStream.writeByte(code);
            outputStream.writeByte((byte)0);
            outputStream.writeShort(sequenceNumber);
            outputStream.writeInt(event);
            outputStream.writeInt(window);
            outputStream.writeInt(parent);
            outputStream.writeShort(x);
            outputStream.writeShort(y);
            outputStream.writeByte((byte)(overrideRedirect ? 1 : 0));
            outputStream.writePad(11);
        }
    }
}
