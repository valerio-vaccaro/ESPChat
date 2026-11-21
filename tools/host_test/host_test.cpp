// Line-oriented driver around the firmware's Noise + packet code (see interop_test.py).
#include <iostream>
#include <string>
#include <string.h>
#include "../../src/crypto/noise.h"
#include "../../src/crypto/crypto_util.h"
#include "../../src/proto/packet.h"
#include "../../src/proto/payloads.h"
#include <Ed25519.h>

static Bytes hx(const std::string& s) { Bytes b(s.size() / 2); fromHex(s, b.data(), b.size()); return b; }
static std::string hs(const Bytes& b) { return toHex(b.data(), b.size()); }

int main() {
  HandshakeState h;
  NoiseSession sess;
  uint8_t spriv[32], spub[32];
  std::string cmd;
  while (std::cin >> cmd) {
    if (cmd == "static") {           // static <priv32hex> -> pub
      std::string k; std::cin >> k; Bytes b = hx(k); memcpy(spriv, b.data(), 32);
      x25519Public(spub, spriv); std::cout << toHex(spub, 32) << std::endl;
    } else if (cmd == "init" || cmd == "resp") {
      sess.reset(); sess.hs.init(cmd == "init", spriv, spub); std::cout << "ok" << std::endl;
    } else if (cmd == "write") {
      Bytes m; bool ok = sess.hs.writeMessage(m); std::cout << (ok ? hs(m) : "ERR") << std::endl;
    } else if (cmd == "read") {
      std::string m; std::cin >> m; Bytes b = hx(m);
      std::cout << (sess.hs.readMessage(b.data(), b.size()) ? "ok" : "ERR") << std::endl;
    } else if (cmd == "split") {
      sess.establish(); std::cout << toHex(sess.remoteStatic, 32) << std::endl;
    } else if (cmd == "enc") {
      std::string m; std::cin >> m; Bytes p = hx(m == "-" ? "" : m), out;
      std::cout << (sess.encrypt(p.data(), p.size(), out) ? hs(out) : "ERR") << std::endl;
    } else if (cmd == "dec") {
      std::string m; std::cin >> m; Bytes c = hx(m), out;
      bool ok = sess.decrypt(c.data(), c.size(), out); std::cout << (ok ? (out.empty() ? "-" : hs(out)) : "ERR") << std::endl;
    } else if (cmd == "signmsg") {   // signmsg <edpriv> <text-hex> <ts> -> frame
      std::string k, t; uint64_t ts; std::cin >> k >> t >> ts;
      Bytes kp = hx(k); uint8_t pub[32]; Ed25519::derivePublicKey(pub, kp.data());
      bc::Packet p; p.type = bc::MT_MESSAGE; p.timestamp = ts; memset(p.sender, 0xAB, 8);
      p.payload = hx(t); Bytes sb = bc::signingBytes(p);
      Ed25519::sign(p.signature, kp.data(), pub, sb.data(), sb.size()); p.hasSignature = true;
      std::cout << hs(bc::encode(p, true)) << " " << toHex(pub, 32) << std::endl;
    } else if (cmd == "verify") {    // verify <frame> <edpub> -> ok/ERR + payload
      std::string f, k; std::cin >> f >> k; Bytes fb = hx(f), kb = hx(k);
      bc::Packet p; if (!bc::decode(fb.data(), fb.size(), p)) { std::cout << "ERR decode" << std::endl; continue; }
      Bytes sb = bc::signingBytes(p);
      bool ok = p.hasSignature && Ed25519::verify(p.signature, kb.data(), sb.data(), sb.size());
      std::cout << (ok ? "ok " : "ERR ") << (p.payload.empty() ? "-" : hs(p.payload)) << std::endl;
    } else if (cmd == "announce") {  // announce <frame>
      std::string f; std::cin >> f; Bytes fb = hx(f); bc::Packet p; bc::Announce a;
      bool ok = bc::decode(fb.data(), fb.size(), p) && bc::decodeAnnounce(p.payload, a);
      std::cout << (ok ? "ok " + a.nickname + " " + toHex(a.noiseKey, 32) : "ERR") << std::endl;
    } else if (cmd == "file") {      // file <payload hex>  -> name mime size
      std::string f; std::cin >> f; Bytes fb = hx(f); bc::FilePacket fp;
      bool ok = bc::decodeFile(fb.data(), fb.size(), fp);
      std::cout << (ok ? "ok " + fp.name + " " + fp.mime + " " + std::to_string(fp.content.size()) : "ERR") << std::endl;
      std::cout << hs(bc::encodeFile(fp)) << std::endl;
    }
  }
}
