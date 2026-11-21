#include "audio.h"
#include <Arduino.h>
#include <driver/i2s.h>
#include "../../include/board_config.h"
#include "../chat/chat.h"
#include "audio_parse.h"

#if HAS_AUDIO_OUT
#include "libhelix-aac/aacdec.h"
#endif

static volatile uint8_t gVolume = 70;
void audioSetVolume(uint8_t v) { gVolume = v > 100 ? 100 : v; }

// ============================ playback ============================
#if HAS_AUDIO_OUT
static constexpr i2s_port_t SPK_PORT = I2S_NUM_0;
static QueueHandle_t playQueue;
static volatile bool stopFlag = false;
static volatile bool playing = false;
static uint32_t curRate = 0;

static void i2sOutBegin() {
  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  cfg.sample_rate = 16000;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = 0;
  cfg.dma_buf_count = 8;
  cfg.dma_buf_len = 256;
  cfg.use_apll = false;
  cfg.tx_desc_auto_clear = true;
  i2s_driver_install(SPK_PORT, &cfg, 0, nullptr);
  i2s_pin_config_t pins = {};
  pins.mck_io_num = I2S_PIN_NO_CHANGE;
  pins.bck_io_num = I2S_SPK_BCLK;
  pins.ws_io_num = I2S_SPK_LRCK;
  pins.data_out_num = I2S_SPK_DOUT;
  pins.data_in_num = I2S_PIN_NO_CHANGE;
  i2s_set_pin(SPK_PORT, &pins);
  i2s_zero_dma_buffer(SPK_PORT);
  curRate = 16000;
}

static void setRate(uint32_t r) {
  if (r && r != curRate) {
    i2s_set_sample_rates(SPK_PORT, r);
    curRate = r;
  }
}

// write interleaved stereo frames, applying volume
static void writeStereo(int16_t* frames, size_t count) {
  int vol = gVolume;
  for (size_t i = 0; i < count * 2; ++i) frames[i] = (int32_t)frames[i] * vol / 100;
  size_t written;
  i2s_write(SPK_PORT, frames, count * 4, &written, portMAX_DELAY);
}

static void playWav(const WavInfo& w) {
  setRate(w.rate);
  int16_t buf[512];
  size_t bps = (w.bits / 8) * w.channels;
  size_t frames = w.dataLen / bps;
  size_t i = 0;
  while (i < frames && !stopFlag) {
    size_t n = 0;
    for (; n < 256 && i < frames; ++n, ++i) {
      const uint8_t* s = w.data + i * bps;
      int16_t l, r;
      if (w.bits == 8) { l = (int16_t)((s[0] - 128) << 8); r = w.channels > 1 ? (int16_t)((s[1] - 128) << 8) : l; }
      else { l = (int16_t)(s[0] | (s[1] << 8)); r = w.channels > 1 ? (int16_t)(s[2] | (s[3] << 8)) : l; }
      buf[2 * n] = l;
      buf[2 * n + 1] = r;
    }
    writeStereo(buf, n);
  }
}

static void playM4a(const Bytes& data, const M4aInfo& m) {
  HAACDecoder dec = AACInitDecoder();
  if (!dec) return;
  AACFrameInfo fi = {};
  fi.nChans = m.channels ? m.channels : 1;
  fi.sampRateCore = m.sampleRate;
  fi.profile = AAC_PROFILE_LC;
  AACSetRawBlockParams(dec, 0, &fi);
  int16_t* pcm = (int16_t*)malloc(2048 * 2 * sizeof(int16_t));
  int16_t* st = (int16_t*)malloc(2048 * 2 * sizeof(int16_t));
  for (size_t s = 0; s < m.sampleSizes.size() && !stopFlag && pcm && st; ++s) {
    unsigned char* in = (unsigned char*)data.data() + m.sampleOffsets[s];
    int left = m.sampleSizes[s];
    if (AACDecode(dec, &in, &left, pcm) != 0) continue;
    AACFrameInfo out;
    AACGetLastFrameInfo(dec, &out);
    setRate(out.sampRateOut);
    int ch = out.nChans ? out.nChans : 1;
    int frames = out.outputSamps / ch;
    for (int i = 0; i < frames; ++i) {
      st[2 * i] = pcm[i * ch];
      st[2 * i + 1] = pcm[i * ch + (ch > 1 ? 1 : 0)];
    }
    writeStereo(st, frames);
  }
  free(pcm);
  free(st);
  AACFreeDecoder(dec);
}

static void playTask(void*) {
  for (;;) {
    std::shared_ptr<Media>* item = nullptr;
    if (xQueueReceive(playQueue, &item, portMAX_DELAY) != pdTRUE || !item) continue;
    std::shared_ptr<Media> m = *item;
    delete item;
    stopFlag = false;
    playing = true;
    WavInfo w;
    M4aInfo a;
    if (parseWav(m->data, w)) playWav(w);
    else if (parseM4a(m->data, a)) playM4a(m->data, a);
    else log_w("unsupported audio format: %s", m->mime.c_str());
    i2s_zero_dma_buffer(SPK_PORT);
    playing = false;
  }
}

bool audioCanPlay() { return true; }
bool audioPlay(std::shared_ptr<Media> m) {
  if (!m) return false;
  if (playing) { stopFlag = true; }
  auto* item = new std::shared_ptr<Media>(m);
  if (xQueueSend(playQueue, &item, 0) != pdTRUE) { delete item; return false; }
  return true;
}
void audioStop() { stopFlag = true; }
bool audioPlaying() { return playing; }
#else
bool audioCanPlay() { return false; }
bool audioPlay(std::shared_ptr<Media>) { return false; }
void audioStop() {}
bool audioPlaying() { return false; }
#endif

// ============================ recording ============================
#if HAS_MIC
static constexpr i2s_port_t MIC_PORT = I2S_NUM_1;
static constexpr uint32_t MIC_RATE = 16000;
static volatile bool recording = false;
static uint8_t* recBuf = nullptr;
static volatile size_t recLen = 0;
static constexpr size_t REC_MAX = VOICE_SAMPLE_RATE * VOICE_MAX_SECONDS;
static TaskHandle_t recTaskHandle = nullptr;

static void micBegin() {
  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
  cfg.sample_rate = MIC_RATE;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.dma_buf_count = 6;
  cfg.dma_buf_len = 256;
  i2s_driver_install(MIC_PORT, &cfg, 0, nullptr);
  i2s_pin_config_t pins = {};
  pins.mck_io_num = I2S_PIN_NO_CHANGE;
  pins.bck_io_num = I2S_MIC_SCK;
  pins.ws_io_num = I2S_MIC_WS;
  pins.data_out_num = I2S_PIN_NO_CHANGE;
  pins.data_in_num = I2S_MIC_SD;
  i2s_set_pin(MIC_PORT, &pins);
  i2s_stop(MIC_PORT);
}

static void recTask(void*) {
  int32_t raw[256];
  int32_t dc = 0;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    i2s_start(MIC_PORT);
    i2s_zero_dma_buffer(MIC_PORT);
    while (recording && recLen < REC_MAX) {
      size_t got = 0;
      i2s_read(MIC_PORT, raw, sizeof(raw), &got, pdMS_TO_TICKS(100));
      size_t n = got / 4;
      for (size_t i = 0; i + 1 < n && recLen < REC_MAX; i += 2) {  // 16 kHz -> 8 kHz
        int32_t s = ((raw[i] >> 14) + (raw[i + 1] >> 14)) / 2;
        dc += (s - dc) >> 6;  // simple DC blocker
        s -= dc;
        s = s > 32767 ? 32767 : s < -32768 ? -32768 : s;
        recBuf[recLen++] = (uint8_t)((s >> 8) + 128);
      }
    }
    i2s_stop(MIC_PORT);
    recording = false;
  }
}

bool audioCanRecord() { return true; }
bool audioRecordStart() {
  if (recording) return false;
  if (!recBuf) recBuf = (uint8_t*)ps_malloc(REC_MAX);
  if (!recBuf) return false;
  recLen = 0;
  recording = true;
  xTaskNotifyGive(recTaskHandle);
  return true;
}
bool audioRecordStop(Bytes& wav) {
  recording = false;
  vTaskDelay(pdMS_TO_TICKS(150));
  if (recLen < VOICE_SAMPLE_RATE / 2) return false;  // < 0.5 s
  wav = makeWav8(recBuf, recLen, VOICE_SAMPLE_RATE);
  return true;
}
bool audioRecording() { return recording; }
float audioRecordSeconds() { return (float)recLen / VOICE_SAMPLE_RATE; }
#else
bool audioCanRecord() { return false; }
bool audioRecordStart() { return false; }
bool audioRecordStop(Bytes&) { return false; }
bool audioRecording() { return false; }
float audioRecordSeconds() { return 0; }
#endif

void audioBegin() {
#if HAS_AUDIO_OUT
  i2sOutBegin();
  playQueue = xQueueCreate(4, sizeof(void*));
  xTaskCreatePinnedToCore(playTask, "audio", 8192, nullptr, 5, nullptr, 1);
#endif
#if HAS_MIC
  micBegin();
  xTaskCreatePinnedToCore(recTask, "mic", 4096, nullptr, 5, &recTaskHandle, 1);
#endif
}
