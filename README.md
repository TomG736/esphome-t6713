# esphome-t6713
Easiest to just use the pwm pin:
```yaml
sensor:
  - platform: pulse_width
    pin: D7
    name: CO2
    unit_of_measurement: "ppm"
    icon: "mdi:molecule-co2"
    device_class: "carbon_dioxide"
    state_class: "measurement"
    accuracy_decimals: 0
    filters:
    - offset: 0.002
    - multiply: 2000
```