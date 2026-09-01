python -m esptool --chip esp32s3 -b 460800 --before default-reset --after hard-reset  --port COM11 write_flash --flash_mode dio --flash_size 16MB --flash_freq 80m 0x0 ./build/bootloader/bootloader.bin  0x8000 ./build/partition_table/partition-table.bin  0x10000  ./build/sample_project.bin
pause
