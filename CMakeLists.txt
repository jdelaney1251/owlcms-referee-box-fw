# Load zephyr build system and add extra modules if required
cmake_minimum_required(VERSION 3.13.1)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(owlcms_ref_btn)

# Add your source file to the "app" target.
target_sources(app PRIVATE 
                src/io.c 
                src/msys.c 
                src/main.c 
                src/io_mgr.c 
                src/settings_util.c
                src/comms_mgr.c
                src/wifi_conn.c
                src/mqtt_client.c
                src/ble_config_mgr.c
                src/config_gatt_service.c)