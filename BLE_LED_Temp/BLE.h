#include "Arduino.h"

#include "esp_log.h"
#include "nvs_flash.h"
#include "bt.h"
#include "bta_api.h"

#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_bt_main.h"


#define GATTS_TAG "BLELib"

///Declare the static function
static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

#define GATTS_NUM_HANDLE     10

String dev_name = "ESP32_BLE";

#define GATTS_DEMO_CHAR_VAL_LEN_MAX 0x40

#define PREPARE_BUF_MAX_SIZE 1024

uint8_t char1_str[] = {0x11, 0x22, 0x33};
esp_attr_value_t gatts_demo_char1_val =
{
  .attr_max_len = GATTS_DEMO_CHAR_VAL_LEN_MAX,
  .attr_len     = sizeof(char1_str),
  .attr_value   = char1_str,
};

uint8_t char2_str[] = {0x12, 0x24, 0x36};
esp_attr_value_t gatts_demo_char2_val =
{
  .attr_max_len = GATTS_DEMO_CHAR_VAL_LEN_MAX,
  .attr_len     = sizeof(char2_str),
  .attr_value   = char2_str,
};

int char1_handle_id = 0;
int char2_handle_id = 0;

static uint8_t test_service_uuid128[32] = {
  /* LSB <--------------------------------------------------------------------------------> MSB */
  //first uuid, 16bit, [12],[13] is the value
  0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xAB, 0xCD, 0x00, 0x00,
  //second uuid, 32bit, [12], [13], [14], [15] is the value
  0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xAB, 0xCD, 0xAB, 0xCD,
};

//static uint8_t test_manufacturer[TEST_MANUFACTURER_DATA_LEN] =  {0x12, 0x23, 0x45, 0x56};
static esp_ble_adv_data_t test_adv_data = {
  .set_scan_rsp = false,
  .include_name = true,
  .include_txpower = true,
  .min_interval = 0x20,
  .max_interval = 0x40,
  .appearance = 0x00,
  .manufacturer_len = 0, //TEST_MANUFACTURER_DATA_LEN,
  .p_manufacturer_data =  NULL, //&test_manufacturer[0],
  .service_data_len = 0,
  .p_service_data = NULL,
  .service_uuid_len = 32,
  .p_service_uuid = test_service_uuid128,
  .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static esp_ble_adv_params_t test_adv_params = {
  .adv_int_min        = 0x20,
  .adv_int_max        = 0x40,
  .adv_type           = ADV_TYPE_IND,
  .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
  .peer_addr          = {0},
  .peer_addr_type     = BLE_ADDR_TYPE_PUBLIC,
  .channel_map        = ADV_CHNL_ALL,
  .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

#define PROFILE_MAX 10
// #define PROFILE_A_APP_ID 0

struct gatts_profile_inst {
//  esp_gatts_cb_t gatts_cb;
  uint16_t gatts_if;
  uint16_t app_id;
  uint16_t conn_id;
  uint16_t service_handle;
  esp_gatt_srvc_id_t service_id;
  uint16_t char_handle;
  esp_bt_uuid_t char_uuid[2];
  esp_gatt_perm_t perm;
  esp_gatt_char_prop_t property;
  uint16_t descr_handle;
  esp_bt_uuid_t descr_uuid;
};

/* One gatt-based profile one app_id and one gatts_if, this array will store the gatts_if returned by ESP_GATTS_REG_EVT */
/*static struct gatts_profile_inst gl_profile_tab[PROFILE_NUM] = {
  [0] = {
    .gatts_cb = gatts_profile_a_event_handler,
    .gatts_if = ESP_GATT_IF_NONE,
  },
};*/
static struct gatts_profile_inst gl_profile_tab[PROFILE_MAX];

typedef enum {
	READ,
	WRITE
} event_t;


typedef void(*eCallback_fn)(int, int);

eCallback_fn onRead;
eCallback_fn onWrite;

int nowCount_profile = 0;
esp_ble_gatts_cb_param_t *paramNow;
char writeData[50];

typedef struct {
  uint8_t                 *prepare_buf;
  int                     prepare_len;
} prepare_type_env_t;

static prepare_type_env_t a_prepare_write_env;

void write_event_env(esp_gatt_if_t gatts_if, prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param);
void exec_write_event_env(prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param);

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
  switch (event) {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
      esp_ble_gap_start_advertising(&test_adv_params);
      break;
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
      esp_ble_gap_start_advertising(&test_adv_params);
      break;
    case ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT:
      esp_ble_gap_start_advertising(&test_adv_params);
      break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
      //advertising start complete event to indicate advertising start successfully or failed
      if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
        ESP_LOGE(GATTS_TAG, "Advertising start failed\n");
      }
      break;
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
      if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
        ESP_LOGE(GATTS_TAG, "Advertising stop failed\n");
      }
      else {
        ESP_LOGI(GATTS_TAG, "Stop adv successfully\n");
      }
      break;
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
      ESP_LOGI(GATTS_TAG, "update connetion params status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, timeout = %d",
               param->update_conn_params.status,
               param->update_conn_params.min_int,
               param->update_conn_params.max_int,
               param->update_conn_params.conn_int,
               param->update_conn_params.latency,
               param->update_conn_params.timeout);
      break;
    default:
      break;
  }
}

void write_event_env(esp_gatt_if_t gatts_if, prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param) {
  esp_gatt_status_t status = ESP_GATT_OK;
  if (param->write.need_rsp) {
    if (param->write.is_prep) {
      if (prepare_write_env->prepare_buf == NULL) {
        prepare_write_env->prepare_buf = (uint8_t *)malloc(PREPARE_BUF_MAX_SIZE * sizeof(uint8_t));
        prepare_write_env->prepare_len = 0;
        if (prepare_write_env->prepare_buf == NULL) {
          LOG_ERROR("Gatt_server prep no mem\n");
          status = ESP_GATT_NO_RESOURCES;
        }
      } else {
        if (param->write.offset > PREPARE_BUF_MAX_SIZE) {
          status = ESP_GATT_INVALID_OFFSET;
        } else if ((param->write.offset + param->write.len) > PREPARE_BUF_MAX_SIZE) {
          status = ESP_GATT_INVALID_ATTR_LEN;
        }
      }

      esp_gatt_rsp_t *gatt_rsp = (esp_gatt_rsp_t *)malloc(sizeof(esp_gatt_rsp_t));
      gatt_rsp->attr_value.len = param->write.len;
      gatt_rsp->attr_value.handle = param->write.handle;
      gatt_rsp->attr_value.offset = param->write.offset;
      gatt_rsp->attr_value.auth_req = ESP_GATT_AUTH_REQ_NONE;
      memcpy(gatt_rsp->attr_value.value, param->write.value, param->write.len);
      esp_err_t response_err = esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, status, gatt_rsp);
      if (response_err != ESP_OK) {
        LOG_ERROR("Send response error\n");
      }
      free(gatt_rsp);
      if (status != ESP_GATT_OK) {
        return;
      }
      memcpy(prepare_write_env->prepare_buf + param->write.offset,
             param->write.value,
             param->write.len);
      prepare_write_env->prepare_len += param->write.len;

    } else {
      esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, status, NULL);
    }
  }
}

void exec_write_event_env(prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param) {
  if (param->exec_write.exec_write_flag == ESP_GATT_PREP_WRITE_EXEC) {
    esp_log_buffer_hex(GATTS_TAG, prepare_write_env->prepare_buf, prepare_write_env->prepare_len);
  } else {
    ESP_LOGI(GATTS_TAG, "ESP_GATT_PREP_WRITE_CANCEL");
  }
  if (prepare_write_env->prepare_buf) {
    free(prepare_write_env->prepare_buf);
    prepare_write_env->prepare_buf = NULL;
  }
  prepare_write_env->prepare_len = 0;
}

int nowIndex = 0;

static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
  paramNow = param;
  switch (event) {
    case ESP_GATTS_REG_EVT:
      ESP_LOGI(GATTS_TAG, "REGISTER_APP_EVT, status %d, app_id %d\n", param->reg.status, param->reg.app_id);
      gl_profile_tab[nowIndex].service_id.is_primary = true;
      gl_profile_tab[nowIndex].service_id.id.inst_id = 0x00;
      gl_profile_tab[nowIndex].service_id.id.uuid.len = ESP_UUID_LEN_16;
      // gl_profile_tab[nowIndex].service_id.id.uuid.uuid.uuid16 = GATTS_SERVICE_UUID_TEST_A;
	  // gl_profile_tab[nowIndex].service_id.id.uuid.uuid.uuid16 = service_char_uuid_t[nowIndex].service_uuid;

      esp_ble_gap_set_device_name(dev_name.c_str());
#ifdef CONFIG_SET_RAW_ADV_DATA
      esp_ble_gap_config_adv_data_raw(raw_adv_data, sizeof(raw_adv_data));
      esp_ble_gap_config_scan_rsp_data_raw(raw_scan_rsp_data, sizeof(raw_scan_rsp_data));
#else
      esp_ble_gap_config_adv_data(&test_adv_data);
#endif
      esp_ble_gatts_create_service(gatts_if, &gl_profile_tab[nowIndex].service_id, GATTS_NUM_HANDLE);
      break;
    case ESP_GATTS_READ_EVT: {
        // ESP_LOGI(GATTS_TAG, "GATT_READ_EVT, conn_id %d, trans_id %d, handle %d\n", param->read.conn_id, param->read.trans_id, param->read.handle);
        Serial.printf("GATT_READ_EVT, conn_id %d, trans_id %d, handle %d\n", param->read.conn_id, param->read.trans_id, param->read.handle);
		
		if (onRead) {
			onRead(gl_profile_tab[nowIndex].service_id.id.uuid.uuid.uuid16, gl_profile_tab[nowIndex].char_uuid[(char1_handle_id == param->read.handle ? 0 : 1)].uuid.uuid16);
		}
        break;
      }
    case ESP_GATTS_WRITE_EVT: {
        ESP_LOGI(GATTS_TAG, "GATT_WRITE_EVT, conn_id %d, trans_id %d, handle %d\n", param->write.conn_id, param->write.trans_id, param->write.handle);
        ESP_LOGI(GATTS_TAG, "GATT_WRITE_EVT, value len %d, value %08x\n", param->write.len, *(uint32_t *)param->write.value);
        write_event_env(gatts_if, &a_prepare_write_env, param);
		
		if (onWrite) {
			strcpy(writeData, (const char*)param->write.value);
			onWrite(gl_profile_tab[nowIndex].service_id.id.uuid.uuid.uuid16, gl_profile_tab[nowIndex].char_uuid[(char1_handle_id == param->read.handle ? 0 : 1)].uuid.uuid16);
		}
        break;
      }
    case ESP_GATTS_EXEC_WRITE_EVT:
      ESP_LOGI(GATTS_TAG, "ESP_GATTS_EXEC_WRITE_EVT");
      esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
      exec_write_event_env(&a_prepare_write_env, param);
      break;
    case ESP_GATTS_MTU_EVT:
    case ESP_GATTS_CONF_EVT:
    case ESP_GATTS_UNREG_EVT:
      break;
    case ESP_GATTS_CREATE_EVT:
      ESP_LOGI(GATTS_TAG, "CREATE_SERVICE_EVT, status %d,  service_handle %d\n", param->create.status, param->create.service_handle);
      gl_profile_tab[nowIndex].service_handle = param->create.service_handle;
      gl_profile_tab[nowIndex].char_uuid[0].len = ESP_UUID_LEN_16;
	  // gl_profile_tab[nowIndex].char_uuid.uuid.uuid16 = GATTS_CHAR_UUID_TEST_A;
      // gl_profile_tab[nowIndex].char_uuid.uuid.uuid16 = service_char_uuid_t[nowIndex].char_uuid;

      esp_ble_gatts_start_service(gl_profile_tab[nowIndex].service_handle);

      esp_ble_gatts_add_char(gl_profile_tab[nowIndex].service_handle, &gl_profile_tab[nowIndex].char_uuid[0],
                             ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                             ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
                             &gatts_demo_char1_val, NULL);
	
	  // ----------------
	  gl_profile_tab[nowIndex].char_uuid[1].len = ESP_UUID_LEN_16;
	  gl_profile_tab[nowIndex].char_uuid[1].uuid.uuid16 = gl_profile_tab[nowIndex].char_uuid[0].uuid.uuid16 + 1;
	  esp_ble_gatts_add_char(gl_profile_tab[nowIndex].service_handle, &gl_profile_tab[nowIndex].char_uuid[1],
                             ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                             ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
                             &gatts_demo_char2_val, NULL);
	  // ----------------
	  
      break;
    case ESP_GATTS_ADD_INCL_SRVC_EVT:
      break;
    case ESP_GATTS_ADD_CHAR_EVT: {
        uint16_t length = 0;
        const uint8_t *prf_char;

        /* ESP_LOGI(GATTS_TAG, "ADD_CHAR_EVT, status %d,  attr_handle %d, service_handle %d\n",
                 param->add_char.status, param->add_char.attr_handle, param->add_char.service_handle);*/
		Serial.printf("ADD_CHAR_EVT, status %d,  attr_handle %d, service_handle %d\n",
                 param->add_char.status, param->add_char.attr_handle, param->add_char.service_handle);
		
		if (param->add_char.char_uuid.uuid.uuid16 == gl_profile_tab[nowIndex].char_uuid[0].uuid.uuid16)
			char1_handle_id = param->add_char.attr_handle;
		else if (param->add_char.char_uuid.uuid.uuid16 == gl_profile_tab[nowIndex].char_uuid[1].uuid.uuid16)
			char2_handle_id = param->add_char.attr_handle;
		
        gl_profile_tab[nowIndex].char_handle = param->add_char.attr_handle;
        gl_profile_tab[nowIndex].descr_uuid.len = ESP_UUID_LEN_16;
        gl_profile_tab[nowIndex].descr_uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
        esp_ble_gatts_get_attr_value(param->add_char.attr_handle,  &length, &prf_char);

        ESP_LOGI(GATTS_TAG, "the gatts demo char length = %x\n", length);
        for (int i = 0; i < length; i++) {
          ESP_LOGI(GATTS_TAG, "prf_char[%x] =%x\n", i, prf_char[i]);
        }
        esp_ble_gatts_add_char_descr(gl_profile_tab[nowIndex].service_handle, &gl_profile_tab[nowIndex].descr_uuid,
                                     ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, NULL, NULL);
        break;
      }
    case ESP_GATTS_ADD_CHAR_DESCR_EVT:
      //ESP_LOGI(GATTS_TAG, "ADD_DESCR_EVT, status %d, attr_handle %d, service_handle %d\n",
      //         param->add_char.status, param->add_char.attr_handle, param->add_char.service_handle);
	  // Serial.printf("ADD_DESCR_EVT, status %d, attr_handle %d, service_handle %d\n",
      //         param->add_char.status, param->add_char.attr_handle, param->add_char.service_handle);
      break;
    case ESP_GATTS_DELETE_EVT:
      break;
    case ESP_GATTS_START_EVT:
      ESP_LOGI(GATTS_TAG, "SERVICE_START_EVT, status %d, service_handle %d\n",
               param->start.status, param->start.service_handle);
      break;
    case ESP_GATTS_STOP_EVT:
      break;
    case ESP_GATTS_CONNECT_EVT: {
        esp_ble_conn_update_params_t conn_params = {0};
        memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
        /* For the IOS system, please reference the apple official documents about the ble connection parameters restrictions. */
        conn_params.latency = 0;
        conn_params.max_int = 0x50;    // max_int = 0x50*1.25ms = 100ms
        conn_params.min_int = 0x30;    // min_int = 0x30*1.25ms = 60ms
        conn_params.timeout = 400;    // timeout = 400*10ms = 4000ms
        ESP_LOGI(GATTS_TAG, "ESP_GATTS_CONNECT_EVT, conn_id %d, remote %02x:%02x:%02x:%02x:%02x:%02x:, is_conn %d\n",
                 param->connect.conn_id,
                 param->connect.remote_bda[0], param->connect.remote_bda[1], param->connect.remote_bda[2],
                 param->connect.remote_bda[3], param->connect.remote_bda[4], param->connect.remote_bda[5],
                 param->connect.is_connected);
        gl_profile_tab[nowIndex].conn_id = param->connect.conn_id;
        //start sent the update connection parameters to the peer device.
        esp_ble_gap_update_conn_params(&conn_params);
        break;
      }
    case ESP_GATTS_DISCONNECT_EVT:
      esp_ble_gap_start_advertising(&test_adv_params);
      break;
    case ESP_GATTS_OPEN_EVT:
    case ESP_GATTS_CANCEL_OPEN_EVT:
    case ESP_GATTS_CLOSE_EVT:
    case ESP_GATTS_LISTEN_EVT:
    case ESP_GATTS_CONGEST_EVT:
    default:
      break;
  }
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
  /* If event is register event, store the gatts_if for each profile */
  if (event == ESP_GATTS_REG_EVT) {
    if (param->reg.status == ESP_GATT_OK) {
      gl_profile_tab[param->reg.app_id].gatts_if = gatts_if;
    } else {
      ESP_LOGI(GATTS_TAG, "Reg app failed, app_id %04x, status %d\n",
               param->reg.app_id,
               param->reg.status);
      return;
    }
  }

  /* If the gatts_if equal to profile A, call profile A cb handler,
     so here call each profile's callback */
  do {
    int idx;
    for (idx = 0; idx < nowCount_profile; idx++) {
      if (gatts_if == ESP_GATT_IF_NONE || /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
          gatts_if == gl_profile_tab[idx].gatts_if) {
//        if (gl_profile_tab[idx].gatts_cb) {
          nowIndex = idx;
//          gl_profile_tab[idx].gatts_cb(event, gatts_if, param);
		  gatts_profile_event_handler(event, gatts_if, param);
//        }
      }
    }
  } while (0);
}

class BLE {
	public:
		BLE(String name) {
			dev_name = name;
		};
		
		bool begin() {
			esp_err_t ret;

			// Initialize NVS.
			ret = nvs_flash_init();
			if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
				ESP_ERROR_CHECK(nvs_flash_erase());
				ret = nvs_flash_init();
			}
			ESP_ERROR_CHECK( ret );

			esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
			ret = esp_bt_controller_init(&bt_cfg);
			if (ret) {
				ESP_LOGE(GATTS_TAG, "%s initialize controller failed\n", __func__);
				return false;
			}

			ret = esp_bt_controller_enable(ESP_BT_MODE_BTDM);
			if (ret) {
				ESP_LOGE(GATTS_TAG, "%s enable controller failed\n", __func__);
				return false;
			}
			ret = esp_bluedroid_init();
			if (ret) {
				ESP_LOGE(GATTS_TAG, "%s init bluetooth failed\n", __func__);
				return false;
			}
			ret = esp_bluedroid_enable();
			if (ret) {
				ESP_LOGE(GATTS_TAG, "%s enable bluetooth failed\n", __func__);
				return false;
			}

			esp_ble_gatts_register_callback(gatts_event_handler);
			esp_ble_gap_register_callback(gap_event_handler);
		}
		
		void on(event_t event, eCallback_fn callback) {
			if (event == READ) onRead = callback;
			if (event == WRITE) onWrite = callback;
		}
		
		char* data() {
			return writeData;
		}
		
		void addCharacteristic(int service_uuid, int char_uuid) {
			int profileIndex = nowCount_profile++;
			
			gl_profile_tab[profileIndex] = {
//				.gatts_cb = gatts_profile_event_handler,
				.gatts_if = ESP_GATT_IF_NONE,
			};
			gl_profile_tab[profileIndex].service_id.id.uuid.uuid.uuid16 = service_uuid;
			gl_profile_tab[profileIndex].char_uuid[0].uuid.uuid16 = char_uuid;
			
			esp_ble_gatts_app_register(profileIndex);
		}
		
		void reply(char *data, int len) {
			esp_gatt_rsp_t rsp;
			memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
			rsp.attr_value.handle = paramNow->read.handle;
			rsp.attr_value.len = len;
			for (int i=0;i<len;i++) rsp.attr_value.value[i] = data[i];
//			strncpy(rsp.attr_value.value, data, len);
			esp_ble_gatts_send_response(gl_profile_tab[nowIndex].gatts_if, paramNow->read.conn_id, paramNow->read.trans_id,
										ESP_GATT_OK, &rsp);
		}
};
