#!/usr/bin/env python3
"""Examples on DIRECT breakout pins (no matrix, less signal loss)."""
import os
from jbox_api import MatrixClient
with MatrixClient(os.getenv('JBOX_IP','192.168.1.80'),os.environ['JBOX_KEY']) as j:
    # IO 9..18: direct ESP32-S3 GPIO, e.g. 10,18,21,38,39,40,...
    j.spi_setup(0,mosi=9,miso=10,sck=11,cs=12,mode=0,hz=1_000_000)
    print('SPI RX:',j.spi_xfer(0,b'\x9f\x00\x00\x00').hex())
    j.spi_close(0)
    # UART0 remains dedicated to programming/debug; hardware UART1 is available.
    j.uart_setup(0,tx=13,rx=14,baud=115200)
    j.uart_write(0,b'AT\r\n')
    print('UART RX:',repr(j.uart_read(0,512,200)))
    j.uart_close(0)
