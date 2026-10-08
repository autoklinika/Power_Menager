idf_component_register(
  SRCS "app_main.cpp" "ina228.cpp" "../core/pm_core.cpp" "../core/pm_modbus.cpp"
  INCLUDE_DIRS "." "../core/include"
  REQUIRES esp_driver_gpio esp_driver_uart esp_driver_i2c esp_timer esp_hw_support esp_system
)
