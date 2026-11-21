// Board / feature configuration for ESPChat.
// Selected by the PlatformIO environment (-DBOARD_WT32_SC01 or -DBOARD_WT32_SC01_PLUS).
#pragma once

// Native panel geometry (portrait) and the UI geometry (landscape).
#define PANEL_W 320
#define PANEL_H 480
#ifndef LCD_ROTATION
  #define LCD_ROTATION 1      // 1 = landscape; use 3 to flip it by 180 degrees
#endif
#define SCREEN_W 480
#define SCREEN_H 320

#if defined(BOARD_WT32_SC01_PLUS)
  #define BOARD_NAME "WT32-SC01 Plus"
  // Display: ST7796, 8-bit parallel (i80)
  #define LCD_D0 9
  #define LCD_D1 46
  #define LCD_D2 3
  #define LCD_D3 8
  #define LCD_D4 18
  #define LCD_D5 17
  #define LCD_D6 16
  #define LCD_D7 15
  #define LCD_WR 47
  #define LCD_RS 0
  #define LCD_RST 4
  #define LCD_BL 45
  #define LCD_INVERT true
  // Touch: FT6336U
  #define TP_SDA 6
  #define TP_SCL 5
  #define TP_INT 7
  // Speaker: NS4168 I2S amplifier
  #define HAS_AUDIO_OUT 1
  #define I2S_SPK_BCLK 36
  #define I2S_SPK_LRCK 35
  #define I2S_SPK_DOUT 37
  // micro SD (SPI)
  #define HAS_SD 1
  #define SD_CS 41
  #define SD_MOSI 40
  #define SD_SCK 39
  #define SD_MISO 38
  // Optional external I2S MEMS microphone (e.g. INMP441) on the expansion
  // header. Set the pins and HAS_MIC 1 to enable voice-note recording.
  #ifndef HAS_MIC
    #define HAS_MIC 0
  #endif
  #define I2S_MIC_SCK 10
  #define I2S_MIC_WS 11
  #define I2S_MIC_SD 12

#elif defined(BOARD_WT32_SC01)
  #define BOARD_NAME "WT32-SC01"
  // Display: ST7796, SPI
  #define LCD_SCLK 14
  #define LCD_MOSI 13
  #define LCD_DC 21
  #define LCD_CS 15
  #define LCD_RST 22
  #define LCD_BL 23
  #define LCD_INVERT false
  // Touch: FT6336U
  #define TP_SDA 18
  #define TP_SCL 19
  #define TP_INT 39
  // No speaker on board. Wire an I2S DAC (e.g. MAX98357A) and set HAS_AUDIO_OUT 1.
  #ifndef HAS_AUDIO_OUT
    #define HAS_AUDIO_OUT 0
  #endif
  #define I2S_SPK_BCLK 26
  #define I2S_SPK_LRCK 25
  #define I2S_SPK_DOUT 33
  #define HAS_SD 0
  #ifndef HAS_MIC
    #define HAS_MIC 0
  #endif
  #define I2S_MIC_SCK 32
  #define I2S_MIC_WS 27
  #define I2S_MIC_SD 34
#else
  #error "Define BOARD_WT32_SC01 or BOARD_WT32_SC01_PLUS"
#endif

// ---- Voice notes recorded on-device: 8 kHz, 8-bit unsigned PCM WAV ----
#define VOICE_SAMPLE_RATE 8000
#define VOICE_MAX_SECONDS 10

// ---- Limits ----
#define MAX_MESSAGES_PER_CONV 80
#define MAX_PUBLIC_MEDIA_BYTES (500 * 1024)
#define MAX_PRIVATE_MEDIA_FRAGMENTS 256
