#pragma once
#ifdef __cplusplus
extern "C" {
#endif

void ble_server_init(void);

void notify_speed(float kmh);
void notify_avg_speed(float kmh);
void notify_distance(float km);

#ifdef __cplusplus
}
#endif
