# User guide

<nav class="site-nav"><a href="index">Home</a><a href="getting-started">Get started</a><a href="hardware">Hardware</a><a href="screenshots">Screenshots</a></nav>

## Navigation

The landscape screen is 480 × 320 pixels. The sidebar tabs are:

| Tab | Purpose |
|---|---|
| `#MESH` | Public, broadcast conversation |
| `PEERS` | Nearby announced peers and private-chat entry point |
| `ID` | Current nickname, peer ID, public keys, and QR code |
| `KEYS` | Private keys, reveal control, export, and import |
| `CFG` | Nickname, connection and screensaver settings, device information, panic reset |

The interface uses a black background and green text. Cyan highlights encrypted chat and information, amber marks warnings and voice, magenta highlights media, and violet marks identity. Nicknames use stable per-peer colors. Red and blue header LEDs indicate unread public and private messages respectively, with a counter.

## Public chat

`#MESH` is a broadcast channel. Type in the input bar and press the send button. Public messages are signed with the sender's Ed25519 signing key; messages without a verified signature are marked as unverified. Messages are held in memory, with a maximum of 80 per conversation, and are not restored after reboot.

## Private chat

Open **PEERS**, then select a node. Private messages use a Noise XX handshake with Curve25519 static keys and ChaChaPoly encryption. The peer row indicates when an encrypted session is available. Message indicators show sent (`✓`), delivered (`✓✓`), and read (`👁`) states when receipts are received.

Private conversations are between the selected peer and this device. They still rely on BLE mesh transport for delivery; encryption does not guarantee that a peer is currently reachable.

## Images and attachments

Tap `+` in a conversation to open the attachment menu.

- **Doodle** opens the drawing pad. Draw with touch, then send the resulting JPEG.
- **SD Card** opens the image picker on WT32-SC01 Plus. Place `.jpg`, `.jpeg`, or `.png` images under `/bitchat/send/` or the SD card root.
- Received image media can be opened from its thumbnail. Received media is saved on SD under `/bitchat/rx/` when storage is available.

The firmware can receive JPEG and PNG images. Progressive JPEG is unsupported. Current media size limits and transfer caveats are listed in [Limitations](limitations).

## Voice notes

If an I2S microphone is wired and recording is enabled in the build, press and hold the microphone control to record; release to send. Recordings are 8 kHz, 8-bit PCM WAV and limited to 10 seconds. The firmware can play WAV and bitchat `.m4a` AAC voice messages. The Plus plays through its on-board NS4168 amplifier; the original SC01 needs an external I2S output device for sound.

## Identity and keys

The **ID** page shows the peer ID, public Noise key, public signing key, and fingerprint in text and QR form. The fingerprint helps peers verify the identity associated with your keys.

The **KEYS** page keeps private key material hidden until **REVEAL** is tapped. Hide the values again when finished. On Plus, keys can be exported to SD or imported from `/bitchat/identity.key`; the UI also supports entering a key as hex. Treat exported private keys as credentials: anyone with them can impersonate the identity.

**PANIC** erases identity and settings and creates a new identity. Existing peers will see the device as a different node. This action is destructive.

## Settings and screensaver

In **CFG**, set the nickname and choose whether ESPChat should also connect to phones as a BLE central. Central mode is off by default; the device always advertises as a peripheral. The digital-rain screensaver can be enabled or disabled; it activates after two minutes of inactivity and exits on touch.

Settings and identity persist in NVS. Conversations do not persist across reboot.
