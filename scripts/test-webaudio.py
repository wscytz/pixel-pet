#!/usr/bin/env python3
# 模拟浏览器插件,往 ws://127.0.0.1:17632 推假频谱,验证 PixelPet C++ 端收到并驱动。
# 纯标准库,无依赖。用法:python3 scripts/test-webaudio.py
import socket, json, os, base64, struct, math, time, sys

HOST, PORT = '127.0.0.1', 17632

s = socket.create_connection((HOST, PORT))
key = base64.b64encode(os.urandom(16)).decode()
req = (f"GET / HTTP/1.1\r\nHost: {HOST}:{PORT}\r\nUpgrade: websocket\r\n"
       f"Connection: Upgrade\r\nSec-WebSocket-Key: {key}\r\n"
       f"Sec-WebSocket-Version: 13\r\n\r\n")
s.send(req.encode())
resp = s.recv(4096)
if b'101' not in resp:
    print('握手失败:', resp[:120]); sys.exit(1)
print('[test] ws 已连接 PixelPet')

def send_frame(payload: bytes):
    header = bytearray([0x81])  # FIN + text
    mask = os.urandom(4)
    n = len(payload)
    if n < 126:
        header.append(0x80 | n)
    elif n < 65536:
        header.append(0x80 | 126); header += struct.pack('>H', n)
    else:
        header.append(0x80 | 127); header += struct.pack('>Q', n)
    header += mask
    masked = bytes(b ^ mask[i % 4] for i, b in enumerate(payload))
    s.send(bytes(header) + masked)

t = 0.0
try:
    while True:
        spec = []
        for i in range(128):
            env = 1.0 - i / 128 * 0.6
            v = (0.5 + 0.5 * math.sin(t * 2 + i * 0.3)) * env
            spec.append(int(max(0, min(255, v * 255))))
        rms = 0.3 + 0.2 * math.sin(t)
        send_frame(json.dumps({"spectrum": spec, "rms": rms, "title": "test song"}).encode())
        t += 0.1
        time.sleep(0.033)
except KeyboardInterrupt:
    pass
