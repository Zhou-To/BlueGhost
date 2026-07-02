#include <Arduino.h>
// 基础的蓝牙功能库
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLESecurity.h>
#include <BLEAdvertising.h>

#include <mbedtls/md.h>  
#include <mbedtls/sha256.h>  
#include <esp_random.h>  
#include <map>   

// EIB Defense Configuration
#define EIB_BINDING_EXPIRY_MS (24 * 60 * 60 * 1000) 
#define EIB_MAX_BINDINGS 50  // 最多允许记录 50 个配对设备


//定义一个结构体：记录了一个设备的配对信息
// EIB Binding Structure
struct EIBBinding {
    uint8_t peer_addr[6];  // 对方的蓝牙地址 (MAC地址)
    uint8_t ephemeral_irk[16];  // 发给对方的“临时身份证 (IRK)”
    unsigned long expiry_time;  // 过期时间戳
    bool is_active;  // 这个记录是否还有效
};

// Global EIB state
static std::map<uint16_t, EIBBinding> eib_bindings;  // 存放所有的配对记录
static uint8_t master_irk[16]; // 主IRK
static bool eib_enabled = true;  // 防御系统的总开关：默认开启

// --- EIB Defense Functions ---

//生成真正的主密钥(Master IRK)
void generate_master_irk() {
    esp_fill_random(master_irk, sizeof(master_irk));
//    Serial.println("Generated Master IRK for EIB defense");
}
//生成一串随机的salt(Nonce)
void generate_random_nonce(uint8_t* nonce, size_t len) {
    esp_fill_random(nonce, len);
}


bool eib_kdf(const uint8_t* salt, size_t salt_len, const uint8_t* ikm, size_t ikm_len, uint8_t* okm, size_t okm_len) {
    // Simple KDF using SHA256: OKM = SHA256(salt || ikm || counter)
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);


    // 0 = SHA256 (not SHA224)
    mbedtls_sha256_starts(&ctx, 0); 

    // Input: salt || 主密钥ikm || 计数器counter// 
    if (salt && salt_len > 0) {
        mbedtls_sha256_update(&ctx, salt, salt_len);
    }

    mbedtls_sha256_update(&ctx, ikm, ikm_len);

    // Add a simple counter for key expansion
    uint8_t counter = 0x01;
    mbedtls_sha256_update(&ctx, &counter, 1);

    
    // 输出 32 字节的哈希值
    uint8_t hash[32];
    mbedtls_sha256_finish(&ctx, hash);

    mbedtls_sha256_free(&ctx);

    // Copy the requested output length (max 32 bytes for SHA256)
    //我们只需要前 16 个字节作为最终的临时密钥 (okm)
    size_t copy_len = (okm_len > 32) ? 32 : okm_len;
    memcpy(okm, hash, copy_len);

    return true;
}

uint16_t generate_binding_id() {
    static uint16_t next_id = 1;
    return next_id++;
}

void cleanup_expired_bindings() {
    unsigned long current_time = millis();
    auto it = eib_bindings.begin();
    while (it != eib_bindings.end()) {
        if (it->second.is_active && current_time > it->second.expiry_time) {
            Serial.printf("EIB: Cleaning up expired binding ID %d\n", it->first);
            it = eib_bindings.erase(it);
        } else {
            ++it;
        }
    }
}


bool store_eib_binding(const uint8_t* peer_addr, const uint8_t* eph_irk) {
    cleanup_expired_bindings();

    if (eib_bindings.size() >= EIB_MAX_BINDINGS) {
        Serial.println("EIB: Binding storage full, cannot store new binding");
        return false;
    }

    uint16_t binding_id = generate_binding_id();
    EIBBinding binding;
    memcpy(binding.peer_addr, peer_addr, 6);
    memcpy(binding.ephemeral_irk, eph_irk, 16);
    binding.expiry_time = millis() + EIB_BINDING_EXPIRY_MS;
    binding.is_active = true;

    eib_bindings[binding_id] = binding;

//    Serial.printf("EIB: Stored binding ID %d for peer ", binding_id);
//    for (int i = 0; i < 6; i++) {
//        Serial.printf("%02X:", peer_addr[i]);
//    }
//    Serial.printf(" expires in %lu ms\n", EIB_BINDING_EXPIRY_MS);

    return true;
}


// 检查当前配对是不是 "Just Works"
bool is_just_works_pairing() {
    // Check if current pairing uses Just Works
    // In this implementation, since we set ESP_IO_CAP_NONE, it's always Just Works
    return true;
}


// 调度函数：决定是生成临时身份证，还是直接给真实的身份证
bool generate_ephemeral_irk(uint8_t* eph_irk_out) {
    if (!eib_enabled) {
        memcpy(eph_irk_out, master_irk, 16);
        return true;
    }

    uint8_t nonce[16];
    generate_random_nonce(nonce, sizeof(nonce));

    if (!eib_kdf(nonce, sizeof(nonce), master_irk, sizeof(master_irk), eph_irk_out, 16)) {
        Serial.println("EIB: Failed to generate ephemeral IRK, using master IRK");
        memcpy(eph_irk_out, master_irk, 16);
        return false;
    }

    Serial.println("EIB: Generated ephemeral IRK for Just Works pairing");
    return true;
}


// --- Original Functions ---
void print_key(const char* key_name, const uint8_t* key_value, uint8_t key_len) {
    Serial.printf("%s: ", key_name);
    for (int i = 0; i < key_len; i++) {
        Serial.printf("%02X", key_value[i]);
    }
    Serial.println();
}

void my_gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
        case ESP_GAP_BLE_KEY_EVT: {
//            Serial.println("\n--- Low-level GAP Key Event ---");
            esp_ble_key_t *key_info = &param->ble_security.ble_key;

            // EIB Defense: Intercept IRK distribution
            if (key_info->key_type == ESP_LE_KEY_LID && eib_enabled) {
//                Serial.println("EIB: Intercepting Local IRK distribution");

                if (is_just_works_pairing()) {
                    uint8_t ephemeral_irk[16];
           
                    if (generate_ephemeral_irk(ephemeral_irk)) {
                        // Store the binding
                        uint8_t peer_addr[6];
                        // Note: peer address should be obtained from connection context
                        // For this demo, we'll use a placeholder
                        memcpy(peer_addr, key_info->bd_addr, 6);
                       
                        store_eib_binding(peer_addr, ephemeral_irk);

                        // Replace the IRK in the key structure
                        memcpy((uint8_t*)key_info->p_key_value.pid_key.irk, ephemeral_irk, 16);
                        print_key("EIB Ephemeral IRK", ephemeral_irk, 16);
                    }
                } else {
                    Serial.println("EIB: High-security pairing detected, using master IRK");
                    print_key("Master IRK", key_info->p_key_value.pid_key.irk, 16);
                }
            } else {
                // Original key handling
                switch (key_info->key_type) {
                    case ESP_LE_KEY_PENC:
                        print_key("Peer LTK", key_info->p_key_value.penc_key.ltk, 16);
                        break;
                    case ESP_LE_KEY_PID:
                        print_key("Peer IRK", key_info->p_key_value.pid_key.irk, 16);
                        break;
                    case ESP_LE_KEY_PCSRK:
                        print_key("Peer CSRK", key_info->p_key_value.pcsrk_key.csrk, 16);
                        break;
                    case ESP_LE_KEY_LENC:
                        print_key("Local LTK", key_info->p_key_value.lenc_key.ltk, 16);
                        break;
                    case ESP_LE_KEY_LID:
                        print_key("Local IRK", key_info->p_key_value.pid_key.irk, 16);
                        break;
                    case ESP_LE_KEY_LCSRK:
                        print_key("Local CSRK", key_info->p_key_value.lcsrk_key.csrk, 16);
                        break;
                    default:
                        break;
                }
            }
            break;
        }

        case ESP_GAP_BLE_AUTH_CMPL_EVT: {
             Serial.println("--- Auth Complete ---");
             esp_ble_auth_cmpl_t *auth_cmpl = &param->ble_security.auth_cmpl;

             if (auth_cmpl->success) {
                Serial.println("Pairing SUCCESS");
                if (eib_enabled && is_just_works_pairing()) {
                    Serial.println("EIB: Just Works pairing completed with ephemeral IRK protection");
                }
             } else {
                Serial.printf("Pairing FAILED. Reason: 0x%x\n", auth_cmpl->fail_reason);
             }
             Serial.println("------------------------------------");
             break;
        }

        default:
            break;
    }
}

// 客户端连接回调
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer, esp_ble_gatts_cb_param_t *param) override {
        Serial.println("Client Connected");
        
        if (eib_enabled) {
            Serial.println("EIB: Defense active - ephemeral IRKs will be used for JW pairing");
        }
// 强制要求加密配对
        esp_ble_set_encryption(param->connect.remote_bda, ESP_BLE_SEC_ENCRYPT);
    }

    void onDisconnect(BLEServer* pServer) override {
        Serial.println("Client Disconnected, restarting advertising...");
        BLEDevice::getAdvertising()->start();
    }
};

// EIB Control Functions
void eib_enable() {
    eib_enabled = true;
    Serial.println("EIB Defense: ENABLED");
}

void eib_disable() {
    eib_enabled = false;
    Serial.println("EIB Defense: DISABLED");
}

void eib_status() {
//    Serial.printf("EIB Defense Status: %s\n", eib_enabled ? "ENABLED" : "DISABLED");
//    Serial.printf("Active bindings: %d\n", eib_bindings.size());
//    Serial.printf("Max bindings: %d\n", EIB_MAX_BINDINGS);
//    Serial.printf("Binding expiry: %lu ms\n", EIB_BINDING_EXPIRY_MS);
}

void eib_list_bindings() {
    cleanup_expired_bindings();
    Serial.printf("EIB Active Bindings (%d):\n", eib_bindings.size());

    unsigned long current_time = millis();
    for (const auto& pair : eib_bindings) {
        const EIBBinding& binding = pair.second;
        unsigned long remaining = binding.expiry_time > current_time ?
                                 binding.expiry_time - current_time : 0;

        Serial.printf("  ID %d: ", pair.first);
        for (int i = 0; i < 6; i++) {
            Serial.printf("%02X:", binding.peer_addr[i]);
        }
        Serial.printf(" expires in %lu ms\n", remaining);
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("Starting BLE Server with EIB Defense");

    // Initialize EIB defense
    generate_master_irk();
    eib_enable();

    BLEDevice::init("Bob-EIB");
    BLEDevice::setCustomGapHandler(my_gap_event_handler);
    BLEServer *pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLESecurity *pSecurity = new BLESecurity();
    pSecurity->setCapability(ESP_IO_CAP_NONE);  // Just Works pairing
    pSecurity->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_BOND);
    pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK | ESP_BLE_CSR_KEY_MASK);
    pSecurity->setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK | ESP_BLE_CSR_KEY_MASK);

    // --- Service Creation (unchanged) ---
    BLEService *pBatteryService = pServer->createService(BLEUUID((uint16_t)0x180F));
    BLECharacteristic *pBatteryLevelCharacteristic = pBatteryService->createCharacteristic(
        BLEUUID((uint16_t)0x2A19),
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    pBatteryLevelCharacteristic->setValue("88");
    pBatteryLevelCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);
    pBatteryService->start();

    BLEService *pDeviceInfoService = pServer->createService(BLEUUID((uint16_t)0x180A));
    pDeviceInfoService->createCharacteristic(BLEUUID((uint16_t)0x2A29), BLECharacteristic::PROPERTY_READ)->setValue("ESP32 EIB Defense Co.");
    BLEService *pHIDService = pServer->createService(BLEUUID((uint16_t)0x1812));
    pHIDService->start();

    // --- Advertising Configuration ---
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(pBatteryService->getUUID());
    pAdvertising->addServiceUUID(pDeviceInfoService->getUUID());
    pAdvertising->addServiceUUID(pHIDService->getUUID());
    pAdvertising->setAppearance(0x0040);

    BLEAdvertisementData scanResponseData = BLEAdvertisementData();
    scanResponseData.setName("Bob-EIB");
    pAdvertising->setScanResponseData(scanResponseData);
    pAdvertising->setScanResponse(true);

    pAdvertising->start();

    Serial.println("Advertising with EIB Defense enabled. Waiting for connection...");
//    Serial.println("Commands: 'eib_status', 'eib_enable', 'eib_disable', 'eib_list'");

    eib_status();
}

void loop() {
    // Check for serial commands
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        command.trim();

        if (command == "eib_status") {
            eib_status();
        } else if (command == "eib_enable") {
            eib_enable();
        } else if (command == "eib_disable") {
            eib_disable();
        } else if (command == "eib_list") {
            eib_list_bindings();
        } else {
            Serial.println("Unknown command. Available: eib_status, eib_enable, eib_disable, eib_list");
        }
    }

    delay(2000);
}
