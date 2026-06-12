#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <driver/i2s.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
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

// Custom Lobe PCB status LED.
static constexpr int LED_GREEN = 14;

// Waveshare base-board QMI8658 IMU pins.
static constexpr int IMU_SDA = 6;
static constexpr int IMU_SCL = 7;

static constexpr i2s_port_t I2S_PORT = I2S_NUM_0;
static constexpr i2s_channel_fmt_t I2S_CHANNEL = I2S_CHANNEL_FMT_ONLY_LEFT;
static constexpr uint32_t SAMPLE_RATE = 16000;
static constexpr size_t AUDIO_BLOCK_SAMPLES = 64;
static constexpr size_t DISPLAY_POINTS_PER_BLOCK = 2;
static constexpr size_t LABEL_HISTORY = 8;
static constexpr uint32_t ACCEL_SAMPLE_PERIOD_MS = 8;
static constexpr uint32_t ACCEL_DEBUG_PERIOD_MS = 1000;
static constexpr uint32_t USB_LOG_DURATION_MS = 60000;
static constexpr uint32_t USB_LOG_PERIOD_MS = 10;
static constexpr uint32_t HEART_REFRACTORY_MS = 620;
static constexpr uint32_t HEART_PEAK_HOLD_MS = 180;
static constexpr bool USB_LOG_ACCEL_DEBUG = false;
static constexpr float MOTION_GATE_THRESHOLD_G = 0.075f;
static constexpr float HEART_NOISE_GATE = 0.045f;
static constexpr uint8_t ACQUISITION_CORE = 0;
static constexpr uint8_t OUTPUT_CORE = 1;
static constexpr uint32_t ACQUISITION_TASK_STACK = 8192;
static constexpr uint32_t OUTPUT_TASK_STACK = 12288;
static constexpr UBaseType_t ACQUISITION_TASK_PRIORITY = 3;
static constexpr UBaseType_t OUTPUT_TASK_PRIORITY = 1;

static constexpr int SCREEN_W = 240;
static constexpr int SCREEN_H = 240;
static constexpr size_t ACCEL_HISTORY = SCREEN_W;
static constexpr int CENTER_X = SCREEN_W / 2;
static constexpr int CENTER_Y = SCREEN_H / 2;
static constexpr int SAFE_RADIUS = 102;
static constexpr int HEADER_BOTTOM = 42;
static constexpr int WAVE_MARGIN = 12;
static constexpr int ACCEL_GRAPH_CENTER_Y = 88;
static constexpr int ACCEL_GRAPH_HALF_H = 32;
static constexpr int MIC_GRAPH_CENTER_Y = 156;
static constexpr int MIC_GRAPH_HALF_H = 42;

static constexpr uint16_t COLOR_BG = 0x2A4B;
static constexpr uint16_t COLOR_GRID = 0x3B6D;
static constexpr uint16_t COLOR_WAVE = 0xDDFB;
static constexpr uint16_t COLOR_TEXT = 0xFFFF;
static constexpr uint16_t COLOR_DIM = 0x9D76;
static constexpr uint16_t COLOR_X = 0xF986;
static constexpr uint16_t COLOR_Y = 0x87F0;
static constexpr uint16_t COLOR_Z = 0x7DFF;
static constexpr uint16_t COLOR_WIFI = 0x07E0;

Arduino_DataBus *bus = new Arduino_ESP32SPI(LCD_DC, LCD_CS, LCD_SCLK, LCD_MOSI, GFX_NOT_DEFINED, SPI2_HOST);
Arduino_GFX *display = new Arduino_GC9A01(bus, LCD_RST, 0, true);
Arduino_Canvas *gfx = new Arduino_Canvas(SCREEN_W, SCREEN_H, display);

static float wave[SCREEN_W];
static float accelXHistory[ACCEL_HISTORY];
static float accelYHistory[ACCEL_HISTORY];
static float accelZHistory[ACCEL_HISTORY];
static float latestAccelX = 0.0f;
static float latestAccelY = 0.0f;
static float latestAccelZ = 0.0f;
static float dc = 0.0f;
static float lowFast = 0.0f;
static float lowSlow = 0.0f;
static float smoothedLevel = 0.0f;
static float displaySample = 0.0f;
static float beatEnvelope = 0.0f;
static float beatFloor = 0.0f;
static float beatThreshold = 0.0f;
static float beatPeak = 0.0f;
static float autoGain = 10.0f;
static float acquisitionBpm = 0.0f;
static float bpm = 0.0f;
static float motionLevel = 0.0f;
static float lastAccelX = 0.0f;
static float lastAccelY = 0.0f;
static float lastAccelZ = 0.0f;
static bool haveLastAccel = false;
static uint32_t lastBeatMs = 0;
static bool beatArmed = true;
static bool beatCandidateActive = false;
static uint32_t beatCandidateStartMs = 0;
static uint32_t beatCandidatePeakMs = 0;
static float beatCandidatePeakLevel = 0.0f;
static float beatCandidatePeakGain = 0.0f;
static uint32_t rejectedBeatCandidates = 0;
static uint32_t lastDrawMs = 0;
static uint32_t lastLedToggleMs = 0;
static bool ledGreenOn = false;
static uint32_t lastOtaCheckMs = 0;
static bool otaCheckedOnce = false;
static uint32_t lastAccelReadMs = 0;
static uint32_t lastAccelDebugMs = 0;
static uint32_t accelSamplesSinceDebug = 0;
static uint32_t accelReadFailuresSinceDebug = 0;
static uint8_t accelRegisterDumpsLeft = 6;
static int16_t latestAccelRawX = 0;
static int16_t latestAccelRawY = 0;
static int16_t latestAccelRawZ = 0;
static float latestMicPoint = 0.0f;
static bool usbLogActive = false;
static uint32_t usbLogStartMs = 0;
static uint32_t usbLogLastSampleMs = 0;
static uint32_t usbLogSamples = 0;
static uint32_t usbLogBeats = 0;
static bool imuReady = false;
static uint8_t imuAddress = 0x6A;
static Preferences preferences;
static QueueHandle_t displayPointQueue = nullptr;
static QueueHandle_t beatEventQueue = nullptr;
static QueueHandle_t accelSampleQueue = nullptr;

struct HeartLabel {
  int x;
  char text[3];
};

struct BeatEvent {
  uint32_t beatMs;
  uint32_t intervalMs;
  float bpm;
  float level;
  float gain;
};

struct AccelSample {
  float x;
  float y;
  float z;
  bool valid;
};

static HeartLabel labels[LABEL_HISTORY] = {};
static bool nextLabelIsS1 = true;

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

static bool isSafePixel(int x, int y)
{
  const int dx = x - CENTER_X;
  const int dy = y - CENTER_Y;
  return (dx * dx + dy * dy) <= ((SAFE_RADIUS - WAVE_MARGIN) * (SAFE_RADIUS - WAVE_MARGIN));
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

static int centeredTextX(const String &text, int textSize)
{
  const int charWidth = 6 * textSize;
  return CENTER_X - (text.length() * charWidth) / 2;
}

static String shortDeviceVersion()
{
  String version = DEVICE_VERSION;
  if (version.length() > 7 && version != "local-dev") {
    version = version.substring(0, 7);
  }
  return version;
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
  const String versionText = "SW " + shortDeviceVersion();
  const int versionX = centeredTextX(versionText, 1);
  gfx->setCursor(versionX, 10);
  gfx->print(versionText);

  if (WiFi.status() == WL_CONNECTED) {
    gfx->fillCircle(versionX + (versionText.length() * 6) + 8, 13, 2, COLOR_WIFI);
  }

  gfx->setCursor(99, 23);
  gfx->print("MIC ACC");

  gfx->setCursor(58, 35);
  gfx->setTextColor(COLOR_X, COLOR_BG);
  gfx->print("X");
  gfx->setTextColor(COLOR_Y, COLOR_BG);
  gfx->print(" Y");
  gfx->setTextColor(COLOR_Z, COLOR_BG);
  gfx->print(" Z");
  gfx->setTextColor(COLOR_TEXT, COLOR_BG);
  gfx->print("  ");
  gfx->print(latestAccelX, 1);
  gfx->print(" ");
  gfx->print(latestAccelY, 1);
  gfx->print(" ");
  gfx->print(latestAccelZ, 1);
}

static void drawBandGrid(int graphCenterY, int graphHalfHeight)
{
  const int top = graphCenterY - graphHalfHeight;
  const int bottom = graphCenterY + graphHalfHeight;

  for (int x = 36; x < SCREEN_W - 24; x += 28) {
    bool drawing = false;
    int startY = top;
    for (int y = top; y <= bottom; ++y) {
      const bool safe = isSafePixel(x, y);
      if (safe && !drawing) {
        startY = y;
        drawing = true;
      } else if (!safe && drawing) {
        gfx->drawFastVLine(x, startY, y - startY, COLOR_GRID);
        drawing = false;
      }
    }
    if (drawing) {
      gfx->drawFastVLine(x, startY, bottom - startY + 1, COLOR_GRID);
    }
  }

  for (int y = top; y <= bottom; y += graphHalfHeight) {
    bool drawing = false;
    int startX = 0;
    for (int x = 0; x < SCREEN_W; ++x) {
      const bool safe = isSafePixel(x, y);
      if (safe && !drawing) {
        startX = x;
        drawing = true;
      } else if (!safe && drawing) {
        gfx->drawFastHLine(startX, y, x - startX, COLOR_GRID);
        drawing = false;
      }
    }
    if (drawing) {
      gfx->drawFastHLine(startX, y, SCREEN_W - startX, COLOR_GRID);
    }
  }
}

static void addHeartLabel()
{
  memmove(&labels[0], &labels[1], sizeof(labels) - sizeof(labels[0]));
  HeartLabel &label = labels[LABEL_HISTORY - 1];
  label.x = SCREEN_W - 10;
  label.text[0] = 'S';
  label.text[1] = nextLabelIsS1 ? '1' : '2';
  label.text[2] = '\0';
  nextLabelIsS1 = !nextLabelIsS1;
}

static void sendDisplayPoint(float point)
{
  if (!displayPointQueue) {
    return;
  }

  if (xQueueSend(displayPointQueue, &point, 0) != pdTRUE) {
    float dropped = 0.0f;
    xQueueReceive(displayPointQueue, &dropped, 0);
    xQueueSend(displayPointQueue, &point, 0);
  }
}

static void sendBeatEvent(const BeatEvent &event)
{
  if (!beatEventQueue) {
    return;
  }

  if (xQueueSend(beatEventQueue, &event, 0) != pdTRUE) {
    BeatEvent dropped = {};
    xQueueReceive(beatEventQueue, &dropped, 0);
    xQueueSend(beatEventQueue, &event, 0);
  }
}

static void discardBeatCandidate()
{
  beatCandidateActive = false;
  ++rejectedBeatCandidates;
}

static void confirmBeatCandidate()
{
  if (!beatCandidateActive) {
    return;
  }

  const uint32_t beatMs = beatCandidatePeakMs;
  const float beatLevel = beatCandidatePeakLevel;
  const float beatGain = beatCandidatePeakGain;
  beatCandidateActive = false;
  beatArmed = false;

  if (lastBeatMs == 0) {
    lastBeatMs = beatMs;
    return;
  }

  const uint32_t intervalMs = beatMs - lastBeatMs;
  if (intervalMs < HEART_REFRACTORY_MS || intervalMs > 1800) {
    ++rejectedBeatCandidates;
    return;
  }

  const float instantBpm = 60000.0f / (float)intervalMs;
  acquisitionBpm = (acquisitionBpm <= 0.0f) ? instantBpm : (acquisitionBpm * 0.78f) + (instantBpm * 0.22f);
  BeatEvent event = {beatMs, intervalMs, acquisitionBpm, beatLevel, beatGain};
  sendBeatEvent(event);
  lastBeatMs = beatMs;
}

static void sendAccelSample(const AccelSample &sample)
{
  if (!accelSampleQueue) {
    return;
  }

  if (xQueueSend(accelSampleQueue, &sample, 0) != pdTRUE) {
    AccelSample dropped = {};
    xQueueReceive(accelSampleQueue, &dropped, 0);
    xQueueSend(accelSampleQueue, &sample, 0);
  }
}

static void scrollHeartLabels()
{
  for (HeartLabel &label : labels) {
    if (label.x > 0) {
      label.x -= DISPLAY_POINTS_PER_BLOCK;
    }
  }
}

static void drawHeartLabels()
{
  gfx->setTextColor(COLOR_TEXT, COLOR_BG);
  gfx->setTextSize(1);

  for (const HeartLabel &label : labels) {
    if (label.x > 8 && label.x < SCREEN_W - 12 && label.text[0] != '\0') {
      gfx->setCursor(label.x - 5, HEADER_BOTTOM + 2);
      gfx->print(label.text);
    }
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
  if (strcmp(DEVICE_VERSION, "local-dev") == 0) {
    return;
  }

  if (strlen(OTA_MANIFEST_URL) == 0 || WiFi.status() != WL_CONNECTED) {
    return;
  }

  Serial.println("checking ota manifest");

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  String manifestUrl = OTA_MANIFEST_URL;
  manifestUrl += (manifestUrl.indexOf('?') >= 0) ? "&t=" : "?t=";
  manifestUrl += String(millis());

  if (!http.begin(client, manifestUrl)) {
    Serial.println("ota manifest begin failed");
    return;
  }
  http.addHeader("User-Agent", "MotemaSens-ESP32S3");

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

static void startUsbLiveLog(uint32_t now)
{
  usbLogActive = true;
  usbLogStartMs = now;
  usbLogLastSampleMs = now;
  usbLogSamples = 0;
  usbLogBeats = 0;
  rejectedBeatCandidates = 0;

  Serial.printf("LIVE_TEST_START,version=%s,duration_ms=%lu,sample_ms=%lu\n",
                DEVICE_VERSION,
                (unsigned long)USB_LOG_DURATION_MS,
                (unsigned long)USB_LOG_PERIOD_MS);
  Serial.println("LOG_HEADER,ms,mic_trace,mic_level,beat_envelope,beat_threshold,motion_level,bpm,acc_x_g,acc_y_g,acc_z_g,raw_x,raw_y,raw_z");
}

static void stopUsbLiveLog(uint32_t now, const char *reason)
{
  Serial.printf("LIVE_TEST_END,reason=%s,elapsed_ms=%lu,samples=%lu,beats=%lu,rejected=%lu,bpm=%.1f\n",
                reason,
                (unsigned long)(now - usbLogStartMs),
                (unsigned long)usbLogSamples,
                (unsigned long)usbLogBeats,
                (unsigned long)rejectedBeatCandidates,
                bpm);
  usbLogActive = false;
}

static void handleUsbLogCommands(uint32_t now)
{
  while (Serial.available() > 0) {
    const char command = (char)Serial.read();
    if ((command == 's' || command == 'S') && !usbLogActive) {
      startUsbLiveLog(now);
    } else if ((command == 'x' || command == 'X') && usbLogActive) {
      stopUsbLiveLog(now, "stopped");
    }
  }
}

static void updateUsbLiveLog(uint32_t now)
{
  if (!usbLogActive) {
    return;
  }

  const uint32_t elapsedMs = now - usbLogStartMs;
  if (elapsedMs >= USB_LOG_DURATION_MS) {
    stopUsbLiveLog(now, "complete");
    return;
  }

  if (now - usbLogLastSampleMs < USB_LOG_PERIOD_MS) {
    return;
  }
  usbLogLastSampleMs += USB_LOG_PERIOD_MS;
  if (now - usbLogLastSampleMs > USB_LOG_PERIOD_MS) {
    usbLogLastSampleMs = now;
  }
  ++usbLogSamples;

  Serial.printf("LOG,%lu,%.4f,%.4f,%.4f,%.4f,%.4f,%.1f,%.4f,%.4f,%.4f,%d,%d,%d\n",
                (unsigned long)elapsedMs,
                latestMicPoint,
                smoothedLevel,
                beatEnvelope,
                beatThreshold,
                motionLevel,
                bpm,
                latestAccelX,
                latestAccelY,
                latestAccelZ,
                latestAccelRawX,
                latestAccelRawY,
                latestAccelRawZ);
}

static void drawWaveform()
{
  drawBackground();
  drawHeartLabels();

  int previousY = CENTER_Y;
  for (int x = 0; x < SCREEN_W; ++x) {
    const float value = constrain(wave[x], -1.0f, 1.0f);
    const int halfHeight = safeHalfHeightAtX(x);
    if (halfHeight <= 0) {
      previousY = CENTER_Y;
      continue;
    }

    const int y = clampInt16(CENTER_Y - value * halfHeight * 0.96f, CENTER_Y - halfHeight, CENTER_Y + halfHeight);

    if (x > 0) {
      gfx->drawLine(x - 1, previousY, x, y, COLOR_WAVE);
    }

    previousY = y;
  }

  gfx->flush();
}

static void pushAccelSample(const AccelSample &sample)
{
  memmove(&accelXHistory[0], &accelXHistory[1], sizeof(accelXHistory) - sizeof(accelXHistory[0]));
  memmove(&accelYHistory[0], &accelYHistory[1], sizeof(accelYHistory) - sizeof(accelYHistory[0]));
  memmove(&accelZHistory[0], &accelZHistory[1], sizeof(accelZHistory) - sizeof(accelZHistory[0]));

  if (sample.valid) {
    if (haveLastAccel) {
      const float delta = fabsf(sample.x - lastAccelX) + fabsf(sample.y - lastAccelY) + fabsf(sample.z - lastAccelZ);
      motionLevel = (motionLevel * 0.90f) + (delta * 0.10f);
    } else {
      haveLastAccel = true;
    }
    lastAccelX = sample.x;
    lastAccelY = sample.y;
    lastAccelZ = sample.z;

    latestAccelX = sample.x;
    latestAccelY = sample.y;
    latestAccelZ = sample.z;
    accelXHistory[ACCEL_HISTORY - 1] = constrain(sample.x, -2.0f, 2.0f);
    accelYHistory[ACCEL_HISTORY - 1] = constrain(sample.y, -2.0f, 2.0f);
    accelZHistory[ACCEL_HISTORY - 1] = constrain(sample.z, -2.0f, 2.0f);
  } else {
    accelXHistory[ACCEL_HISTORY - 1] = 0.0f;
    accelYHistory[ACCEL_HISTORY - 1] = 0.0f;
    accelZHistory[ACCEL_HISTORY - 1] = 0.0f;
  }
}

static int accelYToScreen(float value, int halfHeight)
{
  const float normalized = constrain(value / 2.0f, -1.0f, 1.0f);
  return clampInt16(CENTER_Y - normalized * halfHeight * 0.88f, CENTER_Y - halfHeight, CENTER_Y + halfHeight);
}

static void drawTraceInBand(const float *history, float scale, int graphCenterY, int graphHalfHeight, uint16_t color)
{
  int previousY = graphCenterY;
  int previousX = 0;
  bool hasPrevious = false;

  for (int x = 0; x < SCREEN_W; ++x) {
    const float normalized = constrain(history[x] / scale, -1.0f, 1.0f);
    const int y = clampInt16(graphCenterY - normalized * graphHalfHeight,
                             graphCenterY - graphHalfHeight,
                             graphCenterY + graphHalfHeight);
    if (!isSafePixel(x, y)) {
      hasPrevious = false;
      previousX = x;
      previousY = y;
      continue;
    }

    if (hasPrevious && previousX == x - 1) {
      gfx->drawLine(previousX, previousY, x, y, color);
    }
    previousX = x;
    previousY = y;
    hasPrevious = true;
  }
}

static void drawAccelAxis(const float *history, uint16_t color)
{
  drawTraceInBand(history, 2.0f, CENTER_Y, safeHalfHeightAtX(CENTER_X), color);
}

static void drawAccelGraph()
{
  drawBackground();
  drawAccelAxis(accelXHistory, COLOR_X);
  drawAccelAxis(accelYHistory, COLOR_Y);
  drawAccelAxis(accelZHistory, COLOR_Z);
  gfx->flush();
}

static void drawCombinedGraph()
{
  drawBackground();
  drawBandGrid(ACCEL_GRAPH_CENTER_Y, ACCEL_GRAPH_HALF_H);
  drawBandGrid(MIC_GRAPH_CENTER_Y, MIC_GRAPH_HALF_H);

  gfx->setTextSize(1);
  gfx->setTextColor(COLOR_DIM, COLOR_BG);
  gfx->setCursor(45, ACCEL_GRAPH_CENTER_Y - ACCEL_GRAPH_HALF_H - 10);
  gfx->print("ACC");
  gfx->setCursor(45, MIC_GRAPH_CENTER_Y - MIC_GRAPH_HALF_H - 10);
  gfx->print("MIC");

  drawTraceInBand(accelXHistory, 2.0f, ACCEL_GRAPH_CENTER_Y, ACCEL_GRAPH_HALF_H, COLOR_X);
  drawTraceInBand(accelYHistory, 2.0f, ACCEL_GRAPH_CENTER_Y, ACCEL_GRAPH_HALF_H, COLOR_Y);
  drawTraceInBand(accelZHistory, 2.0f, ACCEL_GRAPH_CENTER_Y, ACCEL_GRAPH_HALF_H, COLOR_Z);
  drawTraceInBand(wave, 1.0f, MIC_GRAPH_CENTER_Y, MIC_GRAPH_HALF_H, COLOR_WAVE);

  gfx->flush();
}

static void pushWavePoint(float sample)
{
  memmove(&wave[0], &wave[1], sizeof(wave) - sizeof(wave[0]));
  const float softened = (wave[SCREEN_W - 2] * 0.18f) + (sample * 0.82f);
  wave[SCREEN_W - 1] = constrain(softened, -1.0f, 1.0f);
  scrollHeartLabels();
}

static void updateBeatDetector(float energy)
{
  const uint32_t now = millis();
  const bool motionQuiet = motionLevel < MOTION_GATE_THRESHOLD_G;
  const float gatedEnergy = (energy < HEART_NOISE_GATE) ? 0.0f : energy;

  beatEnvelope = (beatEnvelope * 0.86f) + (gatedEnergy * 0.14f);
  beatPeak = max(beatEnvelope, beatPeak * 0.985f);
  beatFloor = (beatFloor * 0.997f) + (beatEnvelope * 0.003f);

  const float dynamicMargin = max(0.14f, (beatPeak - beatFloor) * 0.46f);
  const float targetThreshold = constrain(beatFloor + dynamicMargin, 0.26f, 0.62f);
  beatThreshold = (beatThreshold * 0.90f) + (targetThreshold * 0.10f);

  const float resetLevel = max(0.16f, beatThreshold * 0.62f);
  if (beatEnvelope < resetLevel) {
    beatArmed = true;
  }

  if (beatCandidateActive) {
    if (beatEnvelope > beatCandidatePeakLevel) {
      beatCandidatePeakLevel = beatEnvelope;
      beatCandidatePeakMs = now;
      beatCandidatePeakGain = autoGain;
    }

    const bool peakHasSettled = beatEnvelope < beatCandidatePeakLevel * 0.78f;
    const bool peakWindowExpired = now - beatCandidateStartMs >= HEART_PEAK_HOLD_MS;
    if (!motionQuiet) {
      discardBeatCandidate();
    } else if (peakHasSettled || peakWindowExpired) {
      confirmBeatCandidate();
    }
  }

  const bool beatCandidate = motionQuiet &&
                             beatArmed &&
                             !beatCandidateActive &&
                             beatEnvelope > beatThreshold &&
                             gatedEnergy > beatThreshold &&
                             beatEnvelope > beatFloor + 0.12f;
  if (!beatCandidate || now - lastBeatMs < HEART_REFRACTORY_MS) {
    return;
  }

  beatCandidateActive = true;
  beatCandidateStartMs = now;
  beatCandidatePeakMs = now;
  beatCandidatePeakLevel = beatEnvelope;
  beatCandidatePeakGain = autoGain;
}

static bool qmiWriteRegister(uint8_t reg, uint8_t value)
{
  Wire.beginTransmission(imuAddress);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

static bool qmiReadRegister(uint8_t reg, uint8_t *buffer, uint8_t length)
{
  Wire.beginTransmission(imuAddress);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  const uint8_t received = Wire.requestFrom(imuAddress, length);
  if (received != length) {
    return false;
  }

  for (uint8_t i = 0; i < length; ++i) {
    buffer[i] = Wire.read();
  }
  return true;
}

static bool qmiReadByte(uint8_t address, uint8_t reg, uint8_t &value)
{
  Wire.beginTransmission(address);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  if (Wire.requestFrom(address, (uint8_t)1) != 1) {
    return false;
  }

  value = Wire.read();
  return true;
}

static int16_t qmiInt16(uint8_t lo, uint8_t hi)
{
  return (int16_t)(((uint16_t)hi << 8) | lo);
}

static bool setupQmi8658()
{
  Wire.begin(IMU_SDA, IMU_SCL);
  Wire.setClock(400000);
  delay(20);

  uint8_t who = 0;
  for (uint8_t address : { (uint8_t)0x6A, (uint8_t)0x6B }) {
    if (qmiReadByte(address, 0x00, who) && who == 0x05) {
      imuAddress = address;
      break;
    }
  }

  if (who != 0x05) {
    Serial.println("QMI8658 not found");
    return false;
  }

  qmiWriteRegister(0x02, 0x80);
  delay(50);

  // Follow the common QMI8658 driver sequence:
  // CTRL1 auto-increment, CTRL2 accel +/-8g at 1000 Hz, CTRL3 gyro 512 dps at 1000 Hz.
  // Gyro is enabled too because this keeps the standard 12-byte sensor data path active.
  const bool ok = qmiWriteRegister(0x02, 0x60) &&
                  qmiWriteRegister(0x08, 0x00) &&
                  qmiWriteRegister(0x03, 0x23) &&
                  qmiWriteRegister(0x04, 0x43) &&
                  qmiWriteRegister(0x08, 0x03);
  if (ok) {
    Serial.printf("QMI8658 ready at 0x%02X, accel +/-8g 1000Hz\n", imuAddress);
  } else {
    Serial.println("QMI8658 init failed");
  }
  return ok;
}

static bool readQmi8658Accel(float &x, float &y, float &z)
{
  uint8_t status = 0;
  if (!qmiReadRegister(0x2E, &status, 1) || (status & 0x01) == 0) {
    return false;
  }

  uint8_t buffer[6] = {};
  if (!qmiReadRegister(0x35, buffer, sizeof(buffer))) {
    return false;
  }

  const int16_t rawX = qmiInt16(buffer[0], buffer[1]);
  const int16_t rawY = qmiInt16(buffer[2], buffer[3]);
  const int16_t rawZ = qmiInt16(buffer[4], buffer[5]);

  latestAccelRawX = rawX;
  latestAccelRawY = rawY;
  latestAccelRawZ = rawZ;

  // +/-8g full scale uses 4096 LSB/g.
  x = (float)rawX / 4096.0f;
  y = (float)rawY / 4096.0f;
  z = (float)rawZ / 4096.0f;
  return true;
}

static void readAccelSample()
{
  if (!imuReady) {
    return;
  }

  const uint32_t now = millis();
  if (now - lastAccelReadMs < ACCEL_SAMPLE_PERIOD_MS) {
    return;
  }
  lastAccelReadMs = now;

  AccelSample sample = {};
  sample.valid = readQmi8658Accel(sample.x, sample.y, sample.z);
  if (sample.valid) {
    ++accelSamplesSinceDebug;
    sendAccelSample(sample);
  } else {
    ++accelReadFailuresSinceDebug;
  }

  if (USB_LOG_ACCEL_DEBUG && usbLogActive && now - lastAccelDebugMs >= ACCEL_DEBUG_PERIOD_MS) {
    const float magnitude = sqrtf((sample.x * sample.x) + (sample.y * sample.y) + (sample.z * sample.z));
    Serial.printf("ACCEL_DEBUG,hz=%lu,fail=%lu,raw=%d,%d,%d,g=%.2f,%.2f,%.2f,mag=%.2f\n",
                  (unsigned long)accelSamplesSinceDebug,
                  (unsigned long)accelReadFailuresSinceDebug,
                  latestAccelRawX, latestAccelRawY, latestAccelRawZ,
                  sample.x, sample.y, sample.z, magnitude);
    if (accelRegisterDumpsLeft > 0) {
      uint8_t regs[14] = {};
      if (qmiReadRegister(0x30, regs, sizeof(regs))) {
        Serial.print("qmi 30-3D=");
        for (uint8_t i = 0; i < sizeof(regs); ++i) {
          if (regs[i] < 16) {
            Serial.print('0');
          }
          Serial.print(regs[i], HEX);
          Serial.print(i == sizeof(regs) - 1 ? '\n' : ' ');
        }
        --accelRegisterDumpsLeft;
      }
    }
    accelSamplesSinceDebug = 0;
    accelReadFailuresSinceDebug = 0;
    lastAccelDebugMs = now;
  }
}

static void readMicSamples()
{
  int32_t samples[AUDIO_BLOCK_SAMPLES];
  size_t bytesRead = 0;
  const esp_err_t result = i2s_read(I2S_PORT, samples, sizeof(samples), &bytesRead, pdMS_TO_TICKS(8));
  if (result != ESP_OK || bytesRead == 0) {
    sendDisplayPoint(0.0f);
    return;
  }

  const size_t count = bytesRead / sizeof(samples[0]);
  const size_t chunkSize = max<size_t>(1, count / DISPLAY_POINTS_PER_BLOCK);
  float sumSq = 0.0f;
  float absolutePeak = 0.0f;
  float chunkPeaks[DISPLAY_POINTS_PER_BLOCK] = {};
  float chunkSignedPeaks[DISPLAY_POINTS_PER_BLOCK] = {};

  for (size_t i = 0; i < count; ++i) {
    // SPH0645 data is 24-bit I2S in a 32-bit slot. Keep the low-frequency body sound band.
    const float sample = (float)(samples[i] >> 14);
    dc += 0.00020f * (sample - dc);
    const float centered = sample - dc;

    lowFast += 0.052f * (centered - lowFast);
    lowSlow += 0.006f * (centered - lowSlow);
    const float heartBand = lowFast - lowSlow;

    sumSq += heartBand * heartBand;

    const float magnitude = fabsf(heartBand);
    if (magnitude > absolutePeak) {
      absolutePeak = magnitude;
    }

    size_t chunk = i / chunkSize;
    if (chunk >= DISPLAY_POINTS_PER_BLOCK) {
      chunk = DISPLAY_POINTS_PER_BLOCK - 1;
    }
    if (magnitude > chunkPeaks[chunk]) {
      chunkPeaks[chunk] = magnitude;
      chunkSignedPeaks[chunk] = heartBand;
    }
  }

  const float rms = sqrtf(sumSq / max<size_t>(1, count));
  float normalized = max(rms / 4200.0f, absolutePeak / 16000.0f);

  // Heart sounds are much smaller than speech. Keep AGC slow so each beat shape stays natural.
  const float target = normalized * autoGain;
  if (target > 0.48f) {
    autoGain *= 0.965f;
  } else if (target < 0.13f) {
    autoGain *= 1.0015f;
  }
  autoGain = constrain(autoGain, 3.0f, 36.0f);

  normalized = constrain(normalized * autoGain, 0.0f, 1.0f);
  smoothedLevel = (smoothedLevel * 0.88f) + (normalized * 0.12f);
  updateBeatDetector(smoothedLevel);

  for (size_t i = 0; i < DISPLAY_POINTS_PER_BLOCK; ++i) {
    const float sign = (chunkSignedPeaks[i] >= 0.0f) ? 1.0f : -1.0f;
    float displayPoint = constrain((chunkPeaks[i] / 12000.0f) * autoGain, 0.0f, 1.0f);
    displayPoint = constrain(powf(displayPoint, 0.68f) * 0.86f, 0.0f, 0.96f) * sign;
    displaySample += 0.70f * (displayPoint - displaySample);
    sendDisplayPoint(displaySample);
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

static void updateDeviceHeartbeatLed(uint32_t now)
{
  if (now - lastLedToggleMs < 1000) {
    return;
  }

  lastLedToggleMs = now;
  ledGreenOn = !ledGreenOn;
  digitalWrite(LED_GREEN, ledGreenOn ? HIGH : LOW);
}

static void drainAcquisitionQueues()
{
  float point = 0.0f;
  while (xQueueReceive(displayPointQueue, &point, 0) == pdTRUE) {
    latestMicPoint = point;
    pushWavePoint(point);
  }

  BeatEvent event = {};
  while (xQueueReceive(beatEventQueue, &event, 0) == pdTRUE) {
    addHeartLabel();
    bpm = event.bpm;
    if (usbLogActive) {
      ++usbLogBeats;
      const uint32_t eventElapsedMs = (event.beatMs >= usbLogStartMs) ? (event.beatMs - usbLogStartMs) : 0;
      const uint32_t outputDelayMs = millis() - event.beatMs;
      Serial.printf("BEAT,%lu,interval_ms=%lu,bpm=%.1f,level=%.4f,gain=%.1f,delay_ms=%lu\n",
                    (unsigned long)eventElapsedMs,
                    (unsigned long)event.intervalMs,
                    event.bpm,
                    event.level,
                    event.gain,
                    (unsigned long)outputDelayMs);
    }
  }

  AccelSample sample = {};
  while (xQueueReceive(accelSampleQueue, &sample, 0) == pdTRUE) {
    pushAccelSample(sample);
  }
}

static void acquisitionTask(void *)
{
  for (;;) {
    readAccelSample();
    readMicSamples();
    readAccelSample();
  }
}

static void outputTask(void *)
{
  for (;;) {
    const uint32_t now = millis();
    updateDeviceHeartbeatLed(now);
    handleUsbLogCommands(now);

    if (!otaCheckedOnce || now - lastOtaCheckMs >= 60000) {
      otaCheckedOnce = true;
      lastOtaCheckMs = now;
      checkForOtaUpdate();
    }

    drainAcquisitionQueues();
    updateUsbLiveLog(now);

    if (!usbLogActive && now - lastDrawMs >= 24) {
      lastDrawMs = now;
      drawCombinedGraph();
    }

    vTaskDelay(pdMS_TO_TICKS(usbLogActive ? 1 : 2));
  }
}

void setup()
{
  Serial.begin(115200);
  delay(200);
  Serial.printf("device version: %s\n", DEVICE_VERSION);

  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);
  pinMode(LED_GREEN, OUTPUT);
  digitalWrite(LED_GREEN, LOW);

  if (!gfx->begin(80000000)) {
    Serial.println("LCD begin failed");
  }

  drawBackground();
  gfx->flush();
  connectWifi();
  drawBackground();
  gfx->flush();
  imuReady = setupQmi8658();
  setupI2S();

  for (float &point : wave) {
    point = 0.0f;
  }
  for (float &point : accelXHistory) {
    point = 0.0f;
  }
  for (float &point : accelYHistory) {
    point = 0.0f;
  }
  for (float &point : accelZHistory) {
    point = 0.0f;
  }

  displayPointQueue = xQueueCreate(96, sizeof(float));
  beatEventQueue = xQueueCreate(12, sizeof(BeatEvent));
  accelSampleQueue = xQueueCreate(64, sizeof(AccelSample));
  if (!displayPointQueue || !beatEventQueue || !accelSampleQueue) {
    Serial.println("queue create failed");
    ESP.restart();
  }

  xTaskCreatePinnedToCore(acquisitionTask, "acquisition", ACQUISITION_TASK_STACK, nullptr,
                          ACQUISITION_TASK_PRIORITY, nullptr, ACQUISITION_CORE);
  xTaskCreatePinnedToCore(outputTask, "output", OUTPUT_TASK_STACK, nullptr,
                          OUTPUT_TASK_PRIORITY, nullptr, OUTPUT_CORE);

  Serial.printf("mic and accel test running, acquisition core=%u output core=%u\n",
                ACQUISITION_CORE, OUTPUT_CORE);
  Serial.println("usb live test ready: send S for 60 seconds, X to stop");
}

void loop()
{
  vTaskDelay(pdMS_TO_TICKS(1000));
}
