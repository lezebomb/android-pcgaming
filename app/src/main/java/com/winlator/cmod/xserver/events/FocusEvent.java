package com.winlator.cmod.xserver.events;

import com.winlator.cmod.xconnector.XOutputStream;
import com.winlator.cmod.xconnector.XStreamLock;
import com.winlator.cmod.xserver.Window;
import java.io.IOException;

/** Core X11 FocusIn/FocusOut, for normal (non-keyboard-grab) transitions. */
public final class FocusEvent extends Event {
    public enum Detail {ANCESTOR, VIRTUAL, INFERIOR, NONLINEAR, NONLINEAR_VIRTUAL, POINTER, POINTER_ROOT, NONE}
    private final Window window;
    private final Detail detail;

    public FocusEvent(boolean entering, Window window, Detail detail) {
        super(entering ? 9 : 10);
        this.window = window;
        this.detail = detail;
    }

    @Override
    public void send(short sequenceNumber, XOutputStream outputStream) throws IOException {
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(code);
            outputStream.writeByte((byte)detail.ordinal());
            outputStream.writeShort(sequenceNumber);
            outputStream.writeInt(window.id);
            outputStream.writeByte((byte)0); // NotifyNormal
            outputStream.writePad(23);
        }
    }
}
