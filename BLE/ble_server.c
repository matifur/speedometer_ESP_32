#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "esp_bt.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "host/ble_gatt.h"   // lub ble_gattc.h w starych wersjach*

static const char *TAG = "BLE_SRV";
static uint16_t conn_handle = BLE_HS_CONN_HANDLE_NONE;

/* ---------- UUID-y ---------- */
static const ble_uuid128_t SERVICE_UUID =
    BLE_UUID128_INIT(0x34,0x12,0xde,0xc0,0x00,0x00,0x00,0x00,
                     0x00,0x00,0x00,0x00,0xde,0xc0,0xde,0xc0);

static const ble_uuid128_t CHAR_SPEED_UUID =
    BLE_UUID128_INIT(0x78,0x56,0xde,0xc0,0x00,0x00,0x00,0x00,
                     0x00,0x00,0x00,0x00,0xde,0xc0,0xde,0xc0);

static uint16_t speed_handle;         // handle charakterystyki
static uint8_t  ble_addr_type;

/* ---------- GATT tabela ---------- */
static int speed_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                            struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    // tylko READ zwróci ostatnią wartość
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        // przykładowo zwróć 0 km/h
        float zero = 0.0f;
        return os_mbuf_append(ctxt->om, &zero, sizeof(zero));
    }
    return 0;
}

static const struct ble_gatt_svc_def gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = (ble_uuid_t *)&SERVICE_UUID,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid      = (ble_uuid_t *)&CHAR_SPEED_UUID,
                .access_cb = speed_chr_access,
                .val_handle = &speed_handle,
                .flags     = BLE_GATT_CHR_F_READ   |
                             BLE_GATT_CHR_F_NOTIFY
            },
            { 0 }      /* terminator */
        }
    },
    { 0 }
};

/* ---------- GAP & eventy ---------- */
static void ble_app_advertise(void);
static int ble_gap_event(struct ble_gap_event *e, void *arg)
{
    switch (e->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (e->connect.status == 0) {
            conn_handle = e->connect.conn_handle;          // <── NEW
            ESP_LOGI(TAG, "Connected, handle=%d", conn_handle);
        } else {
            ble_app_advertise();   // nieudane połączenie ⇒ reklamuj dalej
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "Disconnected, reason=%d", e->disconnect.reason);
        conn_handle = BLE_HS_CONN_HANDLE_NONE;             // <── NEW
        ble_app_advertise();
        break;
    }
    return 0;
}

static void ble_app_advertise(void)
{
    struct ble_gap_adv_params adv = {
        .conn_mode = BLE_GAP_CONN_MODE_UND,
        .disc_mode = BLE_GAP_DISC_MODE_GEN,
    };
    struct ble_hs_adv_fields fields = {0};

    const char *name = "BikeMeter";
    fields.name = (uint8_t *)name;
    fields.name_len = strlen(name);
    fields.name_is_complete = 1;

    fields.uuids128 = (ble_uuid128_t *)&SERVICE_UUID;
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;

    ble_gap_adv_set_fields(&fields);
    ble_gap_adv_start(ble_addr_type, NULL, BLE_HS_FOREVER,
                      &adv, ble_gap_event, NULL);
}

/* ---------- NimBLE host task ---------- */
static void host_task(void *param)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

static void on_sync(void)
{
    ble_hs_id_infer_auto(0, &ble_addr_type);
    ble_app_advertise();
}

/* ---------- API ---------- */
void ble_server_init(void)
{
    // NVS → kontroler BT
    ESP_ERROR_CHECK(nvs_flash_init());
    esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);

    nimble_port_init();
    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_gatts_count_cfg(gatt_svcs);
    ble_gatts_add_svcs(gatt_svcs);

    ble_hs_cfg.sync_cb = on_sync;

    nimble_port_freertos_init(host_task);
}

/* wysyłanie notyfikacji */
void notify_speed(float kmh)
{
    if (conn_handle == BLE_HS_CONN_HANDLE_NONE) return;   // brak klienta

    uint8_t buf[4];
    memcpy(buf, &kmh, sizeof(kmh));

    struct os_mbuf *om = ble_hs_mbuf_from_flat(buf, sizeof(buf));
    if (!om) return;

    /*  ──> Używamy API **gatts**  (serwer)  */
    int rc = ble_gatts_notify_custom(conn_handle,          /* handle */
                                     speed_handle,         /* char val */
                                     om);                  /* payload */
    if (rc != 0) {
        ESP_LOGW(TAG, "Notify failed rc=%d", rc);
    } else {
        ESP_LOGD(TAG, "Notify: %.2f km/h", kmh);
    }
}