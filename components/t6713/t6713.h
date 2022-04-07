#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace t6713 {

enum class T6713Command : uint8_t {
  NONE = 0,
  GET_PPM,
  GET_SERIAL,
  GET_VERSION,
  GET_ELEVATION,
  GET_ABC,
  ENABLE_ABC,
  DISABLE_ABC,
  SET_ELEVATION,
};

class T6713Component : public PollingComponent, public uart::UARTDevice {
 public:
  float get_setup_priority() const override;

  void loop() override;
  void update() override;
  void dump_config() override;

  void set_co2_sensor(sensor::Sensor *co2_sensor) { this->co2_sensor_ = co2_sensor; }

 protected:
  void query_ppm_();
  void send_ppm_command_();

  T6713Command command_ = T6713Command::NONE;
  uint32_t command_time_ = 0;

  sensor::Sensor *co2_sensor_{nullptr};
};

}  // namespace t6713
}  // namespace esphome