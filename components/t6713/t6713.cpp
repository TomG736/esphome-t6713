#include "t6713.h"
#include "esphome/core/log.h"

#define LSB(x) (x & 0xFF)
#define MSB(x) ((x >> 8) & 0xFF)

namespace esphome {
namespace t6713 {

static const char *const TAG = "t6713";

static const uint32_t T6713_TIMEOUT = 1000;
// static const uint8_t T6713_MAGIC = 0xFF;
// static const uint8_t T6713_ADDR_HOST = 0xFA;
#define T6713_ADDR_DEFAULT 0x15
static uint8_t T6713_ADDR_SENSOR = T6713_ADDR_DEFAULT;

static const uint8_t T6713_READ_FUNCTION = 0x04;

static const uint16_t T6713_FIRMWARE_REGISTER = 0x1389;
static const uint16_t T6713_STATUS_REGISTER = 0x138A;
static const uint16_t T6713_PPM_REGISTER = 0x138B;

static const uint8_t T6713_COMMAND_GET_PPM[] = {0x00, 0x01};
static const uint8_t T6713_COMMAND_GET_SERIAL[] = {0x02, 0x01};
static const uint8_t T6713_COMMAND_GET_VERSION[] = {0x02, 0x0D};
static const uint8_t T6713_COMMAND_GET_ELEVATION[] = {0x02, 0x0F};
static const uint8_t T6713_COMMAND_GET_ABC[] = {0xB7, 0x00};
static const uint8_t T6713_COMMAND_ENABLE_ABC[] = {0xB7, 0x01};
static const uint8_t T6713_COMMAND_DISABLE_ABC[] = {0xB7, 0x02};
static const uint8_t T6713_COMMAND_SET_ELEVATION[] = {0x03, 0x0F};

uint8_t failCount = 0;

// Compute the MODBUS RTU CRC
uint16_t ModRTU_CRC(uint8_t* buf, int len)
{
  uint16_t crc = 0xFFFF;
  
  for (int pos = 0; pos < len; pos++) {
    crc ^= (uint16_t)buf[pos];          // XOR byte into least sig. byte of crc
  
    for (int i = 8; i != 0; i--) {    // Loop over each bit
      if ((crc & 0x0001) != 0) {      // If the LSB is set
        crc >>= 1;                    // Shift right and XOR 0xA001
        crc ^= 0xA001;
      }
      else                            // Else LSB is not set
        crc >>= 1;                    // Just shift right
    }
  }
  // Note, this number has low and high bytes swapped, so use it accordingly (or swap bytes)
  return crc;  
}

void T6713Component::send_read_command_(uint8_t function_code, uint16_t register_address, uint16_t readLen){
  uint16_t crc = 0xFFFF;
  uint8_t message[16];
  uint8_t messageLen = 0;

  message[0] = T6713_ADDR_SENSOR;
  message[1] = function_code;
  message[2] = MSB(register_address);
  message[3] = LSB(register_address);
  message[4] = MSB(readLen);
  message[5] = LSB(readLen);
  messageLen = 6;
  crc = ModRTU_CRC(message, messageLen);
  message[6] = LSB(crc);
  message[7] = MSB(crc);
  messageLen += 2;
  this->write_array(message, messageLen);
}

void T6713Component::send_ppm_command_() {
  this->command_time_ = millis();
  this->command_ = T6713Command::GET_PPM;
  send_read_command_(T6713_READ_FUNCTION, T6713_PPM_REGISTER, 1);
  ESP_LOGD(TAG, "Sent PPM Request");
}

void T6713Component::scan_modbus(uint8_t address) {
  this->command_time_ = millis();
  this->command_ = T6713Command::GET_VERSION;
  T6713_ADDR_SENSOR = address;
  send_read_command_(T6713_READ_FUNCTION, T6713_FIRMWARE_REGISTER, 1);
  ESP_LOGD(TAG, "Sent Firmware Request to %i", T6713_ADDR_SENSOR);
}

void T6713Component::loop() {
  if (this->available() < 5) {
    if (this->command_ == T6713Command::GET_PPM && millis() - this->command_time_ > T6713_TIMEOUT) {
      failCount++;
      ESP_LOGD(TAG, "T6713 Received %i", this->available());
      /* command got eaten, clear the buffer and fire another */
      while (this->available())
        this->read();
      if(failCount >= 5){
        failCount = 0;
        this->scan_modbus(failCount);
        return;
      }
      this->send_ppm_command_();
    }
    if (this->command_ == T6713Command::GET_VERSION && millis() - this->command_time_ > T6713_TIMEOUT) {
      failCount++;
      ESP_LOGD(TAG, "T6713 Received %i", this->available());
      /* command got eaten, clear the buffer and fire another */
      while (this->available())
        this->read();
      if(failCount >= 255){
        failCount = 0;
        this->command_time_ = 0;
        this->command_ = T6713Command::NONE;
        ESP_LOGE(TAG, "No T6713 Found");
        return;
      }
      this->scan_modbus(failCount);
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
    case T6713Command::GET_VERSION: {
      ESP_LOGD(TAG, "T6713 Detected at %i", failCount);
      failCount = 0;
      this->send_ppm_command_();
      break;
    }
    default:
      ESP_LOGW(TAG, "Unknown response: %i", this->command_);
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