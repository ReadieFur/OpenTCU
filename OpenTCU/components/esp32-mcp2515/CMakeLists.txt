# https://esp32.com/viewtopic.php?t=29660
idf_component_register(SRCS "mcp2515.cpp" INCLUDE_DIRS "." REQUIRES driver)
