#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <math.h>

#include "accel_sensor.h"
#include "debug_serial.h"
#include "ecg_ads1294.h"
#include "mic_sensor.h"
#include "motemasens_logo_mask.h"
#include "sensor_config.h"
#include "spi_display_guard.h"

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

static constexpr size_t DISPLAY_POINTS_PER_BLOCK = 2;
static constexpr size_t MIC_DISPLAY_SCROLL_DIVIDER = 4;
static constexpr size_t LABEL_HISTORY = 8;
static constexpr uint32_t ACCEL_DEBUG_PERIOD_MS = 1000;
static constexpr uint32_t USB_LOG_DURATION_MS = 60000;
static constexpr uint32_t USB_LOG_PERIOD_MS = 10;
static constexpr uint32_t ECG_DIAG_DEBUG_PERIOD_MS = 1000;
static constexpr uint16_t WIFI_LOG_PORT = 80;
static constexpr uint32_t WIFI_LOG_PERIOD_MS = 10;
static constexpr uint32_t WIFI_CONNECT_BOOT_WAIT_MS = 3000;
static constexpr uint32_t SYSTEM_STATUS_PERIOD_MS = 1000;
static constexpr uint32_t HEART_REFRACTORY_MS = 720;
static constexpr uint32_t HEART_PEAK_HOLD_MS = 180;
static constexpr uint32_t HEARTBEAT_LED_UPDATE_MS = 20;
static constexpr uint32_t HEARTBEAT_LED_PERIOD_MS = 1600;
static constexpr uint8_t HEARTBEAT_GREEN_PWM_CHANNEL = 0;
static constexpr uint8_t HEARTBEAT_BLUE_PWM_CHANNEL = 1;
static constexpr uint32_t HEARTBEAT_LED_PWM_FREQ_HZ = 5000;
static constexpr uint8_t HEARTBEAT_LED_PWM_BITS = 8;
static constexpr uint8_t HEARTBEAT_LED_MIN_DUTY = 4;
static constexpr uint8_t HEARTBEAT_LED_MAX_DUTY = 120;
static constexpr uint32_t STARTUP_SCREEN_MS = 3000;
static constexpr uint32_t STARTUP_FRAME_MS = 50;
static constexpr bool USB_LOG_ACCEL_DEBUG = false;
static constexpr float MOTION_GATE_THRESHOLD_G = 0.075f;
static constexpr float HEART_NOISE_GATE = 0.025f;
static constexpr uint8_t ACQUISITION_CORE = 0;
static constexpr uint8_t OUTPUT_CORE = 1;
static constexpr uint32_t ACQUISITION_TASK_STACK = 8192;
static constexpr uint32_t OUTPUT_TASK_STACK = 12288;
static constexpr UBaseType_t ACQUISITION_TASK_PRIORITY = 4;
static constexpr UBaseType_t OUTPUT_TASK_PRIORITY = 1;

static constexpr int SCREEN_W = 240;
static constexpr int SCREEN_H = 240;
static constexpr size_t ACCEL_HISTORY = SCREEN_W;
static constexpr int CENTER_X = SCREEN_W / 2;
static constexpr int CENTER_Y = SCREEN_H / 2;
static constexpr int SAFE_RADIUS = 102;
static constexpr int HEADER_BOTTOM = 36;
static constexpr int WAVE_MARGIN = 12;
static constexpr int ECG_GRAPH_CENTER_Y = 90;
static constexpr int ECG_GRAPH_HALF_H = 41;
static constexpr int MIC_GRAPH_CENTER_Y = 150;
static constexpr int MIC_GRAPH_HALF_H = 17;
static constexpr int ACCEL_GRAPH_CENTER_Y = 181;
static constexpr int ACCEL_GRAPH_HALF_H = 13;

static constexpr uint16_t COLOR_BG = 0x2A4B;
static constexpr uint16_t COLOR_GRID = 0x3B6D;
static constexpr uint16_t COLOR_WAVE = 0xDDFB;
static constexpr uint16_t COLOR_TEXT = 0xFFFF;
static constexpr uint16_t COLOR_DIM = 0x9D76;
static constexpr uint16_t COLOR_SPLASH_BG = 0x6B4D;
static constexpr uint16_t COLOR_SPLASH_YELLOW = 0xFFE0;
static constexpr uint16_t COLOR_X = 0xF986;
static constexpr uint16_t COLOR_Y = 0x87F0;
static constexpr uint16_t COLOR_Z = 0x7DFF;
static constexpr uint16_t COLOR_WIFI = 0x07E0;

Arduino_DataBus *bus = new Arduino_ESP32SPI(LCD_DC, LCD_CS, LCD_SCLK, LCD_MOSI, GFX_NOT_DEFINED, SPI2_HOST);
Arduino_GFX *display = new Arduino_GC9A01(bus, LCD_RST, 0, true);
Arduino_Canvas *gfx = new Arduino_Canvas(SCREEN_W, SCREEN_H, display);
WiFiServer wifiLogServer(WIFI_LOG_PORT);

static float wave[SCREEN_W];
static float ecgWave[SCREEN_W];
static float accelXHistory[ACCEL_HISTORY];
static float accelYHistory[ACCEL_HISTORY];
static float accelZHistory[ACCEL_HISTORY];
static float latestAccelX = 0.0f;
static float latestAccelY = 0.0f;
static float latestAccelZ = 0.0f;
static float smoothedLevel = 0.0f;
static float beatEnvelope = 0.0f;
static float beatFloor = 0.0f;
static float beatThreshold = 0.0f;
static float beatPeak = 0.0f;
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
static uint32_t lastHeartbeatLedMs = 0;
static uint32_t lastOtaCheckMs = 0;
static bool otaCheckedOnce = false;
static uint32_t lastAccelReadMs = 0;
static uint32_t lastAccelDebugMs = 0;
static uint32_t lastEcgDiagDebugMs = 0;
static uint32_t accelSamplesSinceDebug = 0;
static uint32_t accelReadFailuresSinceDebug = 0;
static int16_t latestAccelRawX = 0;
static int16_t latestAccelRawY = 0;
static int16_t latestAccelRawZ = 0;
static uint32_t latestAccelTimestampMs = 0;
static uint8_t latestAccelAcqSeq8 = 0;
static uint8_t latestAccelDiagnosticFlags = 0;
static float latestMicPoint = 0.0f;
static float pendingMicDisplaySum = 0.0f;
static size_t pendingMicDisplayCount = 0;
static uint32_t latestMicTimestampMs = 0;
static uint8_t latestMicAcqSeq8 = 0;
static int32_t latestEcgCh1 = 0;
static int32_t latestEcgCh2 = 0;
static int32_t latestEcgCh3 = 0;
static int32_t latestEcgCh4 = 0;
static uint32_t latestEcgStatus = 0;
static uint32_t latestEcgSequence = 0;
static uint8_t latestEcgLeadOffPositive = 0;
static uint8_t latestEcgLeadOffNegative = 0;
static uint8_t latestEcgSaturationMask = 0;
static uint16_t latestEcgDiagnosticFlags = 0;
static float latestEcgCommonModeStep = 0.0f;
static float latestEcgDifferentialStep = 0.0f;
static uint32_t latestEcgTimestampMs = 0;
static uint8_t latestEcgAcqSeq8 = 0;
static float ecgBaseline = 0.0f;
static float ecgDisplayFiltered = 0.0f;
static float ecgDisplayNoise = 5000.0f;
static float ecgScale = 80000.0f;
static bool ecgDisplayReady = false;
static bool usbLogActive = false;
static uint32_t usbLogStartMs = 0;
static uint32_t usbLogLastSampleMs = 0;
static uint32_t usbLogSamples = 0;
static uint32_t usbLogBeats = 0;
static bool wifiLogServerStarted = false;
static bool wifiLogActive = false;
static bool wifiStreamHeaderSent = false;
static uint32_t wifiLogStartMs = 0;
static uint32_t wifiLogLastSampleMs = 0;
static uint32_t wifiLogSamples = 0;
static uint32_t wifiLogBeats = 0;
static volatile uint32_t core0LoopCounter = 0;
static uint8_t core0AcqSeq8 = 0;
static volatile uint32_t core1LoopCounter = 0;
static volatile uint32_t core1BusyMicros = 0;
static uint32_t lastSystemStatusMs = 0;
static uint32_t lastCore0LoopCounter = 0;
static uint32_t lastCore1LoopCounter = 0;
static uint32_t lastCore1BusyMicros = 0;
static uint32_t core0SpeedHz = 0;
static uint32_t core1SpeedHz = 0;
static uint8_t core1UsagePct = 0;
static uint8_t ramUsagePct = 0;
static uint8_t flashUsagePct = 0;
static uint32_t freeHeapKb = 0;
static uint32_t usedSketchKb = 0;
static WiFiClient wifiStreamClient;
static Preferences preferences;
static MicSensor micSensor;
static AccelSensor accelSensor;
static EcgAds1294 ecgSensor;
static QueueHandle_t micFrameQueue = nullptr;
static QueueHandle_t beatEventQueue = nullptr;
static QueueHandle_t accelSampleQueue = nullptr;
static QueueHandle_t ecgSampleQueue = nullptr;

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

static HeartLabel labels[LABEL_HISTORY] = {};
static bool nextLabelIsS1 = true;

static int16_t clampInt16(float value, int16_t minValue, int16_t maxValue)
{
  if (value < minValue) return minValue;
  if (value > maxValue) return maxValue;
  return (int16_t)value;
}

static void flushDisplay()
{
  lockSpiDisplayGuard();
  gfx->flush();
  unlockSpiDisplayGuard();
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
  if (version.length() > 18) {
    version = version.substring(0, 18);
  }
  return version;
}

static String displayBuildId()
{
  String version = DEVICE_VERSION;
  if (version.startsWith("dev-2026.06.13.")) {
    return "V13." + version.substring(15);
  }
  if (version.startsWith("dev-")) {
    return "DEV";
  }
  return shortDeviceVersion();
}

static bool isDevVersion()
{
  String version = DEVICE_VERSION;
  return version == "local-dev" || version.startsWith("dev-");
}

static void drawBackground()
{
  gfx->fillScreen(COLOR_BG);
}

static void drawStartupIcon()
{
  const uint16_t c = COLOR_SPLASH_YELLOW;
  const int startX = CENTER_X - (MOTEMASENS_LOGO_W / 2);
  const int startY = 27;

  for (int y = 0; y < MOTEMASENS_LOGO_H; ++y) {
    for (int x = 0; x < MOTEMASENS_LOGO_W; ++x) {
      const int byteIndex = y * MOTEMASENS_LOGO_BYTES_PER_ROW + (x / 8);
      const uint8_t maskByte = pgm_read_byte(&MOTEMASENS_LOGO_MASK[byteIndex]);
      if (maskByte & (0x80 >> (x & 7))) {
        gfx->drawPixel(startX + x, startY + y, c);
      }
    }
  }
}

static void drawCenteredText(const char *text, int y, uint8_t size, uint16_t color)
{
  const int width = strlen(text) * 6 * size;
  gfx->setTextSize(size);
  gfx->setTextColor(color, COLOR_SPLASH_BG);
  gfx->setCursor((SCREEN_W - width) / 2, y);
  gfx->print(text);
}

static void drawStartupScreen(uint8_t progress)
{
  gfx->fillScreen(COLOR_SPLASH_BG);
  drawStartupIcon();
  drawCenteredText("MotemaSens", 156, 3, COLOR_SPLASH_YELLOW);
  drawCenteredText("INITIALISING", 194, 1, COLOR_TEXT);
  drawCenteredText(DEVICE_VERSION, 207, 1, COLOR_DIM);

  const int barX = 42;
  const int barY = 222;
  const int barW = 156;
  const int barH = 7;
  gfx->drawRect(barX, barY, barW, barH, COLOR_SPLASH_YELLOW);
  const int fillW = ((barW - 2) * progress) / 100;
  if (fillW > 0) {
    gfx->fillRect(barX + 1, barY + 1, fillW, barH - 2, COLOR_SPLASH_YELLOW);
  }
  flushDisplay();
}

static void runStartupScreen()
{
  const uint32_t startMs = millis();
  while (millis() - startMs < STARTUP_SCREEN_MS) {
    const uint32_t elapsed = millis() - startMs;
    uint32_t progress = (elapsed * 100) / STARTUP_SCREEN_MS;
    if (progress > 100) {
      progress = 100;
    }
    drawStartupScreen((uint8_t)progress);
    vTaskDelay(pdMS_TO_TICKS(STARTUP_FRAME_MS));
  }
  drawStartupScreen(100);
}

static void drawTopStatus()
{
  gfx->setTextSize(1);
  const String buildId = displayBuildId();

  gfx->setTextColor(COLOR_TEXT, COLOR_BG);
  gfx->setCursor(84, 12);
  gfx->print(buildId);

  if (WiFi.status() == WL_CONNECTED) {
    gfx->fillCircle(154, 15, 2, COLOR_WIFI);
  }

  gfx->setTextColor(ecgSensor.ready() ? COLOR_WIFI : COLOR_X, COLOR_BG);
  gfx->setCursor(44, 25);
  gfx->print("ECG");

  if (!ecgSensor.ready()) {
    gfx->setTextColor(COLOR_X, COLOR_BG);
    gfx->print(" NOADS");
#if ENABLE_ECG_INTERNAL_TEST_SIGNAL
  } else {
    gfx->setTextColor(COLOR_Y, COLOR_BG);
    gfx->print(" TEST");
  }
#else
  } else if (latestEcgDiagnosticFlags & ECG_DIAG_LEAD_OFF) {
    gfx->setTextColor(COLOR_X, COLOR_BG);
    gfx->print(" LO");
  } else if (latestEcgDiagnosticFlags & ECG_DIAG_DC_SATURATION) {
    gfx->setTextColor(COLOR_X, COLOR_BG);
    gfx->print(" SAT");
  } else if (latestEcgDiagnosticFlags & ECG_DIAG_RLD_UNSTABLE) {
    gfx->setTextColor(COLOR_X, COLOR_BG);
    gfx->print(" RLD");
  } else {
    gfx->setTextColor(COLOR_DIM, COLOR_BG);
    gfx->print(" OK");
  }
#endif

  gfx->setCursor(124, 25);
  gfx->setTextColor(COLOR_DIM, COLOR_BG);
  if (bpm > 0.0f) {
    gfx->print((int)bpm);
    gfx->print(" BPM");
  } else {
    gfx->print("BPM --");
  }

  if (usbLogActive || wifiLogActive) {
    gfx->setTextColor(COLOR_WIFI, COLOR_BG);
    gfx->setCursor(96, 25);
    gfx->print("LOG");
  }
}

static uint8_t nextAcquisitionSeq8()
{
  return ++core0AcqSeq8;
}

static void updateSystemStatus(uint32_t now)
{
  if (lastSystemStatusMs != 0 && now - lastSystemStatusMs < SYSTEM_STATUS_PERIOD_MS) {
    return;
  }

  const uint32_t elapsedMs = (lastSystemStatusMs == 0) ? SYSTEM_STATUS_PERIOD_MS : now - lastSystemStatusMs;
  lastSystemStatusMs = now;

  const uint32_t core0Loops = core0LoopCounter;
  const uint32_t core1Loops = core1LoopCounter;
  const uint32_t core1Busy = core1BusyMicros;
  const uint32_t core0Delta = core0Loops - lastCore0LoopCounter;
  const uint32_t core1Delta = core1Loops - lastCore1LoopCounter;
  const uint32_t core1BusyDelta = core1Busy - lastCore1BusyMicros;
  lastCore0LoopCounter = core0Loops;
  lastCore1LoopCounter = core1Loops;
  lastCore1BusyMicros = core1Busy;

  const uint32_t safeElapsedMs = max((uint32_t)1, elapsedMs);
  core0SpeedHz = (core0Delta * 1000UL) / safeElapsedMs;
  core1SpeedHz = (core1Delta * 1000UL) / safeElapsedMs;
  core1UsagePct = constrain((core1BusyDelta / 10UL) / safeElapsedMs, 0UL, 100UL);

  const uint32_t heapSize = ESP.getHeapSize();
  const uint32_t freeHeap = ESP.getFreeHeap();
  freeHeapKb = freeHeap / 1024UL;
  ramUsagePct = heapSize > 0 ? constrain(((heapSize - freeHeap) * 100UL) / heapSize, 0UL, 100UL) : 0;

  const uint32_t sketchSize = ESP.getSketchSize();
  const uint32_t sketchSpace = ESP.getFreeSketchSpace() + sketchSize;
  usedSketchKb = sketchSize / 1024UL;
  flashUsagePct = sketchSpace > 0 ? constrain((sketchSize * 100UL) / sketchSpace, 0UL, 100UL) : 0;
}

static void drawSystemStatusPanel()
{
  gfx->setTextSize(1);
  gfx->setTextColor(COLOR_DIM, COLOR_BG);

  gfx->setCursor(43, 206);
  gfx->print("C0 ");
  gfx->print(core0SpeedHz / 1000UL);
  gfx->print("k");

  gfx->setCursor(43, 218);
  gfx->print("C1 ");
  gfx->print(core1SpeedHz);
  gfx->print("h");

  gfx->setCursor(132, 206);
  gfx->print("R ");
  gfx->print(ramUsagePct);
  gfx->print("%");

  gfx->setCursor(132, 218);
  gfx->print("M ");
  gfx->print(flashUsagePct);
  gfx->print("%");
  gfx->setTextColor(COLOR_TEXT, COLOR_BG);
}

static void drawBandGrid(int graphCenterY, int graphHalfHeight)
{
  const int top = graphCenterY - graphHalfHeight;
  const int bottom = graphCenterY + graphHalfHeight;

  for (int x = 42; x < SCREEN_W - 36; x += 28) {
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

static void sendMicFrame(const MicFrame &frame)
{
  if (!micFrameQueue) {
    return;
  }

  if (xQueueSend(micFrameQueue, &frame, 0) != pdTRUE) {
    MicFrame dropped = {};
    xQueueReceive(micFrameQueue, &dropped, 0);
    xQueueSend(micFrameQueue, &frame, 0);
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
  if (intervalMs < HEART_REFRACTORY_MS) {
    ++rejectedBeatCandidates;
    return;
  }

  if (intervalMs > 1800) {
    ++rejectedBeatCandidates;
    lastBeatMs = beatMs;
    acquisitionBpm = 0.0f;
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

static const char *logHeader()
{
  return "LOG_HEADER,ms,mic_ms,mic_seq8,acc_ms,acc_seq8,ecg_ms,ecg_seq8,mic_trace,mic_level,beat_envelope,beat_threshold,motion_level,bpm,acc_x_g,acc_y_g,acc_z_g,raw_x,raw_y,raw_z,acc_diag_flags,ecg_seq,ecg_status,ecg_ch1,ecg_ch2,ecg_ch3,ecg_ch4,ecg_lead_off_p,ecg_lead_off_n,ecg_sat_mask,ecg_diag_flags,ecg_common_step,ecg_diff_step";
}

static int formatLogRow(char *buffer, size_t length, uint32_t elapsedMs)
{
  return snprintf(buffer, length,
                  "LOG,%lu,%lu,%u,%lu,%u,%lu,%u,%.4f,%.4f,%.4f,%.4f,%.4f,%.1f,%.4f,%.4f,%.4f,%d,%d,%d,%02X,%lu,%06lX,%ld,%ld,%ld,%ld,%02X,%02X,%02X,%04X,%.1f,%.1f\n",
                  (unsigned long)elapsedMs,
                  (unsigned long)latestMicTimestampMs,
                  (unsigned)latestMicAcqSeq8,
                  (unsigned long)latestAccelTimestampMs,
                  (unsigned)latestAccelAcqSeq8,
                  (unsigned long)latestEcgTimestampMs,
                  (unsigned)latestEcgAcqSeq8,
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
                  latestAccelRawZ,
                  (unsigned)latestAccelDiagnosticFlags,
                  (unsigned long)latestEcgSequence,
                  (unsigned long)latestEcgStatus,
                  (long)latestEcgCh1,
                  (long)latestEcgCh2,
                  (long)latestEcgCh3,
                  (long)latestEcgCh4,
                  (unsigned)latestEcgLeadOffPositive,
                  (unsigned)latestEcgLeadOffNegative,
                  (unsigned)latestEcgSaturationMask,
                  (unsigned)latestEcgDiagnosticFlags,
                  latestEcgCommonModeStep,
                  latestEcgDifferentialStep);
}

static void updateLoggingLed()
{
  // Blue LED is now part of the always-on alternating heartbeat animation.
}

static bool loggingActive()
{
  return usbLogActive || wifiLogActive;
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

static void startWifiServerIfConnected()
{
  if (wifiLogServerStarted || WiFi.status() != WL_CONNECTED) {
    return;
  }

  DebugSerial.print("wifi connected, ip=");
  DebugSerial.println(WiFi.localIP());
  wifiLogServer.begin();
  wifiLogServerStarted = true;
  DebugSerial.printf("wifi logger ready: http://%s/\n", WiFi.localIP().toString().c_str());
}

static void connectWifi()
{
  saveLocalWifiCredentials();

  String ssid;
  String password;
  if (!loadWifiCredentials(ssid, password)) {
    DebugSerial.println("wifi not configured");
    return;
  }

  DebugSerial.printf("connecting wifi: %s\n", ssid.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());

  const uint32_t startMs = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startMs < WIFI_CONNECT_BOOT_WAIT_MS) {
    delay(250);
    DebugSerial.print(".");
  }
  DebugSerial.println();

  startWifiServerIfConnected();
  if (!wifiLogServerStarted) {
    DebugSerial.println("wifi connect pending, UI starting");
  }
}

static bool downloadAndApplyFirmware(const String &firmwareUrl)
{
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  if (!http.begin(client, firmwareUrl)) {
    DebugSerial.println("ota firmware begin failed");
    return false;
  }
  http.addHeader("User-Agent", "MotemaSens-ESP32S3");

  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    DebugSerial.printf("ota firmware http code=%d\n", code);
    http.end();
    return false;
  }

  const int contentLength = http.getSize();
  if (contentLength <= 0) {
    DebugSerial.println("ota firmware size invalid");
    http.end();
    return false;
  }

  if (!Update.begin(contentLength)) {
    DebugSerial.println("ota update begin failed");
    http.end();
    return false;
  }

  WiFiClient *stream = http.getStreamPtr();
  const size_t written = Update.writeStream(*stream);
  const bool ok = (written == (size_t)contentLength) && Update.end(true);
  http.end();

  if (!ok) {
    DebugSerial.printf("ota failed, written=%u expected=%d error=%d\n", (unsigned)written, contentLength, Update.getError());
    return false;
  }

  DebugSerial.println("ota done, rebooting");
  delay(500);
  ESP.restart();
  return true;
}

static void checkForOtaUpdate()
{
  if (isDevVersion()) {
    return;
  }

  if (strlen(OTA_MANIFEST_URL) == 0 || WiFi.status() != WL_CONNECTED) {
    return;
  }

  DebugSerial.println("checking ota manifest");

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  String manifestUrl = OTA_MANIFEST_URL;
  manifestUrl += (manifestUrl.indexOf('?') >= 0) ? "&t=" : "?t=";
  manifestUrl += String(millis());

  if (!http.begin(client, manifestUrl)) {
    DebugSerial.println("ota manifest begin failed");
    return;
  }
  http.addHeader("User-Agent", "MotemaSens-ESP32S3");

  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    DebugSerial.printf("ota manifest http code=%d\n", code);
    http.end();
    return;
  }

  const String manifest = http.getString();
  http.end();

  const String version = jsonValue(manifest, "version");
  const String firmwareUrl = jsonValue(manifest, "firmware_url");

  if (version.length() == 0 || firmwareUrl.length() == 0) {
    DebugSerial.println("ota manifest invalid");
    return;
  }

  DebugSerial.printf("device=%s latest=%s\n", DEVICE_VERSION, version.c_str());
  if (version == DEVICE_VERSION) {
    return;
  }

  downloadAndApplyFirmware(firmwareUrl);
}

static void startUsbLiveLog(uint32_t now)
{
  usbLogActive = true;
  updateLoggingLed();
  usbLogStartMs = now;
  usbLogLastSampleMs = now;
  usbLogSamples = 0;
  usbLogBeats = 0;
  rejectedBeatCandidates = 0;
  beatEnvelope = 0.0f;
  beatFloor = 0.0f;
  beatThreshold = 0.18f;
  beatPeak = 0.0f;
  acquisitionBpm = 0.0f;
  bpm = 0.0f;
  lastBeatMs = 0;
  beatArmed = true;
  beatCandidateActive = false;
  micSensor.resetDetectorState();
  ecgBaseline = 0.0f;
  ecgDisplayFiltered = 0.0f;
  ecgDisplayNoise = 900.0f;
  ecgScale = 3500.0f;
  ecgDisplayReady = false;

  DebugSerial.printf("LIVE_TEST_START,version=%s,duration_ms=%lu,sample_ms=%lu\n",
                DEVICE_VERSION,
                (unsigned long)USB_LOG_DURATION_MS,
                (unsigned long)USB_LOG_PERIOD_MS);
  DebugSerial.println(logHeader());
}

static void stopUsbLiveLog(uint32_t now, const char *reason)
{
  DebugSerial.printf("LIVE_TEST_END,reason=%s,elapsed_ms=%lu,samples=%lu,beats=%lu,rejected=%lu,bpm=%.1f\n",
                reason,
                (unsigned long)(now - usbLogStartMs),
                (unsigned long)usbLogSamples,
                (unsigned long)usbLogBeats,
                (unsigned long)rejectedBeatCandidates,
                bpm);
  usbLogActive = false;
  updateLoggingLed();
}

static void handleUsbLogCommands(uint32_t now)
{
  while (DebugSerial.available() > 0) {
    const char command = (char)DebugSerial.read();
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

  char row[512] = {};
  formatLogRow(row, sizeof(row), elapsedMs);
  DebugSerial.print(row);
}

static void updateEcgDiagDebug(uint32_t now)
{
  if (usbLogActive || wifiLogActive || now - lastEcgDiagDebugMs < ECG_DIAG_DEBUG_PERIOD_MS) {
    return;
  }
  lastEcgDiagDebugMs = now;
  DebugSerial.printf("ECG_DIAG,ready=%u,seq=%lu,status=%06lX,lop=%02X,lon=%02X,sat=%02X,flags=%04X,common=%.1f,diff=%.1f,ch1=%ld,ch2=%ld,ch3=%ld,ch4=%ld\n",
                (unsigned)ecgSensor.ready(),
                (unsigned long)latestEcgSequence,
                (unsigned long)latestEcgStatus,
                (unsigned)latestEcgLeadOffPositive,
                (unsigned)latestEcgLeadOffNegative,
                (unsigned)latestEcgSaturationMask,
                (unsigned)latestEcgDiagnosticFlags,
                latestEcgCommonModeStep,
                latestEcgDifferentialStep,
                (long)latestEcgCh1,
                (long)latestEcgCh2,
                (long)latestEcgCh3,
                (long)latestEcgCh4);
}

static void startWifiLog(uint32_t now)
{
  wifiLogActive = true;
  wifiLogStartMs = now;
  wifiLogLastSampleMs = now;
  wifiLogSamples = 0;
  wifiLogBeats = 0;
  wifiStreamHeaderSent = false;
  updateLoggingLed();
}

static void stopWifiLog()
{
  wifiLogActive = false;
  wifiStreamHeaderSent = false;
  if (wifiStreamClient) {
    wifiStreamClient.stop();
  }
  updateLoggingLed();
}

static void sendHttpResponse(WiFiClient &client, const char *contentType, const String &body)
{
  client.println("HTTP/1.1 200 OK");
  client.println("Connection: close");
  client.print("Content-Type: ");
  client.println(contentType);
  client.print("Content-Length: ");
  client.println(body.length());
  client.println("Access-Control-Allow-Origin: *");
  client.println();
  client.print(body);
}

static String wifiStatusJson()
{
  String json = "{";
  json += "\"version\":\"";
  json += DEVICE_VERSION;
  json += "\",\"wifi_connected\":";
  json += (WiFi.status() == WL_CONNECTED) ? "true" : "false";
  json += ",\"ip\":\"";
  json += (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : "";
  json += "\",\"wifi_logging\":";
  json += wifiLogActive ? "true" : "false";
  json += ",\"usb_logging\":";
  json += usbLogActive ? "true" : "false";
  json += ",\"wifi_samples\":";
  json += String(wifiLogSamples);
  json += ",\"wifi_beats\":";
  json += String(wifiLogBeats);
  json += ",\"wifi_elapsed_ms\":";
  json += String(wifiLogActive ? (millis() - wifiLogStartMs) : 0);
  json += ",\"wifi_rate_hz\":";
  if (wifiLogActive && millis() != wifiLogStartMs) {
    json += String((float)wifiLogSamples * 1000.0f / (float)(millis() - wifiLogStartMs), 1);
  } else {
    json += "0.0";
  }
  json += ",\"ecg_seq\":";
  json += String(latestEcgSequence);
  json += ",\"ecg_ready\":";
  json += ecgSensor.ready() ? "true" : "false";
  json += ",\"ecg_lead_off_p\":\"";
  json += String(latestEcgLeadOffPositive, HEX);
  json += "\",\"ecg_lead_off_n\":\"";
  json += String(latestEcgLeadOffNegative, HEX);
  json += "\",\"ecg_sat_mask\":\"";
  json += String(latestEcgSaturationMask, HEX);
  json += "\",\"ecg_diag_flags\":\"";
  json += String(latestEcgDiagnosticFlags, HEX);
  json += "\",\"ecg_common_step\":";
  json += String(latestEcgCommonModeStep, 1);
  json += ",\"ecg_diff_step\":";
  json += String(latestEcgDifferentialStep, 1);
  json += ",\"mic_level\":";
  json += String(smoothedLevel, 4);
  json += ",\"acc_x\":";
  json += String(latestAccelX, 4);
  json += ",\"acc_y\":";
  json += String(latestAccelY, 4);
  json += ",\"acc_z\":";
  json += String(latestAccelZ, 4);
  json += ",\"acc_diag_flags\":\"";
  json += String(latestAccelDiagnosticFlags, HEX);
  json += "\"";
  json += "}";
  return json;
}

static String controlPageHtml()
{
  String html;
  html.reserve(1600);
  html += "<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>MotemaSens Dev Logger</title></head><body>";
  html += "<h2>MotemaSens Dev Logger</h2>";
  html += "<p>Device IP: ";
  html += WiFi.localIP().toString();
  html += "</p>";
  html += "<p><button onclick=\"fetch('/api/start').then(refresh)\">Start WiFi Log</button> ";
  html += "<button onclick=\"fetch('/api/stop',{cache:'no-store'}).then(refresh).catch(refresh)\">Stop WiFi Log</button> ";
  html += "<a href='/stream' target='_blank' rel='noopener'>Open CSV Stream</a></p>";
  html += "<pre id='status'>loading...</pre>";
  html += "<script>async function refresh(){let r=await fetch('/api/status');";
  html += "document.getElementById('status').textContent=JSON.stringify(await r.json(),null,2);}";
  html += "refresh();setInterval(refresh,1000);</script>";
  html += "</body></html>";
  return html;
}

static String readHttpRequestLine(WiFiClient &client, uint32_t timeoutMs = 250)
{
  String requestLine;
  const uint32_t start = millis();
  while (client.connected() && millis() - start < timeoutMs) {
    while (client.available()) {
      const char c = (char)client.read();
      if (c == '\r') {
        continue;
      }
      if (c == '\n') {
        return requestLine;
      }
      if (requestLine.length() < 160) {
        requestLine += c;
      }
    }
    delay(1);
  }
  return requestLine;
}

static void discardHttpHeaders(WiFiClient &client, uint32_t timeoutMs = 250)
{
  uint8_t newlines = 0;
  const uint32_t start = millis();
  while (client.connected() && millis() - start < timeoutMs) {
    while (client.available()) {
      const char c = (char)client.read();
      if (c == '\n') {
        ++newlines;
        if (newlines >= 2) {
          return;
        }
      } else if (c != '\r') {
        newlines = 0;
      }
    }
    delay(1);
  }
}

static void handleWifiHttpClient(uint32_t now)
{
  if (WiFi.status() != WL_CONNECTED || !wifiLogServerStarted) {
    return;
  }

  WiFiClient client = wifiLogServer.available();
  if (!client) {
    return;
  }

  const bool streamActive = wifiLogActive && wifiStreamClient && wifiStreamClient.connected();
  const uint32_t requestTimeoutMs = streamActive ? 25 : 250;
  const String requestLine = readHttpRequestLine(client, requestTimeoutMs);
  discardHttpHeaders(client, requestTimeoutMs);

  if (streamActive && requestLine.indexOf("GET /api/stop") != 0 && requestLine.indexOf("GET /control?cmd=stop") != 0 &&
      requestLine.indexOf("GET /api/status") != 0 && requestLine.indexOf("GET /status") != 0) {
    sendHttpResponse(client, "application/json", "{\"busy\":true,\"message\":\"stream active, stop logging first\"}");
    client.stop();
    return;
  }

  if (requestLine.indexOf("GET /stream") == 0) {
    if (wifiStreamClient) {
      wifiStreamClient.stop();
    }
    wifiStreamClient = client;
    wifiStreamClient.println("HTTP/1.1 200 OK");
    wifiStreamClient.println("Content-Type: text/csv");
    wifiStreamClient.println("Cache-Control: no-cache");
    wifiStreamClient.println("Connection: close");
    wifiStreamClient.println("Access-Control-Allow-Origin: *");
    wifiStreamClient.println();
    wifiStreamClient.println(logHeader());
    startWifiLog(now);
    wifiStreamHeaderSent = true;
    DebugSerial.println("wifi stream client connected");
    return;
  }

  if (requestLine.indexOf("GET /api/start") == 0 || requestLine.indexOf("GET /control?cmd=start") == 0) {
    startWifiLog(now);
    sendHttpResponse(client, "application/json", wifiStatusJson());
  } else if (requestLine.indexOf("GET /api/stop") == 0 || requestLine.indexOf("GET /control?cmd=stop") == 0) {
    stopWifiLog();
    sendHttpResponse(client, "application/json", wifiStatusJson());
  } else if (requestLine.indexOf("GET /api/status") == 0 || requestLine.indexOf("GET /status") == 0) {
    sendHttpResponse(client, "application/json", wifiStatusJson());
  } else {
    sendHttpResponse(client, "text/html", controlPageHtml());
  }
  client.stop();
}

static void updateWifiLiveLog(uint32_t now)
{
  if (!wifiLogActive) {
    return;
  }

  if (wifiStreamClient && !wifiStreamClient.connected()) {
    stopWifiLog();
    return;
  }

  if (now - wifiLogLastSampleMs < WIFI_LOG_PERIOD_MS) {
    return;
  }
  wifiLogLastSampleMs += WIFI_LOG_PERIOD_MS;
  if (now - wifiLogLastSampleMs > WIFI_LOG_PERIOD_MS) {
    wifiLogLastSampleMs = now;
  }
  ++wifiLogSamples;

  if (!wifiStreamClient || !wifiStreamClient.connected()) {
    return;
  }

  if (!wifiStreamHeaderSent) {
    wifiStreamClient.println(logHeader());
    wifiStreamHeaderSent = true;
  }

  char row[512] = {};
  const uint32_t elapsedMs = now - wifiLogStartMs;
  formatLogRow(row, sizeof(row), elapsedMs);
  wifiStreamClient.print(row);
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

  flushDisplay();
}

static void pushAccelSample(const AccelSample &sample)
{
  memmove(&accelXHistory[0], &accelXHistory[1], sizeof(accelXHistory) - sizeof(accelXHistory[0]));
  memmove(&accelYHistory[0], &accelYHistory[1], sizeof(accelYHistory) - sizeof(accelYHistory[0]));
  memmove(&accelZHistory[0], &accelZHistory[1], sizeof(accelZHistory) - sizeof(accelZHistory[0]));

  if (sample.valid) {
    ++accelSamplesSinceDebug;
    latestAccelTimestampMs = sample.timestampMs;
    latestAccelAcqSeq8 = sample.acqSeq8;
    latestAccelRawX = sample.rawX;
    latestAccelRawY = sample.rawY;
    latestAccelRawZ = sample.rawZ;
    latestAccelDiagnosticFlags = sample.diagnosticFlags;

    if (haveLastAccel) {
      float delta = 0.0f;
      uint8_t healthyAxisCount = 0;
      if (sample.xHealthy) {
        const float axisDelta = fabsf(sample.x - lastAccelX);
        delta += axisDelta;
        ++healthyAxisCount;
      }
      if (sample.yHealthy) {
        const float axisDelta = fabsf(sample.y - lastAccelY);
        delta += axisDelta;
        ++healthyAxisCount;
      }
      if (sample.zHealthy) {
        const float axisDelta = fabsf(sample.z - lastAccelZ);
        delta += axisDelta;
        ++healthyAxisCount;
      }
      if (healthyAxisCount > 0) {
        motionLevel = (motionLevel * 0.90f) + ((delta / healthyAxisCount) * 0.10f);
      }
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
    ++accelReadFailuresSinceDebug;
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
  flushDisplay();
}

static void drawCombinedGraph()
{
  drawBackground();
  drawTopStatus();
  drawBandGrid(ECG_GRAPH_CENTER_Y, ECG_GRAPH_HALF_H);
  drawBandGrid(MIC_GRAPH_CENTER_Y, MIC_GRAPH_HALF_H);
  drawBandGrid(ACCEL_GRAPH_CENTER_Y, ACCEL_GRAPH_HALF_H);

  gfx->setTextSize(1);
  gfx->setTextColor(COLOR_DIM, COLOR_BG);
  gfx->setCursor(48, ECG_GRAPH_CENTER_Y - ECG_GRAPH_HALF_H - 10);
  gfx->print("ECG");
  gfx->setCursor(48, MIC_GRAPH_CENTER_Y - MIC_GRAPH_HALF_H - 10);
  gfx->print("MIC");
  gfx->setCursor(48, ACCEL_GRAPH_CENTER_Y - ACCEL_GRAPH_HALF_H - 10);
  gfx->print("ACC");
  if (latestAccelDiagnosticFlags != 0) {
    gfx->setTextColor(COLOR_X, COLOR_BG);
    gfx->print(" F");
    gfx->print(latestAccelDiagnosticFlags, HEX);
    gfx->setTextColor(COLOR_DIM, COLOR_BG);
  }

  drawTraceInBand(ecgWave, 1.0f, ECG_GRAPH_CENTER_Y, ECG_GRAPH_HALF_H, COLOR_WAVE);
  drawTraceInBand(wave, 1.0f, MIC_GRAPH_CENTER_Y, MIC_GRAPH_HALF_H, COLOR_Y);
  drawTraceInBand(accelXHistory, 2.0f, ACCEL_GRAPH_CENTER_Y, ACCEL_GRAPH_HALF_H, COLOR_X);
  drawTraceInBand(accelYHistory, 2.0f, ACCEL_GRAPH_CENTER_Y, ACCEL_GRAPH_HALF_H, COLOR_Y);
  drawTraceInBand(accelZHistory, 2.0f, ACCEL_GRAPH_CENTER_Y, ACCEL_GRAPH_HALF_H, COLOR_Z);
  drawSystemStatusPanel();

  flushDisplay();
}

static void pushWavePoint(float sample)
{
  memmove(&wave[0], &wave[1], sizeof(wave) - sizeof(wave[0]));
  const float softened = (wave[SCREEN_W - 2] * 0.18f) + (sample * 0.82f);
  wave[SCREEN_W - 1] = constrain(softened, -1.0f, 1.0f);
  scrollHeartLabels();
}

static void pushMicDisplayPoint(float sample)
{
  pendingMicDisplaySum += sample;
  ++pendingMicDisplayCount;

  if (pendingMicDisplayCount < MIC_DISPLAY_SCROLL_DIVIDER) {
    return;
  }

  pushWavePoint(pendingMicDisplaySum / pendingMicDisplayCount);
  pendingMicDisplaySum = 0.0f;
  pendingMicDisplayCount = 0;
}

static void pushEcgSample(const EcgSample &sample)
{
  if (!sample.valid) {
    return;
  }

  latestEcgSequence = sample.sequence;
  latestEcgTimestampMs = sample.timestampMs;
  latestEcgAcqSeq8 = sample.acqSeq8;
  latestEcgStatus = sample.status;
  latestEcgCh1 = sample.channels[0];
  latestEcgCh2 = sample.channels[1];
  latestEcgCh3 = sample.channels[2];
  latestEcgCh4 = sample.channels[3];
  latestEcgLeadOffPositive = sample.leadOffPositive;
  latestEcgLeadOffNegative = sample.leadOffNegative;
  latestEcgSaturationMask = sample.saturationMask;
  latestEcgDiagnosticFlags = sample.diagnosticFlags;
  latestEcgCommonModeStep = sample.commonModeStep;
  latestEcgDifferentialStep = sample.differentialStep;

#if ENABLE_ECG_INTERNAL_TEST_SIGNAL
  const float raw = (float)sample.channels[0];
#else
  const float raw = (float)(sample.channels[0] - sample.channels[1]);
#endif
  if (!ecgDisplayReady) {
    ecgBaseline = raw;
    ecgDisplayFiltered = 0.0f;
    ecgScale = 3500.0f;
    for (float &point : ecgWave) {
      point = 0.0f;
    }
    ecgDisplayReady = true;
  }

  ecgBaseline += 0.010f * (raw - ecgBaseline);
  const float centered = raw - ecgBaseline;
  const float displayJump = centered - ecgDisplayFiltered;
  ecgDisplayNoise = (ecgDisplayNoise * 0.992f) + (fabsf(displayJump) * 0.008f);
  const float maxStep = max(450.0f, ecgDisplayNoise * 1.15f);
  const float limitedCentered = ecgDisplayFiltered + constrain(displayJump, -maxStep, maxStep);
  ecgDisplayFiltered += 0.16f * (limitedCentered - ecgDisplayFiltered);

  ecgScale = max(1200.0f, ecgScale * 0.997f);
  ecgScale = max(ecgScale, fabsf(ecgDisplayFiltered) * 2.4f);

  memmove(&ecgWave[0], &ecgWave[1], sizeof(ecgWave) - sizeof(ecgWave[0]));
  ecgWave[SCREEN_W - 1] = constrain(ecgDisplayFiltered / ecgScale, -0.95f, 0.95f);
}

static void sendEcgSample(const EcgSample &sample)
{
  if (!ecgSampleQueue) {
    return;
  }

  if (xQueueSend(ecgSampleQueue, &sample, 0) != pdTRUE) {
    EcgSample dropped = {};
    xQueueReceive(ecgSampleQueue, &dropped, 0);
    xQueueSend(ecgSampleQueue, &sample, 0);
  }
}

static void readEcgSample()
{
  EcgSample sample = {};
  if (ecgSensor.poll(sample)) {
    sample.acqSeq8 = nextAcquisitionSeq8();
    sendEcgSample(sample);
  }
}

static void updateBeatDetector(float energy)
{
  const uint32_t now = millis();
  const bool motionQuiet = motionLevel < MOTION_GATE_THRESHOLD_G;
  const float gatedEnergy = (energy < HEART_NOISE_GATE) ? 0.0f : energy;

  beatEnvelope = (beatEnvelope * 0.86f) + (gatedEnergy * 0.14f);
  beatPeak = max(beatEnvelope, beatPeak * 0.985f);
  beatFloor = (beatFloor * 0.997f) + (beatEnvelope * 0.003f);

  const float dynamicMargin = max(0.08f, (beatPeak - beatFloor) * 0.42f);
  const float targetThreshold = constrain(beatFloor + dynamicMargin, 0.18f, 0.58f);
  beatThreshold = (beatThreshold * 0.90f) + (targetThreshold * 0.10f);

  const float resetLevel = max(0.16f, beatThreshold * 0.62f);
  if (beatEnvelope < resetLevel) {
    beatArmed = true;
  }

  if (beatCandidateActive) {
    if (beatEnvelope > beatCandidatePeakLevel) {
      beatCandidatePeakLevel = beatEnvelope;
      beatCandidatePeakMs = now;
      beatCandidatePeakGain = micSensor.autoGain();
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
                             beatEnvelope > beatFloor + 0.07f;
  if (!beatCandidate || now - lastBeatMs < HEART_REFRACTORY_MS) {
    return;
  }

  beatCandidateActive = true;
  beatCandidateStartMs = now;
  beatCandidatePeakMs = now;
  beatCandidatePeakLevel = beatEnvelope;
  beatCandidatePeakGain = micSensor.autoGain();
}

static void readAccelSample()
{
  AccelSample sample = {};
  const uint32_t now = millis();
  if (!accelSensor.poll(now, sample)) {
    return;
  }
  sample.timestampMs = now;
  sample.acqSeq8 = nextAcquisitionSeq8();
  sendAccelSample(sample);
}

static void readMicSamples()
{
  MicFrame frame = {};
  micSensor.readFrame(frame);
  frame.timestampMs = millis();
  frame.acqSeq8 = nextAcquisitionSeq8();
  sendMicFrame(frame);
}

static void updateDeviceHeartbeatLed(uint32_t now)
{
  if (now - lastHeartbeatLedMs < HEARTBEAT_LED_UPDATE_MS) {
    return;
  }

  lastHeartbeatLedMs = now;
  const uint32_t phase = now % HEARTBEAT_LED_PERIOD_MS;
  const uint32_t halfPeriod = HEARTBEAT_LED_PERIOD_MS / 2;
  const uint32_t ramp = (phase < halfPeriod) ? phase : (HEARTBEAT_LED_PERIOD_MS - phase);
  const uint32_t dutyRange = HEARTBEAT_LED_MAX_DUTY - HEARTBEAT_LED_MIN_DUTY;
  const uint8_t greenDuty = HEARTBEAT_LED_MIN_DUTY + ((ramp * dutyRange) / halfPeriod);
  const uint8_t blueDuty = HEARTBEAT_LED_MIN_DUTY + (((halfPeriod - ramp) * dutyRange) / halfPeriod);
  ledcWrite(HEARTBEAT_GREEN_PWM_CHANNEL, greenDuty);
  ledcWrite(HEARTBEAT_BLUE_PWM_CHANNEL, blueDuty);
}

static void drainAcquisitionQueues()
{
  MicFrame micFrame = {};
  while (xQueueReceive(micFrameQueue, &micFrame, 0) == pdTRUE) {
    latestMicTimestampMs = micFrame.timestampMs;
    latestMicAcqSeq8 = micFrame.acqSeq8;
    if (micFrame.valid) {
      smoothedLevel = micFrame.normalizedLevel;
      updateBeatDetector(smoothedLevel);
    }

    for (size_t i = 0; i < micFrame.displayPointCount; ++i) {
      latestMicPoint = micFrame.displayPoints[i];
      pushMicDisplayPoint(micFrame.displayPoints[i]);
    }
  }

  BeatEvent event = {};
  while (xQueueReceive(beatEventQueue, &event, 0) == pdTRUE) {
    addHeartLabel();
    bpm = event.bpm;
    if (usbLogActive) {
      ++usbLogBeats;
      const uint32_t eventElapsedMs = (event.beatMs >= usbLogStartMs) ? (event.beatMs - usbLogStartMs) : 0;
      const uint32_t outputDelayMs = millis() - event.beatMs;
      DebugSerial.printf("BEAT,%lu,interval_ms=%lu,bpm=%.1f,level=%.4f,gain=%.1f,delay_ms=%lu\n",
                    (unsigned long)eventElapsedMs,
                    (unsigned long)event.intervalMs,
                    event.bpm,
                    event.level,
                    event.gain,
                    (unsigned long)outputDelayMs);
    }
    if (wifiLogActive) {
      ++wifiLogBeats;
    }
  }

  AccelSample sample = {};
  while (xQueueReceive(accelSampleQueue, &sample, 0) == pdTRUE) {
    pushAccelSample(sample);
  }

  const uint32_t now = millis();
  if (USB_LOG_ACCEL_DEBUG && usbLogActive && now - lastAccelDebugMs >= ACCEL_DEBUG_PERIOD_MS) {
    DebugSerial.printf("ACCEL_DEBUG,hz=%lu,fail=%lu,raw=%d,%d,%d,g=%.2f,%.2f,%.2f\n",
                  (unsigned long)accelSamplesSinceDebug,
                  (unsigned long)accelReadFailuresSinceDebug,
                  latestAccelRawX, latestAccelRawY, latestAccelRawZ,
                  latestAccelX, latestAccelY, latestAccelZ);
    accelSamplesSinceDebug = 0;
    accelReadFailuresSinceDebug = 0;
    lastAccelDebugMs = now;
  }

  EcgSample ecgSample = {};
  while (xQueueReceive(ecgSampleQueue, &ecgSample, 0) == pdTRUE) {
    pushEcgSample(ecgSample);
  }
}

static void acquisitionTask(void *)
{
  for (;;) {
    ++core0LoopCounter;
    readEcgSample();
    readAccelSample();
    readMicSamples();
    readEcgSample();
    readAccelSample();
  }
}

static void outputTask(void *)
{
  for (;;) {
    const uint32_t loopStartUs = micros();
    const uint32_t now = millis();
    ++core1LoopCounter;
    updateDeviceHeartbeatLed(now);
    handleUsbLogCommands(now);
    startWifiServerIfConnected();
    handleWifiHttpClient(now);
    updateEcgDiagDebug(now);
    updateSystemStatus(now);

    if (!otaCheckedOnce || now - lastOtaCheckMs >= 60000) {
      otaCheckedOnce = true;
      lastOtaCheckMs = now;
      checkForOtaUpdate();
    }

    drainAcquisitionQueues();
    updateUsbLiveLog(now);
    updateWifiLiveLog(now);

    const uint32_t displayPeriodMs = (usbLogActive || wifiLogActive) ? 120 : 24;
    if (now - lastDrawMs >= displayPeriodMs) {
      lastDrawMs = now;
      drawCombinedGraph();
    }

    core1BusyMicros += micros() - loopStartUs;
    vTaskDelay(pdMS_TO_TICKS((usbLogActive || wifiLogActive) ? 1 : 2));
  }
}

void setup()
{
  DebugSerial.begin(115200);
  delay(200);
  DebugSerial.printf("device version: %s\n", DEVICE_VERSION);

  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);
  ledcSetup(HEARTBEAT_GREEN_PWM_CHANNEL, HEARTBEAT_LED_PWM_FREQ_HZ, HEARTBEAT_LED_PWM_BITS);
  ledcSetup(HEARTBEAT_BLUE_PWM_CHANNEL, HEARTBEAT_LED_PWM_FREQ_HZ, HEARTBEAT_LED_PWM_BITS);
  ledcAttachPin(LED_GREEN, HEARTBEAT_GREEN_PWM_CHANNEL);
  ledcAttachPin(LED_BLUE, HEARTBEAT_BLUE_PWM_CHANNEL);
  ledcWrite(HEARTBEAT_GREEN_PWM_CHANNEL, 0);
  ledcWrite(HEARTBEAT_BLUE_PWM_CHANNEL, 0);

  if (!gfx->begin(80000000)) {
    DebugSerial.println("LCD begin failed");
  }

  runStartupScreen();
  connectWifi();
  drawBackground();
  gfx->flush();
  accelSensor.begin();
  ecgSensor.begin();
  micSensor.begin();

  for (float &point : wave) {
    point = 0.0f;
  }
  for (float &point : ecgWave) {
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

  micFrameQueue = xQueueCreate(32, sizeof(MicFrame));
  beatEventQueue = xQueueCreate(12, sizeof(BeatEvent));
  accelSampleQueue = xQueueCreate(64, sizeof(AccelSample));
  ecgSampleQueue = xQueueCreate(128, sizeof(EcgSample));
  if (!micFrameQueue || !beatEventQueue || !accelSampleQueue || !ecgSampleQueue) {
    DebugSerial.println("queue create failed");
    ESP.restart();
  }

  xTaskCreatePinnedToCore(acquisitionTask, "acquisition", ACQUISITION_TASK_STACK, nullptr,
                          ACQUISITION_TASK_PRIORITY, nullptr, ACQUISITION_CORE);
  xTaskCreatePinnedToCore(outputTask, "output", OUTPUT_TASK_STACK, nullptr,
                          OUTPUT_TASK_PRIORITY, nullptr, OUTPUT_CORE);

  DebugSerial.printf("mic accel ecg test running, acquisition core=%u output core=%u\n",
                ACQUISITION_CORE, OUTPUT_CORE);
  DebugSerial.println("usb live test ready: send S for 60 seconds, X to stop");
}

void loop()
{
  vTaskDelay(pdMS_TO_TICKS(1000));
}
