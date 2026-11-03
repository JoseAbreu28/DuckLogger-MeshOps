class KeyboardLED:
    def __init__(self, uart: UART) -> None:
        # Standard 11-byte specific data frame for LED control (Cmd 0x12)
        # Byte 7 is the LED status bitmask
        self.uart = uart
        self.frame = bytearray([0x57, 0xAB, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x20])
        self.num_lock = False
        self.caps_lock = False
        self.scrol_llock = False
    
    def xor_seventh(self, mask: int) -> bytearray:
        self.frame[7] ^= mask
        return self.frame

    def toggle_numlock(self) -> bytearray:
        # Bit 0 = Num Lock
        self.xor_seventh(0x01) 
        self.num_lock = not self.num_lock
        return self.frame

    def toggle_capslock(self) -> bytearray:
        # Bit 1 = Caps Lock
        self.xor_seventh(0x02) 
        self.caps_lock = not self.caps_lock
        return self.frame

    def toggle_scrolllock(self) -> bytearray:
        # Bit 2 = Scroll Lock
        self.xor_seventh(0x04) 
        self.scrol_llock = not self.scrol_llock
        return self.frame

    def update_led(self, frame) -> None:
        key_codes = [ key_code for key_code in frame[5:]]
        for code in key_codes:
            if code == 0x39:
                self.toggle_capslock()
            elif code == 0x53:
                self.toggle_numlock()
            elif code == 0x57:
                self.toggle_scrolllock()
        self.uart.write(self.frame)


