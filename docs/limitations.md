# Limitations and compatibility

<nav class="site-nav"><a href="index">Home</a><a href="user-guide">User guide</a><a href="protocol">Protocol</a><a href="development">Development</a></nav>

## Real-device compatibility

The protocol implementation was developed against the bitchat source and checked using the repository's host interop test. It has not yet been validated end-to-end against iOS or Android devices. In particular, verify packet padding by message type, announce signature acceptance, and private-media fragment handling before relying on interoperability.

## Media

- Progressive JPEG images are unsupported.
- Recording produces 8 kHz, 8-bit PCM WAV, not AAC `.m4a`; recordings are capped at 10 seconds.
- The firmware has no AAC encoder. It can play supported `.m4a` AAC and WAV input, subject to hardware audio output.
- Private media is limited to 256 fragments. Large media can exceed the limit and fail to send.
- Public media input is capped at 500 KiB. Public text is sent in chunks of up to 96 bytes.
- Very compressible public WAV data can fail signature verification on phone clients because of differences in payload compression and signature checking. Private voice transfers are not affected by this specific public-signature issue.

## Feature gaps

Nostr/geohash channels, `requestSync` gossip synchronization, and live push-to-talk voice frames are unsupported. Message history is held in RAM and is cleared on reboot. Identity and settings persist in NVS.

## Time and reachability

There is no RTC. The clock starts from the firmware build time and updates from the first suitable peer announce. Mobile clients may reject packets with timestamps too far from their own clock. BLE range, connection counts, and mesh peer availability depend on local radio conditions and device behavior.
