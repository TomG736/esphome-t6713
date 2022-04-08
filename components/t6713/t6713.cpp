#include "t6713.h"
#include "esphome/core/log.h"

namespace esphome {
namespace t6713 {

static const char *const TAG = "t6713";

static const uint32_t T6713_TIMEOUT = 1000;
// static const uint8_t T6713_MAGIC = 0xFF;
// static const uint8_t T6713_ADDR_HOST = 0xFA;
static const uint8_t T6713_ADDR_SENSOR = 0x15;
static const uint8_t T6713_COMMAND_GET_PPM[] = {0x00, 0x01};
static const uint8_t T6713_COMMAND_GET_SERIAL[] = {0x02, 0x01};
static const uint8_t T6713_COMMAND_GET_VERSION[] = {0x02, 0x0D};
static const uint8_t T6713_COMMAND_GET_ELEVATION[] = {0x02, 0x0F};
static const uint8_t T6713_COMMAND_GET_ABC[] = {0xB7, 0x00};
static const uint8_t T6713_COMMAND_ENABLE_ABC[] = {0xB7, 0x01};
static const uint8_t T6713_COMMAND_DISABLE_ABC[] = {0xB7, 0x02};
static const uint8_t T6713_COMMAND_SET_ELEVATION[] = {0x03, 0x0F};

void T6713Component::send_ppm_command_() {
  this->command_time_ = millis();
  this->command_ = T6713Command::GET_PPM;
  this->write_byte(T6713_ADDR_SENSOR);
  this->write_byte(0x04);
  this->write_byte(0x13);
  this->write_byte(0x8b);
  this->write_array(T6713_COMMAND_GET_PPM, sizeof(T6713_COMMAND_GET_PPM));
  this->write_byte(0x46);
  this->write_byte(0x70);
  ESP_LOGW(TAG, "Sent PPM Request");
}

void T6713Component::loop() {
  if (this->available() < 5) {
    if (this->command_ == T6713Command::GET_PPM && millis() - this->command_time_ > T6713_TIMEOUT) {
      /* command got eaten, clear the buffer and fire another */
      while (this->available())
        this->read();
      this->send_ppm_command_();
    }
    return;
  }

  uint8_t response_buffer[6];

  /* by the time we get here, we know we have at least five bytes in the buffer */
  this->read_array(response_buffer, 5);

  // Read header
  if (response_buffer[0] != 0x15) {
    ESP_LOGW(TAG, "Got bad data from T6713! Address was %02X and function was %02X", response_buffer[0],
             response_buffer[1]);
    /* make sure the buffer is empty */
    while (this->available())
      this->read();
    /* try again to read the sensor */
    this->send_ppm_command_();
    this->status_set_warning();
    return;
  }

  this->status_clear_warning();

  switch (this->command_) {
    case T6713Command::GET_PPM: {
      const uint16_t ppm = encode_uint16(response_buffer[3], response_buffer[4]);
      ESP_LOGD(TAG, "T6713 Received CO₂=%uppm", ppm);
      this->co2_sensor_->publish_state(ppm);
      break;
    }
    default:
      ESP_LOGW(TAG, "unknown response");
      break;
  }
  this->command_time_ = 0;
  this->command_ = T6713Command::NONE;
}

void T6713Component::update() { this->query_ppm_(); }

void T6713Component::query_ppm_() {
  if (this->co2_sensor_ == nullptr ||
      (this->command_ != T6713Command::NONE && millis() - this->command_time_ < T6713_TIMEOUT)) {
    return;
  }

  this->send_ppm_command_();
}

float T6713Component::get_setup_priority() const { return setup_priority::DATA; }
void T6713Component::dump_config() {
  ESP_LOGCONFIG(TAG, "T6713:");
  LOG_SENSOR("  ", "CO2", this->co2_sensor_);
  this->check_uart_settings(19200);
}

}  // namespace t6713
}  // namespace esphome