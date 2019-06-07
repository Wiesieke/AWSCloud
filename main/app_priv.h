#pragma once
#include <stdbool.h>

void app_driver_init(void);
int app_driver_set_state(bool state);
bool app_driver_get_state(void);
int cloud_start(void);
