import com.winlator.cmod.inputcontrols.FakeInputWriter;
import com.winlator.cmod.inputcontrols.GamepadState;
import java.nio.*;
import java.nio.file.*;

// One regression: a simultaneous full chord and its neutral release must remain whole frames.
public final class FakeInputWriterCheck {
    public static void main(String[] args) throws Exception {
        Path directory = Files.createTempDirectory(Path.of(args[0]), "chord-");
        FakeInputWriter writer = new FakeInputWriter(directory.toString(), 0);
        GamepadState held = new GamepadState();
        held.buttons = 0x3ff;
        held.thumbLX = held.thumbRX = 1;
        held.thumbLY = held.thumbRY = -1;
        held.triggerL = held.triggerR = 1;
        held.dpad[0] = held.dpad[1] = true;
        writer.writeGamepadState(held);
        writer.reset(); writer.close();
        byte[] bytes = Files.readAllBytes(directory.resolve("event0"));
        ByteBuffer events = ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN);
        int frames=0, pressed=0, released=0, axes=0;
        while (events.hasRemaining()) {
            events.getLong(); events.getLong();
            short type=events.getShort(), code=events.getShort(); int value=events.getInt();
            if (type==0 && code==0) frames++;
            if (type==1) { if (value==1) pressed++; else if (value==0) released++; }
            if (type==3) axes++;
            if (frames==1 && (type==1 || type==3) && value!=0) throw new AssertionError("Release frame retained input");
        }
        if (frames!=2 || pressed!=10 || released!=10 || axes!=16 || bytes.length!=1392)
            throw new AssertionError("Incomplete simultaneous chord/release frame");
        Files.delete(directory.resolve("event0")); Files.delete(directory);
        System.out.println("PASS simultaneous chord + neutral: 2 whole frames, 10 presses, 10 releases, 16 axis events, no overflow");
    }
}
