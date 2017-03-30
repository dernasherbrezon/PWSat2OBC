import serial


class Pins:
    RESET = object()
    CLEAN = object()

    def __init__(self, com_port):
        self._com_port = com_port

        self._port = None
        while self._port is None:
            try:
                self._port = serial.Serial(self._com_port, rtscts=False, dsrdtr=False, baudrate=9600)
            except serial.SerialException as e:
                if not e.args[0].endswith("WindowsError(5, 'Access is denied.')"):
                    raise

    def high(self, pin):
        if pin == self.RESET:
            self._port.rts = False
        elif pin == self.CLEAN:
            self._port.dtr = False

    def low(self, pin):
        if pin == self.RESET:
            self._port.rts = True
        elif pin == self.CLEAN:
            self._port.dtr = True

    def close(self):
        self._port.close()
