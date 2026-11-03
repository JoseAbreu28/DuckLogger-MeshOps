import asyncio

class UARTBuffer:
    def __init__(self, uart: UART) -> None:
        self.uart = uart

    async def _async_read(self, n: int) -> bytes:
        while self.uart.any() < n:
            await asyncio.sleep(0) 
        return self.uart.read(n)

    async def get_frame(self) -> bytearray:
        while True:
            b = (await self._async_read(1))[0]
            if b != 0x57:
                continue

            b = (await self._async_read(1))[0]
            if b != 0xAB:
                continue

            b = (await self._async_read(1))[0]
            if b != 0x01:
                continue

            frame = bytearray(11)
            frame[0] = 0x57
            frame[1] = 0xAB
            frame[2] = 0x01

            frame[3:11] = await self._async_read(8)
            return frame
