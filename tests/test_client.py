import sys
import struct
import hmac
import hashlib
import unittest
import socket
import threading
from pathlib import Path
sys.path.insert(0,str(Path(__file__).parents[1]/'client'))
from jbox_api import *

KEY=bytes(range(32));NONCE=bytes(reversed(range(16)))

class FakeServer(threading.Thread):
    def __init__(self):
        super().__init__(daemon=True)
        self.s=socket.socket();self.s.bind(('127.0.0.1',0));self.s.listen(1)
        self.port=self.s.getsockname()[1]
        self.requests=[]
        self.error=None
    @staticmethod
    def read(fd,n):
        buf=bytearray()
        while len(buf)<n:
            x=fd.recv(n-len(buf))
            if not x:raise EOFError()
            buf+=x
        return bytes(buf)
    def run(self):
        try:
            conn,_=self.s.accept()
            with conn:
                session=hmac.new(KEY,NONCE+b'JMX-SESSION-v1',hashlib.sha256).digest()
                authed=False
                for _ in range(5):
                    h=self.read(conn,16)
                    mg,version,cmd,flags,seq,n=HEADER.unpack(h)
                    assert mg==MAGIC and version==1
                    data=self.read(conn,n)
                    if flags&F_MAC:
                        tag=self.read(conn,16)
                        assert hmac.compare_digest(tag,hmac.new(session,h+data,hashlib.sha256).digest()[:16])
                    if cmd==CMD_HELLO:r=NONCE
                    elif cmd==CMD_AUTH:
                        assert data==hmac.new(KEY,NONCE+b'JMX-AUTH-v1',hashlib.sha256).digest()
                        r=b'';authed=True
                    elif cmd==CMD_CAPS:
                        r=struct.pack('<BBBBHH',1,52,8,19,4096,4747)+bytes(GPIO_LIST)
                    elif cmd==CMD_ROUTE_CONNECT:
                        self.requests.append((cmd,data));r=bytes([3])+b'\0'*64
                    elif cmd==CMD_IO_READ:
                        self.requests.append((cmd,data));r=struct.pack('<I',5)
                    else:raise ValueError(cmd)
                    body=struct.pack('<i',0)+r
                    hdr=HEADER.pack(MAGIC,1,cmd,F_REPLY|(F_MAC if authed else 0),seq,len(body))
                    tag=hmac.new(session,hdr+body,hashlib.sha256).digest()[:16] if authed else b''
                    conn.sendall(hdr+body+tag)
        except Exception as exc:self.error=exc
        finally:self.s.close()

class TestAPI(unittest.TestCase):
    def test_auth_and_binary(self):
        srv=FakeServer();srv.start()
        with MatrixClient('127.0.0.1',KEY.hex(),srv.port) as c:
            self.assertEqual(c.caps()['io_count'],19)
            self.assertEqual(c.connect_nodes(40,0),3)
            self.assertEqual(c.gpio_read([0,2]),{0:1,2:1})
        srv.join(1)
        self.assertIsNone(srv.error)
        self.assertEqual(len(srv.requests),2)
if __name__=='__main__':unittest.main()
