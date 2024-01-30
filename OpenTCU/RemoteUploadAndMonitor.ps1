pio remote run --target upload --environment esp32c3-debug
Start-Sleep -s 1
pio remote device monitor -p COM9 -b 115200
