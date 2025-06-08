#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "ble_server.h"

/* ---------- UUID-y ---------- */
static const ble_uuid128_t SVC_UUID =
    BLE_UUID128_INIT(0xC0,0xDE,0xC0,0xDE,0x00,0x00,0x00,0x00,
                     0x00,0x00,0x00,0x00,0xC0,0xDE,0x12,0x34);

static const ble_uuid128_t CHAR_SPEED_UUID =
    BLE_UUID128_INIT(0xC0,0xDE,0xC0,0xDE,0x00,0x00,0x00,0x00,
                     0x00,0x00,0x00,0x00,0xC0,0xDE,0x56,0x78);

static const ble_uuid128_t CHAR_AVG_UUID =
    BLE_UUID128_INIT(0xC0,0xDE,0xC0,0xDE,0x00,0x00,0x00,0x00,
                     0x00,0x00,0x00,0x00,0xC0,0xDE,0x56,0x79);

static const ble_uuid128_t CHAR_DIST_UUID =
    BLE_UUID128_INIT(0xC0,0xDE,0xC0,0xDE,0x00,0x00,0x00,0x00,
                     0x00,0x00,0x00,0x00,0xC0,0xDE,0x56,0x7A);

/* ---------- zmienne globalne ---------- */
static const char *TAG = "BLE_SRV";
static uint8_t  own_addr_type;
static uint16_t conn_handle = BLE_HS_CONN_HANDLE_NONE;

static uint16_t h_speed, h_avg, h_dist;
//0x01, 0x02, 0x03


/* ---------- GATT ---------- */
static int chr_access_cb(uint16_t conn, uint16_t attr,
                         struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    float val = 0.0f;
    memcpy(&val, arg, sizeof(float));           /* zwróć ostatnią wartość */
    return os_mbuf_append(ctxt->om, &val, sizeof(val));
}

static struct ble_gatt_svc_def gatt_svcs[] = {
    { /* Primary Service */
      .type = BLE_GATT_SVC_TYPE_PRIMARY,
      .uuid = (ble_uuid_t *)&SVC_UUID,
      .characteristics = (struct ble_gatt_chr_def[]) {
          {   /* speed */
              .uuid = (ble_uuid_t *)&CHAR_SPEED_UUID,
              .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
              .access_cb = chr_access_cb,
              .val_handle = &h_speed,
              .arg = &h_speed,
          },
          {   /* average */
              .uuid = (ble_uuid_t *)&CHAR_AVG_UUID,
              .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
              .access_cb = chr_access_cb,
              .val_handle = &h_avg,
              .arg = &h_avg,
          },
          {   /* distance */
              .uuid = (ble_uuid_t *)&CHAR_DIST_UUID,
              .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
              .access_cb = chr_access_cb,
              .val_handle = &h_dist,
              .arg = &h_dist,
          },
          { 0 } /* terminator */
      }
    },
    { 0 }    /* koniec usług */
};

/* ---------- GAP ---------- */
static void advertise(void);
static int gap_event(struct ble_gap_event *e, void *arg)
{
    switch (e->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (e->connect.status == 0) {
            conn_handle = e->connect.conn_handle;
            ESP_LOGI(TAG, "Connected");
        } else {
            advertise();
        }
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "Disconnected");
        conn_handle = BLE_HS_CONN_HANDLE_NONE;
        advertise();
        break;
    default:
        break;
    }
    return 0;
}

static void advertise(void)
{
    struct ble_hs_adv_fields f = {0};
    f.name = (uint8_t *)"BikeMeter";
    f.name_len = strlen((char *)f.name);
    f.name_is_complete = 1;
    f.uuids128 = (ble_uuid128_t *)&SVC_UUID;
    f.num_uuids128 = 1;
    f.uuids128_is_complete = 1;
    ble_gap_adv_set_fields(&f);

    struct ble_gap_adv_params p = {
        .conn_mode = BLE_GAP_CONN_MODE_UND,
        .disc_mode = BLE_GAP_DISC_MODE_GEN,
    };
    ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &p,
                      gap_event, NULL);
}

/* ---------- helper NOTIFY ---------- */
static void notify_float(uint16_t h, float v)
{
    if (conn_handle == BLE_HS_CONN_HANDLE_NONE) return;
    struct os_mbuf *om = ble_hs_mbuf_from_flat(&v, sizeof(v));
    if (om) ble_gatts_notify_custom(conn_handle, h, om);
}

/* ---------- API ---------- */
void notify_speed(float v)     { notify_float(h_speed, v); }
void notify_avg_speed(float v) { notify_float(h_avg,   v); }
void notify_distance(float v)  { notify_float(h_dist,  v); }

/* ---------- inicjalizacja ---------- */
static void host_task(void *p)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

static void on_sync(void)
{
    ble_hs_id_infer_auto(0, &own_addr_type);
    advertise();
}

void ble_server_init(void)
{
    nvs_flash_init();                          /* NVS dla BT stack */
    esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);

    nimble_port_init();
    ble_svc_gap_init();
    ble_svc_gatt_init();

    ble_gatts_count_cfg(gatt_svcs);
    ble_gatts_add_svcs(gatt_svcs);

    ble_hs_cfg.sync_cb = on_sync;
    nimble_port_freertos_init(host_task);
}
