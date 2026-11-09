# Display: text on the ESP-WROVER-KIT LCD. Display/TextRenderer/Font5x7 are generic, DisplayPort is the target port.
idf_component_register(SRCS "Src/Display.cpp"
                            "Src/DisplayPort.cpp"
                            "Src/Font5x7.cpp"
                            "Src/TextRenderer.cpp"
                       INCLUDE_DIRS "Inc"
                       PRIV_REQUIRES esp_driver_gpio esp_driver_spi freertos)
