#!/usr/bin/env python3
"""Interop test: firmware Noise/packet code (C++ host build) vs an independent Python
implementation written from the Noise spec + bitchat wire format, using `cryptography`.
Usage: python3 interop_test.py <path-to-host_test-binary>"""
import os, struct, subprocess, sys, zlib, hashlib, hmac
from cryptography.hazmat.primitives.asymmetric.x25519 import X25519PrivateKey, X25519PublicKey
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey, Ed25519PublicKey
from cryptography.hazmat.primitives.ciphers.aead import ChaCha20Poly1305
from cryptography.hazmat.primitives import serialization

RAW = dict(encoding=serialization.Encoding.Raw, format=serialization.PublicFormat.Raw)
NAME = b"Noise_XX_25519_ChaChaPoly_SHA256"

class Proc:
    def __init__(s, path): s.p = subprocess.Popen([path], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
    def cmd(s, *a):
        s.p.stdin.write(" ".join(a) + "\n"); s.p.stdin.flush(); return s.p.stdout.readline().strip()

def pub(k): return k.public_key().public_bytes(**RAW)
def dh(k, p): return k.exchange(X25519PublicKey.from_public_bytes(p))
def hk(ck, ikm):
    t = hmac.new(ck, ikm, hashlib.sha256).digest()
    o1 = hmac.new(t, b"\x01", hashlib.sha256).digest()
    return o1, hmac.new(t, o1 + b"\x02", hashlib.sha256).digest()
def nonce(n): return b"\0" * 4 + struct.pack("<Q", n)

class HS:
    def __init__(s, initiator, static):
        s.i, s.s, s.e = initiator, static, X25519PrivateKey.generate()
        s.h = NAME; s.ck = NAME; s.k = None; s.n = 0; s.rs = s.re = None
        s.mix_hash(b"")
    def mix_hash(s, d): s.h = hashlib.sha256(s.h + d).digest()
    def mix_key(s, ikm): s.ck, s.k = hk(s.ck, ikm); s.n = 0
    def enc(s, p):
        if s.k is None: c = p
        else: c = ChaCha20Poly1305(s.k).encrypt(nonce(s.n), p, s.h); s.n += 1
        s.mix_hash(c); return c
    def dec(s, c):
        if s.k is None: p = c
        else: p = ChaCha20Poly1305(s.k).decrypt(nonce(s.n), c, s.h); s.n += 1
        s.mix_hash(c); return p
    def split(s):
        k1, k2 = hk(s.ck, b"")
        return (k1, k2) if s.i else (k2, k1)

def check(cond, what):
    print(("PASS " if cond else "FAIL ") + what)
    if not cond: sys.exit(1)

def noise_roundtrip(fw, fw_initiator):
    fw_static = os.urandom(32)
    fw_pub = bytes.fromhex(fw.cmd("static", fw_static.hex()))
    py_static = X25519PrivateKey.generate()
    py = HS(not fw_initiator, py_static)
    fw.cmd("init" if fw_initiator else "resp")
    if fw_initiator:
        m1 = bytes.fromhex(fw.cmd("write")); check(len(m1) == 32, "msg1 is 32 bytes")
        py.re = m1[:32]; py.mix_hash(py.re); py.dec(m1[32:])
        # msg2: e, ee, s, es
        m2 = pub(py.e); py.mix_hash(m2); py.mix_key(dh(py.e, py.re))
        m2 += py.enc(pub(py.s)); py.mix_key(dh(py.s, py.re)); m2 += py.enc(b"")
        check(len(m2) == 96, "msg2 is 96 bytes")
        check(fw.cmd("read", m2.hex()) == "ok", "firmware reads msg2")
        m3 = bytes.fromhex(fw.cmd("write")); check(len(m3) == 64, "msg3 is 64 bytes")
        py.rs = py.dec(m3[:48]); py.mix_key(dh(py.e, py.rs)); py.dec(m3[48:])
    else:
        m1 = pub(py.e); py.mix_hash(m1); m1 += py.enc(b"")
        check(fw.cmd("read", m1.hex()) == "ok", "firmware reads msg1")
        m2 = bytes.fromhex(fw.cmd("write")); check(len(m2) == 96, "firmware msg2 is 96 bytes")
        py.re = m2[:32]; py.mix_hash(py.re); py.mix_key(dh(py.e, py.re))
        py.rs = py.dec(m2[32:80]); py.mix_key(dh(py.e, py.rs)); py.dec(m2[80:])
        m3 = py.enc(pub(py.s)); py.mix_key(dh(py.s, py.re)); m3 += py.enc(b"")
        check(fw.cmd("read", m3.hex()) == "ok", "firmware reads msg3")
    check(py.rs == fw_pub, "python learned firmware static key")
    check(fw.cmd("split") == pub(py.s).hex(), "firmware learned python static key")
    send, recv = py.split()
    # firmware -> python transport frames: [u32 BE nonce][ct][tag]
    for i, msg in enumerate([b"\x01hello", b"x" * 300, b""]):
        ct = bytes.fromhex(fw.cmd("enc", msg.hex() if msg else "-"))
        n = struct.unpack(">I", ct[:4])[0]
        check(n == i, f"firmware nonce {i} on the wire")
        check(ChaCha20Poly1305(recv).decrypt(nonce(n), ct[4:], b"") == msg, f"python decrypts firmware frame {i}")
    for i, msg in enumerate([b"\x03id-1", b"\x01" + os.urandom(100)]):
        frame = struct.pack(">I", i) + ChaCha20Poly1305(send).encrypt(nonce(i), msg, b"")
        check(fw.cmd("dec", frame.hex()) == msg.hex(), f"firmware decrypts python frame {i}")
    check(fw.cmd("dec", frame.hex()) == "ERR", "firmware rejects a replayed frame")
    bad = bytearray(frame); bad[-1] ^= 1
    check(fw.cmd("dec", bytes(bad).hex()) == "ERR", "firmware rejects a tampered frame")

# ---- packet format (independent encoder) ----
def pad(b):
    for blk in (256, 512, 1024, 2048):
        if len(b) + 16 <= blk:
            n = blk - len(b)
            return b + bytes([n]) * n if 1 <= n <= 255 else b
    return b

def py_packet(typ, ts, sender, payload, ttl=7, sig_key=None, recipient=None, compress=False):
    flags = 0
    field = payload
    if compress:
        c = zlib.compressobj(9, zlib.DEFLATED, -15); raw = c.compress(payload) + c.flush()
        field = struct.pack(">H", len(payload)) + raw; flags |= 0x04
    if recipient: flags |= 0x01
    def body(ttl_, fl):
        b = struct.pack(">BBBQBH", 1, typ, ttl_, ts, fl, len(field)) + sender
        if recipient: b += recipient
        return b + field
    out = body(ttl, flags)
    if sig_key:
        sig = sig_key.sign(pad(body(0, flags)))
        out = body(ttl, flags | 0x02) + sig
    return pad(out)

def tlv(t, v): return bytes([t, len(v)]) + v

def main():
    fw = Proc(sys.argv[1])
    print("== Noise XX, firmware as initiator"); noise_roundtrip(fw, True)
    print("== Noise XX, firmware as responder"); noise_roundtrip(fw, False)

    print("== Signed public message (firmware -> python)")
    edk = os.urandom(32); text = "wake up, neo ✓".encode()
    frame, pubhex = fw.cmd("signmsg", edk.hex(), text.hex(), "1763650000000").split()
    frame = bytes.fromhex(frame)
    ver, typ, ttl, ts, flags, ln = struct.unpack(">BBBQBH", frame[:14])
    check((ver, typ, ttl, flags) == (1, 2, 7, 0x02), "header fields (v1, message, ttl 7, signed)")
    payload = frame[22:22 + ln]; sig = frame[22 + ln:22 + ln + 64]
    check(payload == text, "payload is raw UTF-8")
    signed = pad(struct.pack(">BBBQBH", 1, 2, 0, ts, 0, ln) + frame[14:22] + payload)
    Ed25519PublicKey.from_public_bytes(bytes.fromhex(pubhex)).verify(sig, signed)
    check(True, "python verifies Ed25519 signature over padded ttl=0 encoding")
    check(len(frame) == 256, "frame padded to 256-byte block")

    print("== Signed public message (python -> firmware)")
    sk = Ed25519PrivateKey.generate(); spub = sk.public_key().public_bytes(**RAW)
    f = py_packet(2, 1763650001234, b"\x11" * 8, b"hello from python", ttl=5, sig_key=sk)
    check(fw.cmd("verify", f.hex(), spub.hex()) == "ok " + b"hello from python".hex(), "firmware verifies + decodes")
    long_text = ("matrix " * 120).encode()
    f = py_packet(2, 1763650001234, b"\x11" * 8, long_text, ttl=3, sig_key=sk, compress=True)
    check(fw.cmd("verify", f.hex(), spub.hex()) == "ok " + long_text.hex(), "firmware inflates raw-DEFLATE payload + verifies")
    bad = bytearray(f); bad[30] ^= 0xFF
    check(fw.cmd("verify", bytes(bad).hex(), spub.hex()).startswith("ERR"), "firmware rejects tampered packet")

    print("== Announce TLV")
    nk = os.urandom(32)
    ann = tlv(1, b"trinity") + tlv(2, nk) + tlv(3, spub) + tlv(5, b"\x00\x01") + tlv(0x7E, b"future")
    f = py_packet(1, 1763650002000, b"\x22" * 8, ann, sig_key=sk)
    check(fw.cmd("announce", f.hex()) == "ok trinity " + nk.hex(), "announce decoded (unknown TLV skipped)")

    print("== BitchatFilePacket TLV")
    content = os.urandom(3000)
    fp = (b"\x01" + struct.pack(">H", 12) + b"img_test.jpg" + b"\x02" + struct.pack(">HI", 4, len(content)) +
          b"\x03" + struct.pack(">H", 10) + b"image/jpeg" + b"\x04" + struct.pack(">I", len(content)) + content)
    out = fw.cmd("file", fp.hex()); reenc = fw.p.stdout.readline().strip()
    check(out == f"ok img_test.jpg image/jpeg {len(content)}", "file packet decoded")
    check(bytes.fromhex(reenc) == fp, "file packet re-encodes byte-identically")
    print("\nALL TESTS PASSED")

main()
