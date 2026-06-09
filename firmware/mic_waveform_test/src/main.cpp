#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <driver/i2s.h>
#include <math.h>

#if __has_include("local_secrets.h")
#include "local_secrets.h"
#endif

#ifndef LOCAL_WIFI_SSID
#define LOCAL_WIFI_SSID ""
#endif

#ifndef LOCAL_WIFI_PASSWORD
#define LOCAL_WIFI_PASSWORD ""
#endif

#ifndef DEVICE_VERSION
#define DEVICE_VERSION "local-dev"
#endif

#ifndef OTA_MANIFEST_URL
#define OTA_MANIFEST_URL ""
#endif

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
static constexpr size_t AUDIO_BLOCK_SAMPLES = 192;
static constexpr size_t HEART_BLOCKS_PER_POINT = 2;

static constexpr int SCREEN_W = 240;
static constexpr int SCREEN_H = 240;
static constexpr int CENTER_X = SCREEN_W / 2;
static constexpr int CENTER_Y = SCREEN_H / 2;
static constexpr int SAFE_RADIUS = 102;
static constexpr int HEADER_BOTTOM = 42;
static constexpr int WAVE_MARGIN = 12;

static constexpr uint16_t COLOR_BG = 0x2A4B;
static constexpr uint16_t COLOR_GRID = 0x3B6D;
static constexpr uint16_t COLOR_WAVE = 0xDDFB;
static constexpr uint16_t COLOR_TEXT = 0xFFFF;
static constexpr uint16_t COLOR_DIM = 0x9D76;

Arduino_DataBus *bus = new Arduino_ESP32SPI(LCD_DC, LCD_CS, LCD_SCLK, LCD_MOSI, GFX_NOT_DEFINED, SPI2_HOST);
Arduino_GFX *gfx = new Arduino_GC9A01(bus, LCD_RST, 0, true);

static float wave[SCREEN_W];
static float dc = 0.0f;
static float lowFast = 0.0f;
static float lowSlow = 0.0f;
static float heartPointSum = 0.0f;
static size_t heartPointBlocks = 0;
static float smoothedLevel = 0.0f;
static float beatEnvelope = 0.0f;
static float beatFloor = 0.0f;
static float beatThreshold = 0.0f;
static float autoGain = 18.0f;
static float bpm = 0.0f;
static uint32_t lastBeatMs = 0;
static uint32_t lastDrawMs = 0;
static uint32_t lastOtaCheckMs = 0;
static bool otaCheckedOnce = false;
static Preferences preferences;

static int16_t clampInt16(float value, int16_t minValue, int16_t maxValue)
{
  if (value < minValue) return minValue;
  if (value > maxValue) return maxValue;
  return (int16_t)value;
}

static int safeHalfHeightAtX(int x)
{
  const int dx = x - CENTER_X;
  const int inside = SAFE_RADIUS * SAFE_RADIUS - dx * dx;
  if (inside <= 0) {
    return 0;
  }

  int halfHeight = (int)sqrtf((float)inside) - WAVE_MARGIN;
  const int headerLimit = CENTER_Y - HEADER_BOTTOM - WAVE_MARGIN;
  halfHeight = min(halfHeight, headerLimit);
  return max(0, halfHeight);
}

static void drawRoundVerticalGrid(int x)
{
  const int halfHeight = safeHalfHeightAtX(x);
  if (halfHeight <= 0) {
    return;
  }
  gfx->drawFastVLine(x, CENTER_Y - halfHeight, halfHeight * 2, COLOR_GRID);
}

static void drawRoundHorizontalGrid(int y)
{
  const int dy = y - CENTER_Y;
  const int inside = SAFE_RADIUS * SAFE_RADIUS - dy * dy;
  if (inside <= 0) {
    return;
  }

  const int halfWidth = (int)sqrtf((float)inside) - WAVE_MARGIN;
  gfx->drawFastHLine(CENTER_X - halfWidth, y, halfWidth * 2, COLOR_GRID);
}

static void drawBackground()
{
  gfx->fillScreen(COLOR_BG);

  for (int x = 24; x < SCREEN_W; x += 24) {
    drawRoundVerticalGrid(x);
  }

  for (int y = CENTER_Y - 72; y <= CENTER_Y + 72; y += 24) {
    drawRoundHorizontalGrid(y);
  }

  gfx->drawFastHLine(CENTER_X - SAFE_RADIUS + WAVE_MARGIN, CENTER_Y, (SAFE_RADIUS - WAVE_MARGIN) * 2, COLOR_DIM);
  gfx->setTextColor(COLOR_TEXT, COLOR_BG);
  gfx->setTextSize(1);
  gfx->setCursor(76, 13);
  gfx->print("HEART MIC");

  if (WiFi.status() == WL_CONNECTED) {
    gfx->setTextSize(1);
    gfx->setCursor(82, 24);
    gfx->print(WiFi.localIP().toString());
  }

  if (bpm > 30.0f && bpm < 220.0f) {
    gfx->setCursor(96, 35);
    gfx->print((int)(bpm + 0.5f));
    gfx->print(" BPM");
  }
}

static String jsonValue(const String &json, const String &key)
{
  const String pattern = "\"" + key + "\"";
  int keyPos = json.indexOf(pattern);
  if (keyPos < 0) return "";

  int colonPos = json.indexOf(':', keyPos + pattern.length());
  if (colonPos < 0) return "";

  int firstQuote = json.indexOf('"', colonPos + 1);
  if (firstQuote < 0) return "";

  int secondQuote = json.indexOf('"', firstQuote + 1);
  if (secondQuote < 0) return "";

  return json.substring(firstQuote + 1, secondQuote);
}

static void saveLocalWifiCredentials()
{
  if (strlen(LOCAL_WIFI_SSID) == 0) {
    return;
  }

  preferences.begin("wifi", false);
  preferences.putString("ssid", LOCAL_WIFI_SSID);
  preferences.putString("password", LOCAL_WIFI_PASSWORD);
  preferences.end();
}

static bool loadWifiCredentials(String &ssid, String &password)
{
  preferences.begin("wifi", true);
  ssid = preferences.getString("ssid", "");
  password = preferences.getString("password", "");
  preferences.end();
  return ssid.length() > 0;
}

static void connectWifi()
{
  saveLocalWifiCredentials();

  String ssid;
  String password;
  if (!loadWifiCredentials(ssid, password)) {
    Serial.println("wifi not configured");
    return;
  }

  Serial.printf("connecting wifi: %s\n", ssid.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());

  const uint32_t startMs = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startMs < 15000) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("wifi connected, ip=");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("wifi connect failed");
  }
}

static bool downloadAndApplyFirmware(const String &firmwareUrl)
{
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  if (!http.begin(client, firmwareUrl)) {
    Serial.println("ota firmware begin failed");
    return false;
  }
  http.addHeader("User-Agent", "MotemaSens-ESP32S3");
  http.addHeader("Accept", "application/vnd.github.raw");

  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("ota firmware http code=%d\n", code);
    http.end();
    return false;
  }

  const int contentLength = http.getSize();
  if (contentLength <= 0) {
    Serial.println("ota firmware size invalid");
    http.end();
    return false;
  }

  if (!Update.begin(contentLength)) {
    Serial.println("ota update begin failed");
    http.end();
    return false;
  }

  WiFiClient *stream = http.getStreamPtr();
  const size_t written = Update.writeStream(*stream);
  const bool ok = (written == (size_t)contentLength) && Update.end(true);
  http.end();

  if (!ok) {
    Serial.printf("ota failed, written=%u expected=%d error=%d\n", (unsigned)written, contentLength, Update.getError());
    return false;
  }

  Serial.println("ota done, rebooting");
  delay(500);
  ESP.restart();
  return true;
}

static void checkForOtaUpdate()
{
  if (strlen(OTA_MANIFEST_URL) == 0 || WiFi.status() != WL_CONNECTED) {
    return;
  }

  Serial.println("checking ota manifest");

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  if (!http.begin(client, OTA_MANIFEST_URL)) {
    Serial.println("ota manifest begin failed");
    return;
  }
  http.addHeader("User-Agent", "MotemaSens-ESP32S3");
  http.addHeader("Accept", "application/vnd.github.raw");

  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("ota manifest http code=%d\n", code);
    http.end();
    return;
  }

  const String manifest = http.getString();
  http.end();

  const String version = jsonValue(manifest, "version");
  const String firmwareUrl = jsonValue(manifest, "firmware_url");

  if (version.length() == 0 || firmwareUrl.length() == 0) {
    Serial.println("ota manifest invalid");
    return;
  }

  Serial.printf("device=%s latest=%s\n", DEVICE_VERSION, version.c_str());
  if (version == DEVICE_VERSION) {
    return;
  }

  downloadAndApplyFirmware(firmwareUrl);
}

static void drawWaveform()
{
  drawBackground();

  int previousY = CENTER_Y;
  for (int x = 0; x < SCREEN_W; ++x) {
    const float value = wave[x];
    const int halfHeight = safeHalfHeightAtX(x);
    if (halfHeight <= 0) {
      previousY = CENTER_Y;
      continue;
    }

    const int y = clampInt16(CENTER_Y - value * halfHeight * 0.82f, CENTER_Y - halfHeight, CENTER_Y + halfHeight);

    if (x > 0) {
      gfx->drawLine(x - 1, previousY, x, y, COLOR_WAVE);
    }

    previousY = y;
  }
}

static void pushWavePoint(float sample)
{
  memmove(&wave[0], &wave[1], sizeof(wave) - sizeof(wave[0]));
  const float softened = (wave[SCREEN_W - 2] * 0.84f) + (sample * 0.16f);
  wave[SCREEN_W - 1] = constrain(softened, -1.0f, 1.0f);
}

static void updateBeatDetector(float energy)
{
  const uint32_t now = millis();
  beatEnvelope = (beatEnvelope * 0.90f) + (energy * 0.10f);
  beatFloor = (beatFloor * 0.996f) + (beatEnvelope * 0.004f);
  beatThreshold = (beatThreshold * 0.96f) + ((beatFloor + 0.18f) * 0.04f);

  const bool beatCandidate = beatEnvelope > beatThreshold && energy > 0.22f;
  if (!beatCandidate || now - lastBeatMs < 330) {
    return;
  }

  if (lastBeatMs > 0) {
    const uint32_t intervalMs = now - lastBeatMs;
    if (intervalMs >= 330 && intervalMs <= 2000) {
      const float instantBpm = 60000.0f / (float)intervalMs;
      bpm = (bpm <= 0.0f) ? instantBpm : (bpm * 0.78f) + (instantBpm * 0.22f);
      Serial.printf("beat interval=%lu ms bpm=%.1f level=%.3f gain=%.1f\n",
                    (unsigned long)intervalMs, bpm, beatEnvelope, autoGain);
    }
  }

  lastBeatMs = now;
}

static void readMicSamples()
{
  int32_t samples[AUDIO_BLOCK_SAMPLES];
  size_t bytesRead = 0;
  const esp_err_t result = i2s_read(I2S_PORT, samples, sizeof(samples), &bytesRead, pdMS_TO_TICKS(24));
  if (result != ESP_OK || bytesRead == 0) {
    pushWavePoint(0.0f);
    return;
  }

  const size_t count = bytesRead / sizeof(samples[0]);
  float sumSq = 0.0f;
  float signedPeak = 0.0f;

  for (size_t i = 0; i < count; ++i) {
    // SPH0645 data is 24-bit I2S in a 32-bit slot. Keep the low-frequency body sound band.
    const float sample = (float)(samples[i] >> 14);
    dc += 0.00020f * (sample - dc);
    const float centered = sample - dc;

    lowFast += 0.052f * (centered - lowFast);
    lowSlow += 0.006f * (centered - lowSlow);
    const float heartBand = lowFast - lowSlow;

    sumSq += heartBand * heartBand;

    if (fabsf(heartBand) > fabsf(signedPeak)) {
      signedPeak = heartBand;
    }
  }

  const float rms = sqrtf(sumSq / max<size_t>(1, count));
  float normalized = rms / 3200.0f;

  // Heart sounds are much smaller than speech. Keep AGC slow so each beat shape stays natural.
  const float target = normalized * autoGain;
  if (target > 0.72f) {
    autoGain *= 0.992f;
  } else if (target < 0.20f) {
    autoGain *= 1.004f;
  }
  autoGain = constrain(autoGain, 6.0f, 85.0f);

  normalized = constrain(normalized * autoGain, 0.0f, 1.0f);
  smoothedLevel = (smoothedLevel * 0.94f) + (normalized * 0.06f);
  updateBeatDetector(smoothedLevel);

  const float signedPoint = constrain((signedPeak / 4200.0f) * autoGain, -1.0f, 1.0f);
  heartPointSum += signedPoint;
  heartPointBlocks++;

  if (heartPointBlocks >= HEART_BLOCKS_PER_POINT) {
    pushWavePoint(heartPointSum / (float)heartPointBlocks);
    heartPointSum = 0.0f;
    heartPointBlocks = 0;
  }
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
  Serial.printf("device version: %s\n", DEVICE_VERSION);

  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);

  if (!gfx->begin(80000000)) {
    Serial.println("LCD begin failed");
  }

  drawBackground();
  connectWifi();
  drawBackground();
  setupI2S();

  for (float &point : wave) {
    point = 0.0f;
  }

  Serial.println("mic waveform test running");
}

void loop()
{
  const uint32_t now = millis();
  if (!otaCheckedOnce || now - lastOtaCheckMs >= 60000) {
    otaCheckedOnce = true;
    lastOtaCheckMs = now;
    checkForOtaUpdate();
  }

  readMicSamples();

  if (now - lastDrawMs >= 24) {
    lastDrawMs = now;
    drawWaveform();
  }
}
