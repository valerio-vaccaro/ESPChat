# Protocol notes

<nav class="site-nav"><a href="index">Home</a><a href="user-guide">User guide</a><a href="development">Development</a><a href="limitations">Limitations</a></nav>

ESPChat implements the portions of the bitchat protocol used by its current features. The details below describe this firmware implementation; compatibility against official iOS and Android clients remains unverified.

## BLE transport

The device advertises a BLE GATT service and exposes a characteristic supporting write, write without response, and notify:

| Item | UUID |
|---|---|
| Service | `F47B5E2D-4A9E-4C5A-9B3F-8E1D2C3A4B5C` |
| Characteristic | `A1B2C3D4-E5F6-4A5B-8C9D-0E1F2A3B4C5D` |

By default the device acts as a BLE peripheral. Optional central mode can also connect to phones. Connections are shared between roles and capped at three on SC01 and four on Plus.

## Packet layout

The packet header is versioned: v1 uses a 14-byte header and v2 uses a 16-byte header with a 32-bit length. The encoded packet includes the sender peer ID (8 bytes), optional recipient ID (8 bytes), route information in v2, payload, and optional Ed25519 signature (64 bytes).

Packets may be padded using a PKCS#7-style scheme to 256, 512, 1024, or 2048-byte blocks. The signature is calculated over the padded representation with TTL set to zero. See `src/proto/packet.*` for implementation details.

## Identity and announcements

The peer ID is the first eight bytes of SHA-256 of the Noise static public key. Announce TLVs carry the nickname, Noise public key, signing public key, and capabilities. The device's wall clock begins at build time and synchronizes from peer announces.

## Signatures and private messages

Public messages are Ed25519 signed and verified against the sender's announced signing key. Private conversations establish a `Noise_XX_25519_ChaChaPoly_SHA256` session. A transport frame is encoded as `[u32 BE nonce][ciphertext][tag]`.

Current inner message type IDs include `0x01` for message, `0x02` for read receipt, `0x03` for delivered receipt, and `0x20` for file data.

## Fragmentation, compression, and files

Large mesh packets are fragmented and reassembled with a fragment identifier, fragment index, and total count. The implementation relays packets with TTL handling and de-duplication, and supports raw DEFLATE decompression.

File payloads use `BitchatFilePacket` TLVs. Public files use type `0x22` in v2 signed packets. Private files use type `0x20` inside the Noise encrypted transport. Public text is limited to 96-byte chunks by this firmware to avoid incompatibilities in how signature verification treats compressed payloads.

## Unsupported protocol features

Nostr/geohash channels, gossip synchronization (`requestSync`), and live push-to-talk voice frames are not implemented. Such packets are ignored by the client and may still be relayed by the mesh layer. Other limits are listed in [Limitations](limitations).
