#include "mic_sensor.h"

#include "sensor_config.h"
#include <driver/i2s.h>
#include <math.h>

static constexpr i2s_port_t I2S_PORT = I2S_NUM_0;
static constexpr i2s_channel_fmt_t I2S_CHANNEL = I2S_CHANNEL_FMT_ONLY_LEFT;
static constexpr uint32_t SAMPLE_RATE = 16000;
static constexpr size_t AUDIO_BLOCK_SAMPLES = 64;
static constexpr size_t DISPLAY_POINTS_PER_BLOCK = 2;

void MicSensor::begin()
{
#if ENABLE_MIC_SENSOR
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
#endif
}

void MicSensor::resetDetectorState()
{
  smoothedLevel_ = 0.0f;
  displaySample_ = 0.0f;
  autoGain_ = 10.0f;
}

bool MicSensor::readFrame(MicFrame &frame)
{
  frame = {};
#if !ENABLE_MIC_SENSOR
  return false;
#else
  int32_t samples[AUDIO_BLOCK_SAMPLES];
  size_t bytesRead = 0;
  const esp_err_t result = i2s_read(I2S_PORT, samples, sizeof(samples), &bytesRead, pdMS_TO_TICKS(8));
  if (result != ESP_OK || bytesRead == 0) {
    frame.displayPointCount = 1;
    frame.displayPoints[0] = 0.0f;
    return false;
  }

  const size_t count = bytesRead / sizeof(samples[0]);
  const size_t chunkSize = max<size_t>(1, count / DISPLAY_POINTS_PER_BLOCK);
  float sumSq = 0.0f;
  float absolutePeak = 0.0f;
  float chunkPeaks[DISPLAY_POINTS_PER_BLOCK] = {};
  float chunkSignedPeaks[DISPLAY_POINTS_PER_BLOCK] = {};

  for (size_t i = 0; i < count; ++i) {
    const float sample = (float)(samples[i] >> 14);
    dc_ += 0.00020f * (sample - dc_);
    const float centered = sample - dc_;

    lowFast_ += 0.052f * (centered - lowFast_);
    lowSlow_ += 0.006f * (centered - lowSlow_);
    const float heartBand = lowFast_ - lowSlow_;

    sumSq += heartBand * heartBand;
    const float magnitude = fabsf(heartBand);
    absolutePeak = max(absolutePeak, magnitude);

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

  const float target = normalized * autoGain_;
  if (target > 0.48f) {
    autoGain_ *= 0.965f;
  } else if (target < 0.13f) {
    autoGain_ *= 1.0015f;
  }
  autoGain_ = constrain(autoGain_, 3.0f, 36.0f);

  normalized = constrain(normalized * autoGain_, 0.0f, 1.0f);
  smoothedLevel_ = (smoothedLevel_ * 0.88f) + (normalized * 0.12f);

  frame.normalizedLevel = smoothedLevel_;
  frame.displayPointCount = DISPLAY_POINTS_PER_BLOCK;
  frame.valid = true;
  for (size_t i = 0; i < DISPLAY_POINTS_PER_BLOCK; ++i) {
    const float sign = (chunkSignedPeaks[i] >= 0.0f) ? 1.0f : -1.0f;
    float displayPoint = constrain((chunkPeaks[i] / 12000.0f) * autoGain_, 0.0f, 1.0f);
    displayPoint = constrain(powf(displayPoint, 0.68f) * 0.86f, 0.0f, 0.96f) * sign;
    displaySample_ += 0.70f * (displayPoint - displaySample_);
    frame.displayPoints[i] = displaySample_;
  }
  return true;
#endif
}
