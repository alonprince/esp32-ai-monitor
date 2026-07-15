#include "ble_server.h"
#include <string.h>
#include "esp_log.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "audio.h"

static const char *TAG = "ble_server";

static uint16_t s_conn_handle;
static uint16_t s_notify_handle;
static bool s_is_connected = false;
static uint8_t s_own_addr_type = BLE_OWN_ADDR_PUBLIC;

static ble_telemetry_data_t s_telemetry = {
    .state = BLE_STATE_DISCONNECTED,
    .agent_name = "None",
    .workspace = "None",
    .active_tool = "None",
    .message_preview = "None",
    .progress_current = 0,
    .progress_total = 0,
    .sync_time = 0,
    .brightness = 100,
    .volume = 100,
    .updated = false
};

static SemaphoreHandle_t s_telemetry_mutex = NULL;

// Service/Characteristic UUIDs (Little endian representation)
// Service: 0000cafe-0000-1000-8000-00805f9b34fb
static const ble_uuid128_t gatt_svr_svc_uuid =
    BLE_UUID128_INIT(0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
                     0x00, 0x10, 0x00, 0x00, 0xfe, 0xca, 0x00, 0x00);

// Write: 0000cafe-0001-1000-8000-00805f9b34fb
static const ble_uuid128_t gatt_svr_chr_write_uuid =
    BLE_UUID128_INIT(0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
                     0x00, 0x10, 0x00, 0x00, 0xfe, 0xca, 0x01, 0x00);

// Notify: 0000cafe-0002-1000-8000-00805f9b34fb
static const ble_uuid128_t gatt_svr_chr_notify_uuid =
    BLE_UUID128_INIT(0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
                     0x00, 0x10, 0x00, 0x00, 0xfe, 0xca, 0x02, 0x00);

static void ble_server_advertise(void);

static void ble_server_parse_tlv(const uint8_t *data, uint16_t len)
{
    if (xSemaphoreTake(s_telemetry_mutex, portMAX_DELAY) != pdTRUE) {
        return;
    }

    uint16_t offset = 0;
    while (offset + 2 <= len) {
        uint8_t type = data[offset];
        uint8_t tlv_len = data[offset + 1];
        if (offset + 2 + tlv_len > len) {
            ESP_LOGW(TAG, "Malformed TLV block: offset=%d, tlv_len=%d, total=%d", offset, tlv_len, len);
            break; // Exceeds packet size, corrupted
        }
        const uint8_t *val = &data[offset + 2];
        
        switch (type) {
            case 0x01: // Status Update
                if (tlv_len >= 1) {
                    s_telemetry.state = (ble_monitor_state_t)val[0];
                    s_telemetry.updated = true;
                    ESP_LOGI(TAG, "TLV State: %d", s_telemetry.state);
                }
                break;
            case 0x02: // Agent Name
                {
                    int copy_len = tlv_len < sizeof(s_telemetry.agent_name) - 1 ? tlv_len : sizeof(s_telemetry.agent_name) - 1;
                    memcpy(s_telemetry.agent_name, val, copy_len);
                    s_telemetry.agent_name[copy_len] = '\0';
                    s_telemetry.updated = true;
                    ESP_LOGI(TAG, "TLV Agent: %s", s_telemetry.agent_name);
                }
                break;
            case 0x03: // Workspace
                {
                    int copy_len = tlv_len < sizeof(s_telemetry.workspace) - 1 ? tlv_len : sizeof(s_telemetry.workspace) - 1;
                    memcpy(s_telemetry.workspace, val, copy_len);
                    s_telemetry.workspace[copy_len] = '\0';
                    s_telemetry.updated = true;
                    ESP_LOGI(TAG, "TLV Workspace: %s", s_telemetry.workspace);
                }
                break;
            case 0x04: // Active Tool
                {
                    int copy_len = tlv_len < sizeof(s_telemetry.active_tool) - 1 ? tlv_len : sizeof(s_telemetry.active_tool) - 1;
                    memcpy(s_telemetry.active_tool, val, copy_len);
                    s_telemetry.active_tool[copy_len] = '\0';
                    s_telemetry.updated = true;
                    ESP_LOGI(TAG, "TLV Tool: %s", s_telemetry.active_tool);
                }
                break;
            case 0x05: // Message Preview
                {
                    int copy_len = tlv_len < sizeof(s_telemetry.message_preview) - 1 ? tlv_len : sizeof(s_telemetry.message_preview) - 1;
                    memcpy(s_telemetry.message_preview, val, copy_len);
                    s_telemetry.message_preview[copy_len] = '\0';
                    s_telemetry.updated = true;
                    ESP_LOGI(TAG, "TLV Msg Preview: %s", s_telemetry.message_preview);
                }
                break;
            case 0x06: // Stats / Progress
                if (tlv_len >= 2) {
                    s_telemetry.progress_current = val[0];
                    s_telemetry.progress_total = val[1];
                    s_telemetry.updated = true;
                    ESP_LOGI(TAG, "TLV Progress: %d/%d", s_telemetry.progress_current, s_telemetry.progress_total);
                }
                break;
            case 0x07: // Sync Time
                if (tlv_len >= 4) {
                    s_telemetry.sync_time = ((uint32_t)val[0] << 24) | 
                                            ((uint32_t)val[1] << 16) | 
                                            ((uint32_t)val[2] << 8) | 
                                            val[3];
                    s_telemetry.updated = true;
                    ESP_LOGI(TAG, "TLV Time Sync: %lu", s_telemetry.sync_time);
                }
                break;
            case 0x08: // Sound & Light
                if (tlv_len >= 2) {
                    s_telemetry.brightness = val[0];
                    s_telemetry.volume = val[1];
                    s_telemetry.updated = true;
                    ESP_LOGI(TAG, "TLV Brightness: %d, Volume: %d", s_telemetry.brightness, s_telemetry.volume);
                }
                break;
            default:
                ESP_LOGW(TAG, "Unknown TLV type: 0x%02X", type);
                break;
        }
        offset += 2 + tlv_len;
    }

    xSemaphoreGive(s_telemetry_mutex);
}

static int ble_svr_cb_write(uint16_t conn_handle, uint16_t attr_handle,
                           struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
    uint8_t *buf = malloc(len);
    if (!buf) {
        return BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    int rc = ble_hs_mbuf_to_flat(ctxt->om, buf, len, NULL);
    if (rc != 0) {
        free(buf);
        return BLE_ATT_ERR_UNLIKELY;
    }

    ESP_LOGI(TAG, "Received %d bytes of TLV data", len);
    ble_server_parse_tlv(buf, len);
    free(buf);

    return 0;
}

static int ble_svr_cb_notify(uint16_t conn_handle, uint16_t attr_handle,
                            struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    return 0;
}

static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &gatt_svr_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = &gatt_svr_chr_write_uuid.u,
                .access_cb = ble_svr_cb_write,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            {
                .uuid = &gatt_svr_chr_notify_uuid.u,
                .access_cb = ble_svr_cb_notify,
                .val_handle = &s_notify_handle,
                .flags = BLE_GATT_CHR_F_NOTIFY,
            },
            {
                0, // No more characteristics in this service
            }
        },
    },
    {
        0, // No more services
    }
};

static int ble_server_gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                ESP_LOGI(TAG, "BLE Connected");
                s_is_connected = true;
                s_conn_handle = event->connect.conn_handle;
                
                xSemaphoreTake(s_telemetry_mutex, portMAX_DELAY);
                s_telemetry.state = BLE_STATE_IDLE;
                s_telemetry.updated = true;
                xSemaphoreGive(s_telemetry_mutex);

                audio_play_connect();
            } else {
                ESP_LOGE(TAG, "BLE Connection failed status=%d", event->connect.status);
                ble_server_advertise();
            }
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "BLE Disconnected, reason=%d", event->disconnect.reason);
            s_is_connected = false;
            
            xSemaphoreTake(s_telemetry_mutex, portMAX_DELAY);
            s_telemetry.state = BLE_STATE_DISCONNECTED;
            s_telemetry.updated = true;
            xSemaphoreGive(s_telemetry_mutex);

            audio_play_disconnect();

            ble_server_advertise();
            break;

        case BLE_GAP_EVENT_ADV_COMPLETE:
            ESP_LOGI(TAG, "BLE Advertising complete");
            ble_server_advertise();
            break;

        case BLE_GAP_EVENT_MTU:
            ESP_LOGI(TAG, "BLE MTU exchanged: conn_handle=%d mtu=%d",
                     event->mtu.conn_handle, event->mtu.value);
            break;

        default:
            break;
    }
    return 0;
}

static void ble_server_advertise(void)
{
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    int rc;

    memset(&fields, 0, sizeof fields);
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    const char *name = "Buddy-AMOLED";
    fields.name = (uint8_t *)name;
    fields.name_len = strlen(name);
    fields.name_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error setting advertising data; rc=%d", rc);
        return;
    }

    memset(&adv_params, 0, sizeof adv_params);
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    rc = ble_gap_adv_start(s_own_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, ble_server_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error starting advertising; rc=%d", rc);
    } else {
        ESP_LOGI(TAG, "BLE Advertising started as '%s'", name);
    }
}

static void ble_server_host_task(void *param)
{
    ESP_LOGI(TAG, "NimBLE Host Task Started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

static void ble_server_on_sync(void)
{
    int rc = ble_hs_id_infer_auto(0, &s_own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error determining address type; rc=%d", rc);
        return;
    }
    ble_server_advertise();
}

static void ble_server_on_reset(int reason)
{
    ESP_LOGE(TAG, "Resetting NimBLE host; reason=%d", reason);
}

void ble_server_init(void)
{
    s_telemetry_mutex = xSemaphoreCreateMutex();
    assert(s_telemetry_mutex != NULL);

    int rc = nimble_port_init();
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to init nimble port; rc=%d", rc);
        return;
    }

    // Initialize GAP & GATT service configs
    ble_svc_gap_init();
    ble_svc_gatt_init();

    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to count GATT svcs; rc=%d", rc);
        return;
    }

    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to add GATT svcs; rc=%d", rc);
        return;
    }

    // Set host callbacks
    ble_hs_cfg.sync_cb = ble_server_on_sync;
    ble_hs_cfg.reset_cb = ble_server_on_reset;

    // Set default GAP device name
    rc = ble_svc_gap_device_name_set("Buddy-AMOLED");
    assert(rc == 0);

    nimble_port_freertos_init(ble_server_host_task);
}

void ble_server_send_interaction(uint8_t event_code)
{
    if (!s_is_connected) {
        ESP_LOGW(TAG, "Cannot notify interaction: BLE not connected");
        return;
    }

    // TLV Notify format: [Event Code (0x81)] [Payload Length (1)] [Event Payload (1 byte event_code)]
    uint8_t notification[3] = { 0x81, 0x01, event_code };
    struct os_mbuf *om = ble_hs_mbuf_from_flat(notification, sizeof(notification));
    if (!om) {
        ESP_LOGE(TAG, "Failed to allocate mbuf for notification");
        return;
    }

    int rc = ble_gattc_notify_custom(s_conn_handle, s_notify_handle, om);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error sending BLE notify; rc=%d", rc);
    } else {
        ESP_LOGI(TAG, "Notification sent: event=%d", event_code);
    }
}

void ble_server_get_telemetry(ble_telemetry_data_t *out_data)
{
    if (xSemaphoreTake(s_telemetry_mutex, portMAX_DELAY) == pdTRUE) {
        memcpy(out_data, &s_telemetry, sizeof(ble_telemetry_data_t));
        xSemaphoreGive(s_telemetry_mutex);
    }
}

void ble_server_clear_update_flag(void)
{
    if (xSemaphoreTake(s_telemetry_mutex, portMAX_DELAY) == pdTRUE) {
        s_telemetry.updated = false;
        xSemaphoreGive(s_telemetry_mutex);
    }
}

bool ble_server_is_connected(void)
{
    return s_is_connected;
}
