#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void ble_server_init(void);
void notify_speed(float kmh);          // łatwo dodać kolejne notify_xyz()

#ifdef __cplusplus
}
#endif
