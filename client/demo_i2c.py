#!/usr/bin/env python3
"""Read a sensor register via matrix and I2C. Adapt 0x44/reg for your sensor."""
import os
from jbox_api import MatrixClient
IP=os.getenv('JBOX_IP','192.168.1.80')
with MatrixClient(IP,os.environ['JBOX_KEY']) as j:
    # IO0=ESP GPIO1 -> H2-1 (node0); IO1=ESP GPIO2 -> H2-2 (node1).
    # H2/SDA and H2/SCL need EXTERNAL 3.3V pull-up resistors,
    # attached on the board, NOT direct rail links to MCU pins.
    j.route_set([[40,0],[41,1]])
    j.i2c_setup(0,sda=0,scl=1,hz=50000)
    print('I2C addresses:',[hex(a) for a in j.i2c_scan(0)])
    # Demo device transaction (uncomment after choosing a real device):
    # print(j.i2c_xfer(0,0x44,write=b'\x00',read=2).hex())
    j.i2c_close(0)
