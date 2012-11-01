package de.entropia.can;

import java.io.IOException;
import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.lang.reflect.Method;

import de.entropia.can.CanSocket.CanFrame;
import de.entropia.can.CanSocket.CanId;
import de.entropia.can.CanSocket.CanInterface;
import de.entropia.can.CanSocket.Mode;

public class CanSocketTest {

    private static final String CAN_INTERFACE = "vcan0";
    
    @Retention(RetentionPolicy.RUNTIME)
    @Target({ElementType.METHOD})
    @interface Test { /* EMPTY */ }

    public static void main(String[] args) throws IOException {
	startTests();
//	miscTests();
    }
    
    @SuppressWarnings("unused")
    private static void miscTests() throws IOException {
	final CanId id = new CanId(0x30001).setEFFSFF();
	try (final CanSocket s = new CanSocket(Mode.RAW)) {
	    final CanInterface canif = new CanInterface(s, "slcan0");
	    s.bind(canif);
	    s.send(new CanFrame(canif, id, new byte[] {0,
		(byte) 0x91}));
	    System.out.println(s.recv());
	}
    }
    
    private static void startTests() {
        final CanSocketTest dummy = new CanSocketTest();
        boolean succeeded = true;
        for (Method testMethod : CanSocketTest.class.getMethods()) {
            if (testMethod.getAnnotation(Test.class) != null) {
                System.out.print("Test: " + testMethod.getName());
                try {
                    testMethod.invoke(dummy);
                    System.out.println(" OK");
                } catch (final Exception e) {
                    System.out.println(" FAILED");
                    e.printStackTrace();
                    succeeded = false;
                }
            }
        }
        if (!succeeded) {
            System.out.println("unit tests went wrong".toUpperCase());
            System.exit(-1);
        }
    }
    
    @Test
    public void testSocketCanRawCreate() throws IOException {
        new CanSocket(Mode.RAW).close();
    }

    @Test
    public void testSocketCanBcmCreate() throws IOException {
        new CanSocket(Mode.BCM).close();
    }
    
    @Test
    public void testFlags() {
        final CanId id_0 = new CanId(0);
        final CanId id_f = new CanId(0xffffffff);
        assert !id_0.isSetEFFSFF();
        assert !id_0.isSetRTR();
        assert !id_0.isSetERR();
        
        assert id_f.isSetEFFSFF();
        assert id_f.isSetRTR();
        assert id_f.isSetERR();
        
        id_0.setEFFSFF();
        assert id_0.isSetEFFSFF();
        id_0.clearEFFSFF();
        assert !id_0.isSetEFFSFF();
        
        id_0.setRTR();
        assert id_0.isSetRTR();
        id_0.clearRTR();
        assert !id_0.isSetRTR();
        
        id_0.setERR();
        assert id_0.isSetERR();
        id_0.clearERR();
        assert !id_0.isSetERR();
        
        assert id_0.getCanId_EFF() == 0x0;
    }
    
    @Test
    public void testInterface() throws IOException {
        try (final CanSocket socket = new CanSocket(Mode.RAW)) {
            new CanInterface(socket, CAN_INTERFACE);
        }
    }
    
    @Test
    public void testBindInterface() throws IOException {
        try (final CanSocket socket = new CanSocket(Mode.RAW)) {
            final CanInterface canIf = new CanInterface(socket, CAN_INTERFACE);
            socket.bind(canIf);
        }
    }
    
    @Test
    public void testBind2Interface() throws IOException {
        try (final CanSocket socket = new CanSocket(Mode.RAW)) {
            final CanInterface canIf = new CanInterface(socket, CAN_INTERFACE);
            socket.bind(CanSocket.CAN_ALL_INTERFACES);
            socket.bind(canIf);
        }
    }
    
    @Test
    public void testSend() throws IOException {
        try (final CanSocket socket = new CanSocket(Mode.RAW)) {
            final CanInterface canif = new CanInterface(socket, CAN_INTERFACE);
            socket.bind(canif);
            socket.send(new CanFrame(canif,
                    new CanId(0x5), new byte[] {0,0,0,0,0,0,0,0}));
        }
    }
    
//    @Test
    public void testRecv() throws IOException {
        try (final CanSocket socket = new CanSocket(Mode.RAW)) {
            socket.bind(CanSocket.CAN_ALL_INTERFACES);
            socket.recv();
        }
    }
    
    @Test
    public void testMtu() throws IOException {
	try (final CanSocket socket = new CanSocket(Mode.RAW)) {
	    final CanInterface canInterface =
		    new CanInterface(socket, CAN_INTERFACE);
	    socket.bind(canInterface);
	    final int mtu = socket.getMtu(CAN_INTERFACE);
	    if (!(mtu == CanSocket.CAN_MTU) && !(mtu == CanSocket.CAN_FD_MTU)) {
		throw new IOException("illegal interface mtu: " + mtu);
	    }
	}
    }

    @Test
    public void testSockOpts() throws IOException {
        try (final CanSocket socket = new CanSocket(Mode.RAW)) {
            socket.setLoopbackMode(true);
            assert socket.getLoopbackMode();
            socket.setRecvOwnMsgsMode(true);
            assert socket.getRecvOwnMsgsMode();
            socket.setRecvOwnMsgsMode(false);
            assert !socket.getRecvOwnMsgsMode();
            socket.setLoopbackMode(false);
            assert !socket.getLoopbackMode();
        }
    }
}
