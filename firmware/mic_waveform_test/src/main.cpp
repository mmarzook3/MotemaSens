#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <driver/i2s.h>
#include <math.h>

// Waveshare ESP32-S3-LCD-1.28 display pins.
static constexpr int LCD_DC = 8;
static constexpr int LCD_CS = 9;
static constexpr int LCD_SCLK = 10;
static constexpr int LCD_MOSI = 11;
static constexpr int LCD_RST = 12;
static constexpr int LCD_BL = 40;

// Custom Lobe PCB SPH0645LM4H-B mic pins.
static constexpr int I2S_DATA = 3;
static constexpr int I2S_BCLK = 4;
static constexpr int I2S_WS = 5;

static constexpr i2s_port_t I2S_PORT = I2S_NUM_0;
static constexpr i2s_channel_fmt_t I2S_CHANNEL = I2S_CHANNEL_FMT_ONLY_LEFT;
static constexpr uint32_t SAMPLE_RATE = 16000;
static constexpr size_t AUDIO_BLOCK_SAMPLES = 160;

static constexpr int SCREEN_W = 240;
static constexpr int SCREEN_H = 240;
static constexpr int CENTER_Y = SCREEN_H / 2;
static constexpr int WAVE_TOP = 34;
static constexpr int WAVE_BOTTOM = 214;
static constexpr int WAVE_HALF_H = (WAVE_BOTTOM - WAVE_TOP) / 2;

static constexpr uint16_t COLOR_BG = 0x2A4B;
static constexpr uint16_t COLOR_GRID = 0x3B6D;
static constexpr uint16_t COLOR_WAVE = 0xDDFB;
static constexpr uint16_t COLOR_TEXT = 0xFFFF;
static constexpr uint16_t COLOR_DIM = 0x9D76;

Arduino_DataBus *bus = new Arduino_ESP32SPI(LCD_DC, LCD_CS, LCD_SCLK, LCD_MOSI, GFX_NOT_DEFINED, SPI2_HOST);
Arduino_GFX *gfx = new Arduino_GC9A01(bus, LCD_RST, 0, true);

static float wave[SCREEN_W];
static float dc = 0.0f;
static float smoothedLevel = 0.0f;
static float autoGain = 12.0f;
static uint32_t lastDrawMs = 0;

static int16_t clampInt16(float value, int16_t minValue, int16_t maxValue)
{
  if (value < minValue) return minValue;
  if (value > maxValue) return maxValue;
  return (int16_t)value;
}

static void drawBackground()
{
  gfx->fillScreen(COLOR_BG);

  for (int x = 0; x < SCREEN_W; x += 24) {
    gfx->drawFastVLine(x, WAVE_TOP, WAVE_BOTTOM - WAVE_TOP, COLOR_GRID);
  }

  for (int y = WAVE_TOP; y <= WAVE_BOTTOM; y += 30) {
    gfx->drawFastHLine(0, y, SCREEN_W, COLOR_GRID);
  }

  gfx->drawFastHLine(0, CENTER_Y, SCREEN_W, COLOR_DIM);
  gfx->setTextColor(COLOR_TEXT, COLOR_BG);
  gfx->setTextSize(2);
  gfx->setCursor(46, 8);
  gfx->print("MIC TEST");
}

static void drawWaveform()
{
  drawBackground();

  int previousY = CENTER_Y;
  for (int x = 0; x < SCREEN_W; ++x) {
    const float value = wave[x];
    const int y = clampInt16(CENTER_Y - value * WAVE_HALF_H, WAVE_TOP, WAVE_BOTTOM);

    if (x > 0) {
      gfx->drawLine(x - 1, previousY, x, y, COLOR_WAVE);
    }

    const int bar = abs(y - CENTER_Y);
    if (bar > 2) {
      gfx->drawFastVLine(x, min(y, CENTER_Y), bar, COLOR_WAVE);
    }

    previousY = y;
  }

  const int meterWidth = clampInt16(smoothedLevel * 120.0f, 0, 120);
  gfx->fillRect(60, 224, 120, 5, 0x1A2A);
  gfx->fillRect(60, 224, meterWidth, 5, COLOR_WAVE);
}

static void pushWavePoint(float level)
{
  memmove(&wave[0], &wave[1], sizeof(wave) - sizeof(wave[0]));
  wave[SCREEN_W - 1] = level;
}

static float readMicLevel()
{
  int32_t samples[AUDIO_BLOCK_SAMPLES];
  size_t bytesRead = 0;
  const esp_err_t result = i2s_read(I2S_PORT, samples, sizeof(samples), &bytesRead, pdMS_TO_TICKS(40));
  if (result != ESP_OK || bytesRead == 0) {
    return 0.0f;
  }

  const size_t count = bytesRead / sizeof(samples[0]);
  float sumSq = 0.0f;

  for (size_t i = 0; i < count; ++i) {
    // SPH0645 data is 24-bit I2S in a 32-bit slot. Use the upper bits and remove DC slowly.
    const float sample = (float)(samples[i] >> 14);
    dc += 0.0015f * (sample - dc);
    const float centered = sample - dc;
    sumSq += centered * centered;
  }

  const float rms = sqrtf(sumSq / max<size_t>(1, count));
  float normalized = rms / 16384.0f;

  // Slow AGC: speech should fill the display, but silence should stay calm.
  const float target = normalized * autoGain;
  if (target > 0.72f) {
    autoGain *= 0.985f;
  } else if (target < 0.28f) {
    autoGain *= 1.002f;
  }
  autoGain = constrain(autoGain, 4.0f, 80.0f);

  normalized = constrain(normalized * autoGain, 0.0f, 1.0f);
  smoothedLevel = (smoothedLevel * 0.82f) + (normalized * 0.18f);
  return smoothedLevel;
}

static void setupI2S()
{
  const i2s_config_t config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 6,
    .dma_buf_len = 256,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0,
    .mclk_multiple = I2S_MCLK_MULTIPLE_256,
    .bits_per_chan = I2S_BITS_PER_CHAN_32BIT
  };

  const i2s_pin_config_t pins = {
    .mck_io_num = I2S_PIN_NO_CHANGE,
    .bck_io_num = I2S_BCLK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_DATA
  };

  ESP_ERROR_CHECK(i2s_driver_install(I2S_PORT, &config, 0, nullptr));
  ESP_ERROR_CHECK(i2s_set_pin(I2S_PORT, &pins));
  ESP_ERROR_CHECK(i2s_zero_dma_buffer(I2S_PORT));
}

void setup()
{
  Serial.begin(115200);
  delay(200);

  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);

  if (!gfx->begin(80000000)) {
    Serial.println("LCD begin failed");
  }

  drawBackground();
  setupI2S();

  for (float &point : wave) {
    point = 0.0f;
  }

  Serial.println("mic waveform test running");
}

void loop()
{
  const float level = readMicLevel();
  pushWavePoint(level * 0.95f);

  const uint32_t now = millis();
  if (now - lastDrawMs >= 24) {
    lastDrawMs = now;
    drawWaveform();
  }
}

