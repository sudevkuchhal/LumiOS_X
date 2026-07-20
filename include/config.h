#ifndef CONFIG_H
#define CONFIG_H

// ======================================================
// LumiOS X - Project Configuration
// Version : 1.0.0
// Board   : ESP32 DevKit V1
// ======================================================

// ----------------------
// Project Information
// ----------------------
#define PROJECT_NAME    "LumiOS X"
#define PROJECT_VERSION "1.0.0"

// ----------------------
// Component Versions (Version Manager)
// Bumped independently as each layer changes so the dashboard can
// show exactly which firmware/API/UI/OLED combination is running,
// instead of a single opaque PROJECT_VERSION.
// ----------------------
#define FIRMWARE_VERSION   "1.0.0"
#define API_VERSION        "1.0"
#define DASHBOARD_VERSION  "1.0"
#define OLED_VERSION       "1.0"

// ----------------------
// OLED Display (I2C)
// ----------------------
#define OLED_SDA 21
#define OLED_SCL 22

// ----------------------
// LED GPIO Pins
// ----------------------
#define LED1_PIN 13
#define LED2_PIN 14
#define LED3_PIN 25
#define LED4_PIN 26
#define LED5_PIN 27

// ----------------------
// Status LED (Onboard)
// ----------------------
#define STATUS_LED_PIN 2

// ----------------------
// LED Count
// ----------------------
#define TOTAL_LEDS 5

// ----------------------
// Serial Monitor
// ----------------------
#define SERIAL_BAUDRATE 115200

// ----------------------
// Boot Settings
// ----------------------
#define BOOT_DELAY 250


// ----------------------
// sensor pin
// ----------------------
#define DHT_PIN   4
#define DHT_TYPE  DHT11

// ----------------------
// HC-SR04 Ultrasonic Distance Sensor
// ----------------------
#define TRIG_PIN  32
#define ECHO_PIN  33
#define DISTANCE_WARN_CM   15.0   // object closer than this = warning
#define DISTANCE_MAX_CM    400.0  // sensor max usable range

// ----------------------
// MQ2 Smoke / Gas Sensor (analog)
// ----------------------
#define MQ2_PIN   35
#define SMOKE_WARN_RAW     1800   // raw ADC (0-4095) above this = warning
#define SMOKE_DANGER_RAW   2800   // raw ADC above this = danger

// ----------------------
// Boot Counter (persisted)
// ----------------------
#define BOOTCOUNT_FILE "/bootcount.txt"

// ----------------------
// Cloud Relay (Stage I) — remote access from ANY network
//
// The ESP32 makes an OUTBOUND connection to a public MQTT
// broker over the internet (no port forwarding, no VPS).
// The phone dashboard (remote_dashboard.html) connects to the
// SAME broker over MQTT-over-WebSockets and both sides "meet"
// there. Works whether the phone is on the same WiFi or on
// mobile data, anywhere in the world.
//
// IOT_DEVICE_ID acts like a shared secret / channel name.
// CHANGE IT to something unique before you go "live" — anyone
// who knows this ID and the broker can read your status and
// send commands, because broker.hivemq.com is a public/free
// test broker with no login. Fine for learning; swap to a
// private broker (e.g. HiveMQ Cloud free tier, with
// username/password + TLS) before trusting it with anything
// real. See CHANGELOG for the upgrade path.
// ----------------------
#define MQTT_BROKER               "broker.hivemq.com"
#define MQTT_PORT                 1883
#define IOT_DEVICE_ID             "lumiosx_sudev_9f21"   // <-- change this
#define MQTT_TOPIC_STATUS         "lumiosx/" IOT_DEVICE_ID "/status"
#define MQTT_TOPIC_CMD            "lumiosx/" IOT_DEVICE_ID "/cmd"
#define MQTT_PUBLISH_INTERVAL_MS  3000UL   // how often we publish live data

// ----------------------
// Cloud Relay — Stage I.2 (reliability additions)
//
// These are ADDITIVE — MQTT_TOPIC_STATUS / MQTT_TOPIC_CMD above are
// completely unchanged, so nothing that already worked stops working.
//
//   /avail     - retained "online"/"offline" (set via MQTT Last-Will).
//                Lets the dashboard know the ESP32 itself dropped off,
//                even if the browser's own broker connection is fine.
//   /heartbeat - tiny, NOT retained, published every few seconds.
//                The dashboard uses "have I heard a heartbeat recently"
//                as its offline-detection clock (independent of /status,
//                so a stalled sensor read can't fake "online").
//   /ack       - published right after a /cmd is executed, echoing the
//                command id back so the dashboard can confirm it landed
//                instead of just hoping the publish worked.
// ----------------------
#define MQTT_TOPIC_AVAIL           "lumiosx/" IOT_DEVICE_ID "/avail"
#define MQTT_TOPIC_HEARTBEAT       "lumiosx/" IOT_DEVICE_ID "/heartbeat"
#define MQTT_TOPIC_ACK             "lumiosx/" IOT_DEVICE_ID "/ack"

#define MQTT_HEARTBEAT_INTERVAL_MS 5000UL  // how often we send a heartbeat
#define MQTT_RECONNECT_INTERVAL_MS 5000UL  // how often we retry a dropped connection
#define MQTT_RECONNECT_MAX_BACKOFF_MS 60000UL // reconnect backoff ceiling

// ----------------------
// WiFi Recovery — exponential backoff (v1.0)
// Cooldown between full retry cycles grows after each consecutive
// failed cycle instead of a fixed 6s wait, so a genuinely unreachable
// network doesn't cause endless rapid retries. Resets to the base
// value the moment a connection succeeds.
// ----------------------
#define WIFI_COOLDOWN_BASE_MS      6000UL
#define WIFI_COOLDOWN_MAX_MS       60000UL

// ----------------------
// Health Monitor — thresholds (v1.0)
// Crossing these fires a NotifPriority::WARNING/CRITICAL through the
// existing NotificationManager, so it shows up on both the OLED alert
// overlay and the dashboard notification console automatically.
// ----------------------
#define HEALTH_HEAP_WARN_BYTES     40000UL   // free heap below this = warning
#define HEALTH_HEAP_CRIT_BYTES     20000UL   // free heap below this = critical
#define HEALTH_FS_WARN_PERCENT     80        // LittleFS usage above this = warning
#define HEALTH_FS_CRIT_PERCENT     95        // LittleFS usage above this = critical
#define HEALTH_CHECK_INTERVAL_MS   4000UL

// ----------------------
// OTA (v1.0) — Firmware Validation / Protected OTA
// ----------------------
#define OTA_MIN_FIRMWARE_BYTES     30000UL   // reject absurdly small uploads early
#define OTA_MAGIC_BYTE             0xE9      // first byte of a valid ESP32 app image

// ----------------------
// Error / Event Logger (v1.0) — persisted, rotating text logs
// ----------------------
#define ERROR_LOG_FILE            "/errorlog.txt"
#define EVENT_LOG_FILE            "/eventlog.txt"
#define LOG_MAX_LINES             120   // oldest lines trimmed beyond this
#define CRASHCOUNT_FILE           "/crashcount.txt"
#define OTA_INFO_FILE             "/otainfo.txt"

#endif