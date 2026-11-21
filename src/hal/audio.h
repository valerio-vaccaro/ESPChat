// Voice notes: playback of WAV and M4A/AAC (bitchat iOS/Android voice notes) through I2S,
// and recording from an optional I2S MEMS microphone into 8 kHz 8-bit WAV.
#pragma once
#include <memory>
#include "../proto/bytes.h"
struct Media;

void audioBegin();
bool audioCanPlay();
bool audioCanRecord();
bool audioPlay(std::shared_ptr<Media> m);
void audioStop();
bool audioPlaying();
void audioSetVolume(uint8_t v);  // 0..100

bool audioRecordStart();
bool audioRecordStop(Bytes& wav);  // returns a complete .wav file
bool audioRecording();
float audioRecordSeconds();

// Duration estimate in seconds (0 when unknown). Pure parsing, no hardware.
float audioDurationSec(const Media& m);
// Build a mono 8-bit PCM WAV file.
Bytes makeWav8(const uint8_t* pcm, size_t n, uint32_t rate);
