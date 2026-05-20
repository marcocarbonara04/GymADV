#include <M5Cardputer.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <SD.h>
#include <SPI.h>
#include <DNSServer.h>
#include "web_dashboard.h"
#include "exercises.h"
#include <sys/time.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// --- POWER MANAGEMENT & VBT ---
bool wifi_enabled = false;

// Forward declarations/definitions for workout states
enum WorkoutState {
    STATE_READY,
    STATE_ACTIVE,
    STATE_SUMMARY
};
extern volatile WorkoutState workout_state;

// --- BLE HEART RATE MONITOR STATE ---
static BLEUUID serviceUUID((uint16_t)0x180D);
static BLEUUID charUUID((uint16_t)0x2A37);

static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = true;
static BLERemoteCharacteristic* pRemoteCharacteristic = nullptr;
static BLEAdvertisedDevice* myDevice = nullptr;

volatile int current_heart_rate = 0;
volatile unsigned long last_heart_rate_time = 0;
bool ble_enabled = true; // On by default

// Core-0 tracking for active sets
volatile int sum_hr_active = 0;
volatile int count_hr_active = 0;
volatile int max_hr_active = 0;

#include <vector>
std::vector<int> hr_active_series;
std::vector<int> hr_recovery_series;

bool show_live_curve = true;
int hr_live_history[60] = {0};
int hr_live_history_count = 0;

static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {
    if (length >= 2) {
        uint8_t flags = pData[0];
        int hr = 0;
        if (flags & 0x01) {
            hr = (pData[2] << 8) | pData[1];
        } else {
            hr = pData[1];
        }
        current_heart_rate = hr;
        last_heart_rate_time = millis();
        
        // Populate live 60-second scrolling window (1Hz)
        static unsigned long last_history_append = 0;
        if (millis() - last_history_append >= 1000) {
            last_history_append = millis();
            for (int k = 0; k < 59; k++) {
                hr_live_history[k] = hr_live_history[k+1];
            }
            hr_live_history[59] = hr;
            if (hr_live_history_count < 60) hr_live_history_count++;
        }
        
        // Asynchronously track workout HR metrics on Core-0
        if (workout_state == STATE_ACTIVE) {
            sum_hr_active += hr;
            count_hr_active++;
            if (hr > max_hr_active) {
                max_hr_active = hr;
            }
            hr_active_series.push_back(hr);
        } else if (workout_state == STATE_SUMMARY) {
            hr_recovery_series.push_back(hr);
        }
    }
}

class MyClientCallback : public BLEClientCallbacks {
    void onConnect(BLEClient* pclient) {
        connected = true;
    }
    void onDisconnect(BLEClient* pclient) {
        connected = false;
        doConnect = false;
        doScan = true;
        current_heart_rate = 0;
        pRemoteCharacteristic = nullptr;
        Serial.println("BLE Heart Rate Client Disconnected");
    }
};

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {
            BLEDevice::getScan()->stop();
            if (myDevice != nullptr) {
                delete myDevice;
            }
            myDevice = new BLEAdvertisedDevice(advertisedDevice);
            doConnect = true;
            doScan = false;
            Serial.println("BLE Heart Rate Device found!");
        }
    }
};

bool connectToServer() {
    if (myDevice == nullptr) return false;
    
    Serial.print("Connecting to: ");
    Serial.println(myDevice->getAddress().toString().c_str());
    
    BLEClient* pClient = BLEDevice::createClient();
    pClient->setClientCallbacks(new MyClientCallback());

    if (!pClient->connect(myDevice)) {
        Serial.println("Failed to connect to BLE server");
        delete pClient;
        return false;
    }

    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr) {
        Serial.println("Failed to find HR service");
        pClient->disconnect();
        delete pClient;
        return false;
    }

    pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
    if (pRemoteCharacteristic == nullptr) {
        Serial.println("Failed to find HR Measurement characteristic");
        pClient->disconnect();
        delete pClient;
        return false;
    }

    if (pRemoteCharacteristic->canNotify()) {
        pRemoteCharacteristic->registerForNotify(notifyCallback);
    } else {
        Serial.println("Characteristic does not support notifications!");
        pClient->disconnect();
        delete pClient;
        return false;
    }

    connected = true;
    return true;
}

void disconnectBLE() {
    connected = false;
    doConnect = false;
    doScan = false;
    current_heart_rate = 0;
    BLEDevice::getScan()->stop();
    Serial.println("BLE explicitly stopped");
}

TaskHandle_t BLETaskHandle;
void bleTask(void *pvParameters) {
    BLEScan* pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setInterval(1349);
    pBLEScan->setWindow(449);
    pBLEScan->setActiveScan(true);

    for (;;) {
        if (ble_enabled) {
            if (doConnect == true) {
                if (connectToServer()) {
                    Serial.println("Connected to Heart Rate Monitor!");
                } else {
                    doScan = true;
                }
                doConnect = false;
            }

            if (!connected && doScan) {
                Serial.println("Scanning for Heart Rate Monitor...");
                BLEDevice::getScan()->start(5, false);  // Scan for 5 seconds
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
unsigned long last_activity_time = 0;
uint8_t current_backlight_brightness = 160; // 0-255 scale
unsigned long last_key_activity_time = 0;
bool is_time_synced = false;

float current_velocity = 0.0f;
float peak_velocity = 0.0f;
float last_rep_peak_vel = 0.0f;
std::vector<float> current_set_rep_velocities;

JsonDocument last_weights_doc;

// --- CONFIGURATION ---
const char* AP_SSID = "GymTracker";
const char* AP_PASS = "12345678";
const char* SD_DIR = "/GymTracker";
const int SD_CS_PIN = 12; // M5Cardputer SD card CS pin
bool sd_available = false;
bool show_wifi_qr = false;

// Dynamically fetch the active filesystem for workout logs (SD if present, otherwise LittleFS fallback)
fs::FS& getWorkoutLogFS() {
    if (sd_available) {
        return SD;
    }
    return LittleFS;
}

// IMU Calibration Thresholds
// Adjust these depending on the sensor mounting and noise.
// 1.0g is resting. > 1.0g means accelerating against gravity (pulling up).
// < 1.0g means accelerating with gravity (dropping down).
// Tuned for weight stack: adjusted to be highly sensitive to short travel movements (few cm)
float THRESHOLD_HIGH = 1.04f; 
float THRESHOLD_LOW = 0.96f;

// --- STATE MACHINES ---

enum RepPhase {
    PHASE_REST,
    PHASE_CONCENTRIC,
    PHASE_ECCENTRIC
};

// --- SHARED VARIABLES (Cross-Core) ---
volatile WorkoutState workout_state = STATE_READY;
volatile RepPhase rep_phase = PHASE_REST;
volatile bool is_eccentric_first = false;
volatile bool sound_muted = false;

volatile int current_weight = 20;
volatile int current_reps = 0;
volatile int current_poor_form_reps = 0;
volatile int rep_beep_type = 0;  // 1=Good, 2=Ok, 3=Bad

volatile uint16_t bg_color = BLACK;
float total_volume = 0;
int today_total_volume = 0;
int today_total_sets = 0;
int today_total_reps = 0;
int today_bad_reps = 0;

void loadHistory();

unsigned long set_start_time = 0;
unsigned long rest_start_time = 0;
unsigned long session_start_time = 0;
unsigned long concentric_start_time = 0;
unsigned long eccentric_start_time = 0;
unsigned long phase_start_time = 0;
unsigned long current_concentric_duration = 0;
unsigned long current_eccentric_duration = 0;

float current_rep_max_acc = 1.0f;
float current_rep_min_acc = 1.0f;

float filtered_acc = 1.0f;
// Moderate low-pass filter (0.15) to balance noise filtering and rapid tracking of short movements.
const float ALPHA_LPF = 0.15f;

bool editing_weight = false;
String weight_input_str = "";

bool editing_reps = false;
String reps_input_str = "";

// Feature: configurable rest timer
const int REST_PRESETS[] = {60, 90, 120, 180};
const int NUM_REST_PRESETS = 4;
int rest_target_idx = 1;  // default 90s
int rest_target_sec = 90;

// Feature: set counter per exercise
int current_set_number = 0;

// Feature: 1RM tracking
float estimated_1rm = 0;

String active_exercise = "Bench Press";
String prev_exercise = "";

// PRs, Favorites & Search
JsonDocument pr_doc;
int pr_scroll = 0;

bool is_favorite[NUM_MUSCLES][MAX_EX_PER_MUSCLE] = {false};
int current_ex_list[128];
int current_ex_count = 0;

String search_query = "";

// Dynamic custom exercises variables
std::vector<String> custom_exercises;
bool creating_custom_exercise = false;
String custom_exercise_input_str = "";
struct SearchResult { int m; int e; };
SearchResult search_results[64];
int search_count = 0;

struct SessionSet {
    int s; // set number
    int r; // reps
    int w; // weight
    int v; // volume
    int pf; // poor form reps
    int est_1rm;
    int hr_avg;
    int hr_max;
    int hr_rec_avg;
    int hr_rec_max;
    std::vector<int> hr_series;
    std::vector<int> hr_rec_series;
    std::vector<float> rep_velocities; // VBT peak velocity profile for each rep (m/s)
};

struct SessionExercise {
    String name;
    std::vector<SessionSet> sets;
};

struct SessionLog {
    unsigned long long t; // timestamp of session start
    std::vector<SessionExercise> exercises;
};

// Global active session
SessionLog active_session;

void serializeSession(const SessionLog &session, JsonVariant doc) {
    doc["t"] = session.t;
    JsonArray exercisesArr = doc["exercises"].to<JsonArray>();
    for (const auto &ex : session.exercises) {
        JsonObject exObj = exercisesArr.add<JsonObject>();
        exObj["ex"] = ex.name;
        JsonArray setsArr = exObj["sets"].to<JsonArray>();
        for (const auto &s : ex.sets) {
            JsonObject sObj = setsArr.add<JsonObject>();
            sObj["s"] = s.s;
            sObj["r"] = s.r;
            sObj["w"] = s.w;
            sObj["v"] = s.v;
            sObj["pf"] = s.pf;
            sObj["1rm"] = s.est_1rm;
            sObj["hr_avg"] = s.hr_avg;
            sObj["hr_max"] = s.hr_max;
            sObj["hr_rec_avg"] = s.hr_rec_avg;
            sObj["hr_rec_max"] = s.hr_rec_max;
            
            JsonArray hrArr = sObj["hr_series"].to<JsonArray>();
            for (int val : s.hr_series) hrArr.add(val);
            
            JsonArray recArr = sObj["hr_rec_series"].to<JsonArray>();
            for (int val : s.hr_rec_series) recArr.add(val);
            
            JsonArray velArr = sObj["rep_vel"].to<JsonArray>();
            for (float val : s.rep_velocities) velArr.add(val);
        }
    }
}

void deserializeSession(JsonVariant doc, SessionLog &session) {
    session.t = doc["t"] | 0ULL;
    session.exercises.clear();
    if (doc["exercises"].is<JsonArray>()) {
        JsonArray exercisesArr = doc["exercises"].as<JsonArray>();
        for (JsonObject exObj : exercisesArr) {
            SessionExercise ex;
            ex.name = exObj["ex"] | "";
            if (exObj["sets"].is<JsonArray>()) {
                JsonArray setsArr = exObj["sets"].as<JsonArray>();
                for (JsonObject sObj : setsArr) {
                    SessionSet s;
                    s.s = sObj["s"] | 0;
                    s.r = sObj["r"] | 0;
                    s.w = sObj["w"] | 0;
                    s.v = sObj["v"] | 0;
                    s.pf = sObj["pf"] | 0;
                    s.est_1rm = sObj["1rm"] | 0;
                    s.hr_avg = sObj["hr_avg"] | 0;
                    s.hr_max = sObj["hr_max"] | 0;
                    s.hr_rec_avg = sObj["hr_rec_avg"] | 0;
                    s.hr_rec_max = sObj["hr_rec_max"] | 0;
                    
                    s.hr_series.clear();
                    if (sObj["hr_series"].is<JsonArray>()) {
                        for (int val : sObj["hr_series"].as<JsonArray>()) {
                            s.hr_series.push_back(val);
                        }
                    }
                    
                    s.hr_rec_series.clear();
                    if (sObj["hr_rec_series"].is<JsonArray>()) {
                        for (int val : sObj["hr_rec_series"].as<JsonArray>()) {
                            s.hr_rec_series.push_back(val);
                        }
                    }
                    
                    s.rep_velocities.clear();
                    if (sObj["rep_vel"].is<JsonArray>()) {
                        for (float val : sObj["rep_vel"].as<JsonArray>()) {
                            s.rep_velocities.push_back(val);
                        }
                    }
                    ex.sets.push_back(s);
                }
            }
            session.exercises.push_back(ex);
        }
    }
}

void saveCustomExercises() {
    File f = LittleFS.open("/custom_exercises.json", "w");
    if (f) {
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();
        for (const auto &ex : custom_exercises) {
            arr.add(ex);
        }
        serializeJson(doc, f);
        f.close();
    }
}

void loadCustomExercises() {
    custom_exercises.clear();
    if (LittleFS.exists("/custom_exercises.json")) {
        File f = LittleFS.open("/custom_exercises.json", "r");
        if (f) {
            JsonDocument doc;
            if (deserializeJson(doc, f) == DeserializationError::Ok && doc.is<JsonArray>()) {
                for (JsonVariant val : doc.as<JsonArray>()) {
                    custom_exercises.push_back(val.as<String>());
                }
            }
            f.close();
        }
    }
    // If empty, add a default custom exercise as an example
    if (custom_exercises.empty()) {
        custom_exercises.push_back("Custom Press");
        custom_exercises.push_back("Custom Lift");
    }
}

String getExerciseName(int m, int e) {
    if (m == 10) { // Custom muscle group
        if (e >= 0 && e < (int)custom_exercises.size()) {
            return custom_exercises[e];
        }
        return "Custom";
    }
    return String(exercise_db[m][e]);
}

bool determineIfEccentricFirst(String name) {
    String lowerName = name;
    lowerName.toLowerCase();
    
    // Check specific keywords for eccentric-first
    if (lowerName.indexOf("squat") >= 0) return true;
    if (lowerName.indexOf("bench") >= 0) return true; // Bench Press, Incline Bench, Decline Bench, Close Grip BP
    if (lowerName.indexOf("fly") >= 0 && lowerName.indexOf("cable") == -1) return true; // Dumbbell Fly (but not Cable Fly/Cable Cross)
    if (lowerName.indexOf("push-up") >= 0) return true;
    if (lowerName.indexOf("pushup") >= 0) return true;
    if (lowerName.indexOf("leg press") >= 0) return true;
    if (lowerName.indexOf("lunge") >= 0) return true;
    if (lowerName.indexOf("sissy") >= 0) return true;
    if (lowerName.indexOf("wall sit") >= 0) return true;
    
    if (lowerName.indexOf("romanian") >= 0) return true; // Romanian DL, Cable RDL
    if (lowerName.indexOf("rdl") >= 0) return true;
    if (lowerName.indexOf("stiff leg") >= 0) return true; // Stiff Leg DL, Cable SL DL
    if (lowerName.indexOf("good morning") >= 0) return true;
    if (lowerName.indexOf("nordic") >= 0) return true;
    if (lowerName.indexOf("glute-ham") >= 0) return true;
    
    if (lowerName.indexOf("skull") >= 0) return true; // Skull Crush
    if (lowerName.indexOf("dip") >= 0) return true; // Dips
    if (lowerName.indexOf("overhead ext") >= 0) return true; // Overhead Ext, Cable Overhd Ext
    if (lowerName.indexOf("overhd ext") >= 0) return true;
    
    if (lowerName.indexOf("bulgarian") >= 0) return true; // Bulgarian SS
    
    return false;
}

void saveSoundConfig() {
    File f = LittleFS.open("/sound_config.json", "w");
    if (f) {
        JsonDocument doc;
        doc["muted"] = (bool)sound_muted;
        serializeJson(doc, f);
        f.close();
    }
}

uint8_t getExerciseEquip(int m, int e) {
    if (m == 10) {
        return 1; // EQ_DUMBBELL for custom exercises
    }
    return exercise_equip[m][e];
}

void buildExerciseList(int m_idx) {
    current_ex_count = 0;
    if (m_idx == 10) { // Custom group
        current_ex_count = custom_exercises.size();
        for (int i = 0; i < (int)custom_exercises.size() && i < 128; i++) {
            current_ex_list[i] = i;
        }
        return;
    }
    for(int i=0; i<ex_count[m_idx]; i++) {
        if (is_favorite[m_idx][i]) current_ex_list[current_ex_count++] = i;
    }
    for(int i=0; i<ex_count[m_idx]; i++) {
        if (!is_favorite[m_idx][i]) current_ex_list[current_ex_count++] = i;
    }
}

void doSearch() {
    search_count = 0;
    String q = search_query;
    q.toLowerCase();
    for (int m=0; m<NUM_MUSCLES; m++) {
        int count = (m == 10) ? custom_exercises.size() : ex_count[m];
        for (int e=0; e<count; e++) {
            String ex = getExerciseName(m, e);
            if (ex.length() == 0) continue;
            String ext = ex; ext.toLowerCase();
            if (ext.indexOf(q) >= 0) {
                search_results[search_count].m = m;
                search_results[search_count].e = e;
                search_count++;
                if (search_count >= 64) return;
            }
        }
    }
}

void loadDataFiles() {
    if (LittleFS.exists("/weights.json")) {
        File f = LittleFS.open("/weights.json", "r");
        deserializeJson(last_weights_doc, f);
        f.close();
    }
    if (!last_weights_doc.is<JsonObject>()) last_weights_doc.to<JsonObject>();

    if (LittleFS.exists("/prs.json")) {
        File f = LittleFS.open("/prs.json", "r");
        deserializeJson(pr_doc, f);
        f.close();
    }
    if (!pr_doc.is<JsonObject>()) pr_doc.to<JsonObject>();
    
    if (LittleFS.exists("/favs.bin")) {
        File f = LittleFS.open("/favs.bin", "r");
        f.read((uint8_t*)is_favorite, sizeof(is_favorite));
        f.close();
    }

    if (LittleFS.exists("/sound_config.json")) {
        File f = LittleFS.open("/sound_config.json", "r");
        JsonDocument doc;
        if (deserializeJson(doc, f) == DeserializationError::Ok) {
            sound_muted = doc["muted"] | false;
        }
        f.close();
    }

    if (getWorkoutLogFS().exists("/active_session.json")) {
        File f = getWorkoutLogFS().open("/active_session.json", "r");
        if (f) {
            JsonDocument tempDoc;
            if (deserializeJson(tempDoc, f) == DeserializationError::Ok) {
                deserializeSession(tempDoc, active_session);
                today_total_sets = 0;
                today_total_reps = 0;
                today_total_volume = 0;
                today_bad_reps = 0;
                for (const auto &ex : active_session.exercises) {
                    today_total_sets += ex.sets.size();
                    for (const auto &s : ex.sets) {
                        today_total_reps += s.r;
                        today_total_volume += s.v;
                        today_bad_reps += s.pf;
                    }
                }
                
                // Recover active exercise, set number, and estimated 1RM from active_session
                if (!active_session.exercises.empty()) {
                    active_exercise = active_session.exercises.back().name;
                    current_set_number = active_session.exercises.back().sets.size();
                    
                    // Recover estimated 1RM for the active exercise
                    estimated_1rm = 0;
                    for (const auto &s : active_session.exercises.back().sets) {
                        if (s.est_1rm > estimated_1rm) {
                            estimated_1rm = s.est_1rm;
                        }
                    }
                }
            }
            f.close();
        }
    }
}

void saveFavorites() {
    File f = LittleFS.open("/favs.bin", "w");
    f.write((uint8_t*)is_favorite, sizeof(is_favorite));
    f.close();
}

void checkAndUpdatePR(String ex, float e1rm) {
    float current_pr = pr_doc[ex] | 0.0f;
    if (e1rm > current_pr) {
        pr_doc[ex] = e1rm;
        File f = LittleFS.open("/prs.json", "w");
        serializeJson(pr_doc, f);
        f.close();
    }
}

AsyncWebServer server(80);
DNSServer dnsServer;
const byte DNS_PORT = 53;

// --- CUSTOM ROUTINES DEFINITIONS ---
struct RoutineExercise {
    char name[32];
    int target_sets;
    int target_reps;
    int target_rest; // in seconds
    int completed_sets;
};

struct Routine {
    char name[20];
    RoutineExercise exercises[10]; // up to 10 exercises
    int exercise_count;
};

Routine routines[5];
int routines_count = 0;
int selected_routine_idx = 0;

// Routine Execution State
bool routine_active = false;
int active_routine_idx = -1;
int routine_selected_ex_idx = 0;
bool routine_exercising = false;

enum RoutineSubView { ROUTINE_LIST, ROUTINE_CREATOR, ROUTINE_PICK_EXERCISE, ROUTINE_EDIT_ITEM };
RoutineSubView routine_subview = ROUTINE_LIST;

Routine temp_routine;
int creator_selected_idx = 0;
int edit_field_idx = 0; // 0=sets, 1=reps, 2=rest
String edit_input_str = "";
RoutineExercise backup_exercise;

int getRemainingWorkoutTimeSec() {
    if (!routine_active || active_routine_idx < 0) return 0;
    Routine &r = routines[active_routine_idx];
    int N = r.exercise_count;
    int active_ex = routine_selected_ex_idx;
    if (active_ex < 0 || active_ex >= N) return 0;
    
    int rem_sec = 0;
    
    // 1. Current exercise remaining sets
    RoutineExercise &curr_ex = r.exercises[active_ex];
    int done = curr_ex.completed_sets;
    int total = curr_ex.target_sets;
    if (done < total) {
        int rem_sets = total - done;
        rem_sec += rem_sets * curr_ex.target_reps * 4;
        if (rem_sets > 1) {
            rem_sec += (rem_sets - 1) * curr_ex.target_rest;
        }
    }
    
    // 2. Subsequent exercises
    for (int j = active_ex + 1; j < N; j++) {
        RoutineExercise &ex = r.exercises[j];
        rem_sec += ex.target_sets * ex.target_reps * 4;
        if (ex.target_sets > 1) {
            rem_sec += (ex.target_sets - 1) * ex.target_rest;
        }
    }
    
    // 3. Remaining inter-exercise rest transitions (3 mins / 180s each)
    int rem_transitions = N - 1 - active_ex;
    if (rem_transitions > 0) {
        rem_sec += rem_transitions * 180;
    }
    
    return rem_sec;
}

int getRoutineEstimatedTimeSec(Routine &r) {
    int N = r.exercise_count;
    if (N == 0) return 0;
    int total_sec = 0;
    for (int i = 0; i < N; i++) {
        RoutineExercise &ex = r.exercises[i];
        total_sec += ex.target_sets * ex.target_reps * 4;
        if (ex.target_sets > 1) {
            total_sec += (ex.target_sets - 1) * ex.target_rest;
        }
    }
    if (N > 1) {
        total_sec += (N - 1) * 180;
    }
    return total_sec;
}

String formatDuration(int total_sec) {
    int h = total_sec / 3600;
    int m = (total_sec % 3600) / 60;
    char buf[16];
    if (h > 0) {
        snprintf(buf, sizeof(buf), "%dh%dm", h, m);
    } else {
        snprintf(buf, sizeof(buf), "%dm", m);
    }
    return String(buf);
}

String formatRestTimerInput(String str) {
    if (str.length() == 0) return "0:00";
    if (str.length() == 1) {
        return "0:0" + str;
    }
    if (str.length() == 2) {
        int val = str.toInt();
        int m = val / 60;
        int s = val % 60;
        char buf[8];
        snprintf(buf, sizeof(buf), "%d:%02d", m, s);
        return String(buf);
    }
    int len = str.length();
    String secStr = str.substring(len - 2);
    String minStr = str.substring(0, len - 2);
    int m = minStr.toInt();
    int s = secStr.toInt();
    char buf[16];
    snprintf(buf, sizeof(buf), "%d:%02d", m, s);
    return String(buf);
}

int parseRestTimerInput(String str) {
    if (str.length() == 0) return 0;
    if (str.length() <= 2) {
        return str.toInt();
    }
    int len = str.length();
    String secStr = str.substring(len - 2);
    String minStr = str.substring(0, len - 2);
    return minStr.toInt() * 60 + secStr.toInt();
}

String secondsToTimerInput(int total_sec) {
    int m = total_sec / 60;
    int s = total_sec % 60;
    if (m == 0) {
        return String(s);
    }
    char buf[16];
    snprintf(buf, sizeof(buf), "%d%02d", m, s);
    return String(buf);
}

// Forward declarations/helpers for routines
void saveRoutines() {
    JsonDocument doc;
    JsonArray routs = doc.to<JsonArray>();
    for (int i = 0; i < routines_count; i++) {
        JsonObject r = routs.add<JsonObject>();
        r["name"] = routines[i].name;
        JsonArray exs = r["exercises"].to<JsonArray>();
        for (int j = 0; j < routines[i].exercise_count; j++) {
            JsonObject ex = exs.add<JsonObject>();
            ex["name"] = routines[i].exercises[j].name;
            ex["sets"] = routines[i].exercises[j].target_sets;
            ex["reps"] = routines[i].exercises[j].target_reps;
            ex["rest"] = routines[i].exercises[j].target_rest;
        }
    }
    File f = getWorkoutLogFS().open("/routines.json", "w");
    serializeJson(doc, f);
    f.close();
}

void loadRoutines() {
    routines_count = 0;
    fs::FS& fs = getWorkoutLogFS();
    if (!fs.exists("/routines.json")) {
        // Seed with a default demo routine so the user has something immediately!
        routines_count = 1;
        strcpy(routines[0].name, "Full Body A");
        routines[0].exercise_count = 3;
        
        strcpy(routines[0].exercises[0].name, "Bench Press");
        routines[0].exercises[0].target_sets = 4;
        routines[0].exercises[0].target_reps = 10;
        routines[0].exercises[0].target_rest = 90;
        routines[0].exercises[0].completed_sets = 0;

        strcpy(routines[0].exercises[1].name, "Squat");
        routines[0].exercises[1].target_sets = 4;
        routines[0].exercises[1].target_reps = 8;
        routines[0].exercises[1].target_rest = 120;
        routines[0].exercises[1].completed_sets = 0;

        strcpy(routines[0].exercises[2].name, "Lat Pulldown");
        routines[0].exercises[2].target_sets = 3;
        routines[0].exercises[2].target_reps = 12;
        routines[0].exercises[2].target_rest = 90;
        routines[0].exercises[2].completed_sets = 0;
        
        saveRoutines();
        return;
    }
    File f = fs.open("/routines.json", "r");
    JsonDocument doc;
    if (deserializeJson(doc, f) == DeserializationError::Ok) {
        JsonArray routs = doc.as<JsonArray>();
        routines_count = min(5, (int)routs.size());
        for (int i = 0; i < routines_count; i++) {
            JsonObject r = routs[i].as<JsonObject>();
            strncpy(routines[i].name, r["name"] | "Routine", 19);
            JsonArray exs = r["exercises"].as<JsonArray>();
            routines[i].exercise_count = min(10, (int)exs.size());
            for (int j = 0; j < routines[i].exercise_count; j++) {
                JsonObject ex = exs[j].as<JsonObject>();
                strncpy(routines[i].exercises[j].name, ex["name"] | "Bench Press", 31);
                routines[i].exercises[j].target_sets = ex["sets"] | 3;
                routines[i].exercises[j].target_reps = ex["reps"] | 10;
                routines[i].exercises[j].target_rest = ex["rest"] | 90;
                routines[i].exercises[j].completed_sets = 0;
            }
        }
    }
    f.close();
}

// --- DISPLAY STYLING ---
M5Canvas canvas(&M5Cardputer.Display);

enum AppView { VIEW_WORKOUT, VIEW_STATS, VIEW_EXERCISES, VIEW_HISTORY, VIEW_PR, VIEW_ROUTINES, VIEW_CARDIO, VIEW_HELP, VIEW_HISTORY_DETAIL };
AppView current_view = VIEW_WORKOUT;
int help_selected_idx = 0;
int help_scroll_offset = 0;

// History browsing
SessionLog history_sessions[10];
int history_count = 0;
int history_scroll = 0;
int history_selected_idx = 0;
int detail_selected_ex_idx = 0;
bool history_confirm_delete = false;
bool routine_confirm_delete = false;

// LED NeoPixel globals
Adafruit_NeoPixel ledPixel(1, 21, NEO_GRB + NEO_KHZ800);
unsigned long led_flash_end_time = 0;
bool led_flashing = false;

// Stats Volume Graph globals
bool stats_show_graph = false;
int stats_volumes[7] = {0};
String stats_labels[7];
int stats_sets_count = 0;

// Menu state
int menu_muscle_idx = 0;
int menu_exercise_idx = 0;
int menu_scroll_offset = 0; // for scrolling long lists
bool selecting_muscle = true;

// Relocated to top

// daily metrics declared at the top of the file

// --- iOS-STYLE COLOR PALETTE ---
uint16_t C_BG       = M5Cardputer.Display.color565(0, 0, 0);
uint16_t C_CARD     = M5Cardputer.Display.color565(28, 28, 30);
uint16_t C_CARD_HI  = M5Cardputer.Display.color565(44, 44, 46);
uint16_t C_ACCENT   = M5Cardputer.Display.color565(10, 132, 255);
uint16_t C_IGREEN   = M5Cardputer.Display.color565(48, 209, 88);
uint16_t C_IORANGE  = M5Cardputer.Display.color565(255, 159, 10);
uint16_t C_IRED     = M5Cardputer.Display.color565(255, 69, 58);
uint16_t C_IYELLOW  = M5Cardputer.Display.color565(255, 214, 10);
uint16_t C_IGRAY    = M5Cardputer.Display.color565(142, 142, 147);
uint16_t C_LABEL    = M5Cardputer.Display.color565(174, 174, 178);
uint16_t C_TAB_BG   = M5Cardputer.Display.color565(18, 18, 18);

// Smooth scroll animation
float scroll_anim_offset = 0.0f;
int scroll_anim_target = 0;

// Daily goals for fitness rings
const int GOAL_VOLUME = 5000;  // kg
const int GOAL_SETS   = 20;

// Helper: draw a thick arc (ring segment) for fitness rings
void drawRing(M5Canvas &c, int cx, int cy, int r_out, int r_in, int startAngle, int endAngle, uint16_t col) {
    c.fillArc(cx, cy, r_out, r_in, startAngle, endAngle, col);
}

// Helper: draw a thick progress ring with rounded ends (caps)
void drawProgressRing(M5Canvas &c, int cx, int cy, int r_out, int r_in, int pct, uint16_t col) {
    if (pct <= 0) return;
    int endA = 270 + (pct * 360) / 100;
    
    // Draw main thick arc
    if (endA > 360) {
        c.fillArc(cx, cy, r_out, r_in, 270, 360, col);
        c.fillArc(cx, cy, r_out, r_in, 0, endA - 360, col);
    } else {
        c.fillArc(cx, cy, r_out, r_in, 270, endA, col);
    }
    
    // Rounded ends (caps)
    float r_mid = (r_out + r_in) / 2.0f;
    float cap_r = (r_out - r_in) / 2.0f;
    
    // Start cap (at 270 degrees - straight up)
    float sx = cx;
    float sy = cy - r_mid;
    c.fillCircle((int)sx, (int)sy, (int)cap_r, col);
    
    // End cap (at endA degrees)
    float rad = (float)endA * 3.14159265f / 180.0f;
    float ex = (float)cx + r_mid * cos(rad);
    float ey = (float)cy + r_mid * sin(rad);
    c.fillCircle((int)ex, (int)ey, (int)cap_r, col);
}

void drawUI() {
    // --- Wi-Fi QR Code Overlay ---
    if (show_wifi_qr) {
        canvas.fillSprite(C_BG);
        
        // Title
        canvas.setTextColor(C_ACCENT); canvas.setTextSize(1.2);
        canvas.drawString("Scan to connect Wi-Fi", 10, 4);
        canvas.drawLine(0, 16, 240, 16, C_CARD);
        
        // Draw QR code using M5GFX built-in method
        String qrContent = "WIFI:T:WPA;S:" + String(AP_SSID) + ";P:" + String(AP_PASS) + ";;";
        // Maximize QR code size (screen height is 135)
        int qr_size = 125;
        int qr_x = 5;
        int qr_y = 5;
        canvas.qrcode(qrContent.c_str(), qr_x, qr_y, qr_size, 2);
        
        // Network info to the right of QR code
        int text_x = 135;
        canvas.setTextColor(C_ACCENT); canvas.setTextSize(1);
        canvas.drawString("GymTracker", text_x, 15);
        canvas.drawString("Wi-Fi", text_x, 30);
        
        canvas.setTextColor(C_IORANGE);
        canvas.drawString("SSID:", text_x, 55);
        canvas.setTextColor(WHITE);
        canvas.drawString(String(AP_SSID), text_x, 70);
        
        canvas.setTextColor(C_IGRAY);
        canvas.drawString("IP:", text_x, 95);
        canvas.setTextColor(WHITE);
        canvas.drawString("192.168.4.1", text_x, 110);
        
        // Dismiss hint
        canvas.setTextColor(C_IGRAY); canvas.setTextSize(1);
        canvas.drawString("Press any key to close", 10, 133);
        
        canvas.pushSprite(0, 0);
        return;
    }
    
    // If not in rest mode or not in workout view, ensure display is at full brightness
    if (workout_state != STATE_SUMMARY || current_view != VIEW_WORKOUT) {
        if (current_backlight_brightness != 160) {
            current_backlight_brightness = 160;
            M5Cardputer.Display.setBrightness(current_backlight_brightness);
        }
    }
    if (workout_state == STATE_ACTIVE && current_view == VIEW_WORKOUT) {
        canvas.fillSprite(bg_color);
    } else {
        canvas.fillSprite(C_BG);
    }
    
    // ====== BOTTOM TAB BAR ======
    canvas.fillRect(0, 118, 240, 17, C_TAB_BG);
    canvas.drawLine(0, 118, 240, 118, C_CARD);
    const char* tabs[] = {"Work", "Stats", "Exerc", "Log", "PRs", "Routs", "Card"};
    for (int t = 0; t < 7; t++) {
        int tx = (t * 240) / 7;
        int next_tx = ((t + 1) * 240) / 7;
        int tab_width = next_tx - tx;
        int cx = tx + tab_width / 2;
        
        bool act = (t == (int)current_view);
        canvas.setTextSize(1);
        canvas.setTextColor(act ? C_ACCENT : C_IGRAY);
        if (act) canvas.fillCircle(cx, 121, 2, C_ACCENT);
        
        int tw = canvas.textWidth(tabs[t]);
        canvas.drawString(tabs[t], cx - tw / 2, 126);
    }
    
    // ====== TOP HEADER BAR ======
    canvas.fillRect(0, 0, 240, 14, C_TAB_BG);
    
    if (routine_active && active_routine_idx >= 0 && active_routine_idx < routines_count) {
        int total_sets = 0;
        int completed_sets = 0;
        Routine &r = routines[active_routine_idx];
        for (int k = 0; k < r.exercise_count; k++) {
            total_sets += r.exercises[k].target_sets;
            completed_sets += min(r.exercises[k].completed_sets, r.exercises[k].target_sets);
        }
        
        int pct = total_sets > 0 ? (completed_sets * 100) / total_sets : 0;
        pct = constrain(pct, 0, 100);
        int rem_sec = getRemainingWorkoutTimeSec();
        
        char statusBuf[24];
        if (rem_sec >= 3600) {
            int h = rem_sec / 3600;
            int m = (rem_sec % 3600) / 60;
            snprintf(statusBuf, sizeof(statusBuf), "%d%% %dh%dm", pct, h, m);
        } else {
            snprintf(statusBuf, sizeof(statusBuf), "%d%% %dm", pct, rem_sec / 60);
        }
        
        canvas.setTextColor(C_IORANGE); canvas.setTextSize(1);
        canvas.drawString(String(statusBuf), 45, 4);
        
        int bar_w = (pct * 240) / 100;
        canvas.fillRect(0, 13, 240, 2, M5Cardputer.Display.color565(30, 30, 30));
        if (bar_w > 0) {
            canvas.fillRect(0, 13, bar_w, 2, C_ACCENT);
        }
        canvas.drawLine(0, 14, 240, 14, C_CARD);
    } else {
        canvas.drawLine(0, 14, 240, 14, C_CARD);
    }
    
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm *tm_info = localtime(&tv.tv_sec);
    if (tm_info->tm_year > 120) {
        char tStr[10];
        sprintf(tStr, "%02d:%02d", tm_info->tm_hour, tm_info->tm_min);
        canvas.setTextColor(WHITE); canvas.setTextSize(1);
        canvas.drawString(tStr, 5, 4);
    } else {
        canvas.setTextColor(C_IGRAY); canvas.setTextSize(1);
        unsigned long sess_s = (millis() - session_start_time) / 1000;
        char tBuf[10];
        sprintf(tBuf, "%lu:%02lu", sess_s / 60, sess_s % 60);
        canvas.drawString(tBuf, 5, 4);
    }
    
    int wx = 158, wy = 12;
    if (wifi_enabled) {
        canvas.drawArc(wx, wy, 8, 7, 210, 330, C_ACCENT);
        canvas.drawArc(wx, wy, 5, 4, 210, 330, C_ACCENT);
        canvas.fillCircle(wx, wy-1, 1.5, C_ACCENT);
    }
    
    // Draw sound status icon (next to Wi-Fi, at x=171, y=4)
    int sx = 171, sy = 4;
    if (sound_muted) {
        canvas.fillRect(sx, sy + 2, 2, 3, C_IRED);
        canvas.fillTriangle(sx + 2, sy + 2, sx + 5, sy - 1, sx + 5, sy + 7, C_IRED);
        canvas.drawLine(sx + 7, sy + 1, sx + 9, sy + 5, C_IRED);
        canvas.drawLine(sx + 9, sy + 1, sx + 7, sy + 5, C_IRED);
    } else {
        canvas.fillRect(sx, sy + 2, 2, 3, C_IGREEN);
        canvas.fillTriangle(sx + 2, sy + 2, sx + 5, sy - 1, sx + 5, sy + 7, C_IGREEN);
        canvas.drawArc(sx + 4, sy + 3, 4, 3, -45, 45, C_IGREEN);
    }
    
    int batt = M5.Power.getBatteryLevel();
    batt = constrain(batt, 0, 100);
    uint16_t battCol = batt > 50 ? C_IGREEN : (batt > 20 ? C_IYELLOW : C_IRED);
    int bx = 210, by = 2;
    canvas.drawRoundRect(bx, by, 22, 10, 2, C_IGRAY);
    canvas.fillRect(bx + 22, by + 3, 2, 4, C_IGRAY);
    int fillW = max(1, (batt * 18) / 100);
    canvas.fillRoundRect(bx + 2, by + 2, fillW, 6, 1, battCol);
    
    canvas.setTextColor(WHITE); canvas.setTextSize(1);
    canvas.drawString(String(batt) + "%", bx - 28, 4);
    
    // ====== TOP HEADER HEART RATE INDICATOR ======
    if (ble_enabled) {
        bool hr_stale = (millis() - last_heart_rate_time) > 6000;
        if (connected && current_heart_rate > 0 && !hr_stale) {
            // Heart rate pulses at the exact speed of the user's current heart rate!
            int period = 60000 / current_heart_rate;
            bool pulse_on = (millis() % period) < (period / 3);
            uint16_t heartCol = pulse_on ? C_IRED : M5Cardputer.Display.color565(120, 20, 20);
            
            // Draw pixel-art heart centered at x=98, y=6
            canvas.fillCircle(98 - 2, 6, 2, heartCol);
            canvas.fillCircle(98 + 2, 6, 2, heartCol);
            canvas.fillTriangle(98 - 4, 7, 98 + 4, 7, 98, 11, heartCol);
            
            canvas.setTextColor(WHITE); canvas.setTextSize(1);
            canvas.drawString(String(current_heart_rate) + " bpm", 106, 4);
        } else {
            // Searching/Scanning - blink a gray heart slowly (period 2s)
            bool blink = (millis() % 2000) < 1000;
            uint16_t heartCol = blink ? C_IGRAY : M5Cardputer.Display.color565(50, 50, 50);
            
            canvas.fillCircle(98 - 2, 6, 2, heartCol);
            canvas.fillCircle(98 + 2, 6, 2, heartCol);
            canvas.fillTriangle(98 - 4, 7, 98 + 4, 7, 98, 11, heartCol);
            
            canvas.setTextColor(C_IGRAY); canvas.setTextSize(1);
            canvas.drawString("-- bpm", 106, 4);
        }
    }
    
    if (current_view == VIEW_WORKOUT) {
        if (routine_active && !routine_exercising && workout_state == STATE_READY) {
            canvas.setTextColor(C_ACCENT); canvas.setTextSize(1.5);
            canvas.drawString("Routine: " + String(routines[active_routine_idx].name), 10, 18);
            
            Routine &r = routines[active_routine_idx];
            const int VIS = 4, RH = 18;
            int start = max(0, routine_selected_ex_idx - 2);
            for (int vi = 0; vi < min(VIS, r.exercise_count); vi++) {
                int idx = start + vi;
                if (idx >= r.exercise_count) break;
                int y = 34 + (vi * (RH + 2));
                bool sel = (idx == routine_selected_ex_idx);
                
                canvas.fillRoundRect(5, y, 230, RH, 4, sel ? C_CARD_HI : C_CARD);
                if (sel) canvas.drawRoundRect(5, y, 230, RH, 4, C_ACCENT);
                
                bool comp = (r.exercises[idx].completed_sets >= r.exercises[idx].target_sets);
                if (comp) {
                    canvas.fillCircle(15, y + 9, 5, C_IGREEN);
                    canvas.setTextColor(BLACK); canvas.setTextSize(1);
                    canvas.drawString("v", 12, y + 5);
                } else {
                    canvas.drawCircle(15, y + 9, 5, C_IGRAY);
                }
                
                canvas.setTextColor(WHITE); canvas.setTextSize(1);
                String exName = r.exercises[idx].name;
                if (exName.length() > 16) exName = exName.substring(0, 14) + "..";
                canvas.drawString(exName, 28, y + 5);
                
                canvas.setTextColor(C_IORANGE);
                canvas.drawString(String(r.exercises[idx].completed_sets) + "/" + String(r.exercises[idx].target_sets) + " Sets", 175, y + 5);
            }
            
            canvas.setTextColor(C_IGRAY); canvas.setTextSize(1);
            canvas.drawString("UP/DOWN:Select  ENT:Start  Q:Finish", 10, 110);
            
            canvas.pushSprite(0, 0);
            return;
        }

        // Draw Watermark
        if (workout_state != STATE_SUMMARY) {
            uint16_t mc2[10] = {C_IRED, C_ACCENT, C_IGREEN, M5Cardputer.Display.color565(0,200,200), C_IORANGE,
                                M5Cardputer.Display.color565(200,0,200), C_ACCENT, M5Cardputer.Display.color565(140,60,200), C_IYELLOW, C_IGRAY};
            drawMuscleIcon(canvas, 140, 50, menu_muscle_idx, C_CARD); 
        }

        switch (workout_state) {
            case STATE_READY: {
                canvas.setTextColor(WHITE);
                canvas.setTextSize(1.5);
                canvas.drawString(active_exercise, 10, 18);
                
                canvas.setTextColor(C_IGRAY);
                canvas.setTextSize(1);
                canvas.drawString("Set " + String(current_set_number + 1), 10, 36);
                
                canvas.fillRoundRect(50, 34, 40, 14, 4, C_CARD);
                canvas.setTextColor(C_IORANGE);
                canvas.drawString(String(rest_target_sec) + "s", 58, 37);
                
                canvas.fillRoundRect(10, 52, 220, 36, 8, C_CARD);
                canvas.setTextColor(C_LABEL);
                canvas.setTextSize(1);
                canvas.drawString("WEIGHT", 18, 55);
                canvas.setTextColor(WHITE);
                canvas.setTextSize(2.5);
                if (editing_weight) {
                    canvas.drawString(weight_input_str + "_", 18, 66);
                } else {
                    canvas.drawString(String(current_weight), 18, 66);
                    canvas.setTextColor(C_IGRAY);
                    canvas.setTextSize(1.5);
                    canvas.drawString("kg", 90, 68);
                    
                    canvas.fillRoundRect(150, 56, 32, 26, 6, C_CARD_HI);
                    canvas.fillRoundRect(190, 56, 32, 26, 6, C_CARD_HI);
                    canvas.setTextColor(C_ACCENT);
                    canvas.setTextSize(2);
                    canvas.drawString("-", 160, 58);
                    canvas.drawString("+", 200, 58);
                }
                
                if (estimated_1rm > 0) {
                    canvas.fillRoundRect(10, 92, 80, 18, 6, C_CARD);
                    canvas.setTextColor(C_LABEL); canvas.setTextSize(1);
                    canvas.drawString("1RM", 16, 96);
                    canvas.setTextColor(C_IORANGE);
                    canvas.drawString(String((int)estimated_1rm) + "kg", 44, 96);
                }
                
                if (editing_weight) {
                    canvas.setTextColor(C_IGRAY);
                    canvas.setTextSize(1);
                    canvas.drawString("Type weight + ENTER", 100, 96);
                } else {
                    canvas.fillRoundRect(100, 92, 130, 18, 6, C_ACCENT);
                    canvas.setTextColor(WHITE);
                    canvas.setTextSize(1);
                    canvas.drawString("ENTER: Start", 125, 96);
                }
                break;
            }
            case STATE_ACTIVE: {
                canvas.setTextColor(C_LABEL);
                canvas.setTextSize(1);
                canvas.drawString(active_exercise, 10, 18);
                canvas.setTextColor(C_ACCENT);
                canvas.drawString(String(current_weight) + "kg | Set " + String(current_set_number + 1), 10, 29);
                
                if (ble_enabled) {
                    bool hr_stale = (millis() - last_heart_rate_time) > 6000;
                    if (connected && current_heart_rate > 0 && !hr_stale) {
                        int period = 60000 / current_heart_rate;
                        bool pulse_on = (millis() % period) < (period / 3);
                        uint16_t hrCol = current_heart_rate > 156 ? C_IRED : (current_heart_rate > 117 ? C_IYELLOW : C_IGREEN);
                        uint16_t heartCol = pulse_on ? hrCol : M5Cardputer.Display.color565(50, 50, 50);
                        
                        int hx = 180, hy = 22;
                        canvas.fillCircle(hx - 2, hy, 2, heartCol);
                        canvas.fillCircle(hx + 2, hy, 2, heartCol);
                        canvas.fillTriangle(hx - 4, hy + 1, hx + 4, hy + 1, hx, hy + 5, heartCol);
                        
                        canvas.setTextColor(hrCol); canvas.setTextSize(1.5);
                        canvas.drawString(String(current_heart_rate), hx + 10, hy - 2);
                    }
                }
                
                canvas.setTextColor(WHITE);
                canvas.setTextSize(7);
                String repStr;
                if (editing_reps) {
                    repStr = reps_input_str + "_";
                } else {
                    repStr = String((int)current_reps);
                }
                int rw = repStr.length() * 42;
                canvas.drawString(repStr, 120 - rw / 2, 41);
                
                canvas.setTextColor(C_LABEL);
                canvas.setTextSize(1);
                if (editing_reps) {
                    canvas.drawString("MANUAL REPS", 85, 101);
                } else {
                    canvas.drawString("REPS", 108, 101);
                }
                
                if (rep_phase != PHASE_REST && !editing_reps) {
                    canvas.fillRoundRect(80, 101, 80, 16, 8, C_ACCENT);
                    canvas.setTextColor(WHITE);
                    canvas.drawString("MOVING", 100, 105);
                } else if (editing_reps) {
                    canvas.fillRoundRect(50, 101, 140, 16, 8, C_IORANGE);
                    canvas.setTextColor(WHITE);
                    canvas.drawString("ENTER TO SAVE", 80, 105);
                } else {
                    canvas.fillRoundRect(80, 101, 80, 16, 8, C_CARD);
                    canvas.setTextColor(C_IGRAY);
                    canvas.drawString("READY", 104, 105);
                }
                break;
            }
            case STATE_SUMMARY: {
                static bool rest_beeped = false;
                unsigned long rest_elapsed = millis() - rest_start_time;
                int rest_sec = rest_elapsed / 1000;
                int remaining = rest_target_sec - rest_sec;
                bool done = remaining <= 0;
                
                bool is_eco = (rest_sec >= 10 && remaining > 5 && !done && (millis() - last_key_activity_time > 5000));
                
                if (is_eco) {
                    canvas.fillSprite(BLACK);
                    
                    canvas.setTextColor(C_IORANGE);
                    canvas.setTextSize(6.5);
                    char buf[8];
                    sprintf(buf, "%d:%02d", remaining / 60, remaining % 60);
                    String timerStr(buf);
                    int tw = timerStr.length() * 36;
                    canvas.drawString(timerStr, 120 - tw / 2, 45);
                    
                    int batt = M5.Power.getBatteryLevel();
                    batt = constrain(batt, 0, 100);
                    uint16_t battCol = batt > 50 ? C_IGREEN : (batt > 20 ? C_IYELLOW : C_IRED);
                    int bx = 210, by = 4;
                    canvas.drawRoundRect(bx, by, 22, 10, 2, C_IGRAY);
                    canvas.fillRect(bx + 22, by + 3, 2, 4, C_IGRAY);
                    int fillW = max(1, (batt * 18) / 100);
                    canvas.fillRoundRect(bx + 2, by + 2, fillW, 6, 1, battCol);
                    
                    canvas.setTextColor(C_IGRAY); canvas.setTextSize(1);
                    canvas.drawString("ECO MODE", 10, 5);
                    
                    if (current_backlight_brightness != 40) {
                        current_backlight_brightness = 40;
                        M5Cardputer.Display.setBrightness(current_backlight_brightness);
                    }
                } else {
                    canvas.setTextColor(C_IGREEN);
                    canvas.setTextSize(1.5);
                    canvas.drawString("Set Complete", 10, 18);
                    
                    int bw = 55;
                    for(int i=0; i<4; i++) {
                        canvas.fillRoundRect(4 + i*58, 37, bw, 42, 6, C_CARD);
                    }
                    
                    canvas.setTextColor(C_LABEL);
                    canvas.setTextSize(1);
                    canvas.drawString("REPS", 18, 41);
                    canvas.drawString("FORM", 76, 41);
                    canvas.drawString("VOL", 137, 41);
                    canvas.drawString("VBT", 195, 41);
                    
                    canvas.setTextSize(1.5);
                    canvas.setTextColor(WHITE);
                    canvas.drawString(String((int)current_reps), 22, 57);
                    
                    int bad = (int)current_poor_form_reps;
                    canvas.setTextColor(bad == 0 ? C_IGREEN : (bad < current_reps ? C_IYELLOW : C_IRED));
                    canvas.drawString(bad == 0 ? "OK" : String(bad) + "x", 78, 57);
                    
                    canvas.setTextColor(C_ACCENT);
                    canvas.drawString(String((int)(current_weight * current_reps)), 126, 57);
                    
                    canvas.setTextColor(C_IORANGE);
                    canvas.drawString(String(last_rep_peak_vel, 1), 185, 57);
                    
                    canvas.fillRoundRect(20, 85, 200, 32, 8, C_CARD);
                    
                    canvas.setTextColor(done ? C_IGREEN : C_IORANGE);
                    canvas.setTextSize(2.5);
                    char buf[8];
                    if (done) {
                        sprintf(buf, "+%d", -remaining);
                    } else {
                        sprintf(buf, "%d:%02d", remaining / 60, remaining % 60);
                    }
                    canvas.drawString(buf, 90, 89);
                    
                    if (ble_enabled) {
                        bool hr_stale = (millis() - last_heart_rate_time) > 6000;
                        if (connected && current_heart_rate > 0 && !hr_stale) {
                            int period = 60000 / current_heart_rate;
                            bool pulse_on = (millis() % period) < (period / 3);
                            uint16_t zoneCol = current_heart_rate > 156 ? C_IRED : (current_heart_rate > 117 ? C_IYELLOW : C_IGREEN);
                            uint16_t heartCol = pulse_on ? zoneCol : M5Cardputer.Display.color565(50, 50, 50);
                            
                            canvas.fillCircle(168 - 2, 98, 2, heartCol);
                            canvas.fillCircle(168 + 2, 98, 2, heartCol);
                            canvas.fillTriangle(168 - 4, 99, 168 + 4, 99, 168, 103, heartCol);
                            
                            canvas.setTextColor(zoneCol); canvas.setTextSize(1.5);
                            canvas.drawString(String(current_heart_rate), 178, 93);
                        }
                    }
                    
                    int prog = min(190, (int)((rest_sec * 190L) / rest_target_sec));
                    canvas.fillRoundRect(25, 113, max(3, prog), 2, 1, done ? C_IGREEN : C_ACCENT);
                    
                    if (current_backlight_brightness != 160) {
                        current_backlight_brightness = 160;
                        M5Cardputer.Display.setBrightness(current_backlight_brightness);
                    }
                }
                
                if (rest_sec == rest_target_sec && !rest_beeped) {
                    if (!sound_muted) {
                        M5Cardputer.Speaker.tone(4000, 300);
                    }
                    rest_beeped = true;
                } else if (rest_sec != rest_target_sec) {
                    rest_beeped = false;
                }
                break;
            }
        }
    } else if (current_view == VIEW_STATS) {
        if (stats_show_graph) {
            canvas.setTextColor(C_ACCENT); canvas.setTextSize(1.5);
            canvas.drawString("Progressione Volume", 10, 18);
            
            if (stats_sets_count == 0) {
                canvas.fillRoundRect(20, 45, 200, 50, 10, C_CARD);
                canvas.setTextColor(C_IGRAY); canvas.setTextSize(1);
                canvas.drawString("Nessun set salvato", 55, 65);
            } else {
                int maxV = 1;
                for (int i = 0; i < stats_sets_count; i++) {
                    if (stats_volumes[i] > maxV) maxV = stats_volumes[i];
                }
                
                int bar_w = 20;
                int gap = 11;
                int start_x = 18;
                
                for (int i = 0; i < stats_sets_count; i++) {
                    int bx = start_x + i * (bar_w + gap);
                    int bar_h = (stats_volumes[i] * 50) / maxV;
                    bar_h = max(2, bar_h); // at least 2 pixels high
                    int by = 95 - bar_h;
                    
                    // Draw nice rounded bar
                    canvas.fillRoundRect(bx, by, bar_w, bar_h, 4, C_ACCENT);
                    
                    // Value above the bar
                    canvas.setTextColor(WHITE); canvas.setTextSize(1);
                    canvas.drawString(String(stats_volumes[i]), bx - (String(stats_volumes[i]).length() == 4 ? 4 : 2), by - 10);
                    
                    // Label below the bar
                    canvas.setTextColor(C_LABEL);
                    canvas.drawString(stats_labels[i], bx + 1, 98);
                }
                
                // Draw a simple X axis line
                canvas.drawLine(10, 96, 230, 96, C_CARD);
            }
            
            // View toggle instructions at bottom
            canvas.fillRoundRect(5, 107, 230, 10, 4, C_CARD);
            canvas.setTextColor(C_IGRAY); canvas.setTextSize(1);
            canvas.drawString("ENT: Torna a Anelli", 10, 108);
            
        } else {
            // ====== APPLE FITNESS RINGS ======
            int ring_cx = 60, ring_cy = 68;
            
            // Calculate percentages (capped at 100%)
            int vol_pct = min(100, (today_total_volume * 100) / max(1, GOAL_VOLUME));
            int set_pct = min(100, (today_total_sets * 100) / max(1, GOAL_SETS));
            int good_reps = today_total_reps - today_bad_reps;
            int quality = today_total_reps > 0 ? (good_reps * 100) / today_total_reps : 0;
            
            // Background rings (dark tracks)
            uint16_t C_RING_BG = M5Cardputer.Display.color565(40, 40, 44);
            drawRing(canvas, ring_cx, ring_cy, 42, 36, 0, 360, C_RING_BG);  // Volume (outer)
            drawRing(canvas, ring_cx, ring_cy, 34, 28, 0, 360, C_RING_BG);  // Sets (middle)
            drawRing(canvas, ring_cx, ring_cy, 26, 20, 0, 360, C_RING_BG);  // Form (inner)
            
            // Active rings (start from top = 270 degrees) with rounded caps
            drawProgressRing(canvas, ring_cx, ring_cy, 42, 36, vol_pct, C_ACCENT);
            drawProgressRing(canvas, ring_cx, ring_cy, 34, 28, set_pct, C_IGREEN);
            drawProgressRing(canvas, ring_cx, ring_cy, 26, 20, quality, C_IRED);
            
            // Right side: Stat labels + values
            int rx = 120;
            
            // Volume
            canvas.fillCircle(rx, 28, 4, C_ACCENT);
            canvas.setTextColor(C_LABEL); canvas.setTextSize(1);
            canvas.drawString("VOLUME", rx + 10, 25);
            canvas.setTextColor(WHITE); canvas.setTextSize(1.5);
            canvas.drawString(String(today_total_volume) + "kg", rx + 10, 36);
            canvas.setTextColor(C_IGRAY); canvas.setTextSize(1);
            canvas.drawString("/" + String(GOAL_VOLUME), rx + 70, 39);
            
            // Sets
            canvas.fillCircle(rx, 56, 4, C_IGREEN);
            canvas.setTextColor(C_LABEL); canvas.setTextSize(1);
            canvas.drawString("SETS", rx + 10, 53);
            canvas.setTextColor(WHITE); canvas.setTextSize(1.5);
            canvas.drawString(String(today_total_sets), rx + 10, 64);
            canvas.setTextColor(C_IGRAY); canvas.setTextSize(1);
            canvas.drawString("/" + String(GOAL_SETS), rx + 40, 67);
            
            // Form
            canvas.fillCircle(rx, 84, 4, C_IRED);
            canvas.setTextColor(C_LABEL); canvas.setTextSize(1);
            canvas.drawString("FORM", rx + 10, 81);
            canvas.setTextColor(quality > 80 ? C_IGREEN : (quality > 50 ? C_IYELLOW : C_IRED));
            canvas.setTextSize(1.5);
            canvas.drawString(String(quality) + "%", rx + 10, 92);
            
            // View toggle instructions at bottom
            canvas.fillRoundRect(5, 107, 230, 10, 4, C_CARD);
            canvas.setTextColor(C_IGRAY); canvas.setTextSize(1);
            canvas.drawString("ENT: Vedi Istogramma Volume", 10, 108);
        }
        
    } else if (current_view == VIEW_CARDIO) {
        canvas.setTextColor(WHITE); canvas.setTextSize(1.5);
        canvas.drawString("Cardio Analytics", 10, 18);
        
        // Show status (BLE and Wi-Fi)
        canvas.setTextColor(C_LABEL); canvas.setTextSize(1);
        String statusStr = "";
        if (connected) statusStr += "BLE: OK";
        else statusStr += "BLE: Scanning";
        if (wifi_enabled) {
            if (WiFi.status() == WL_CONNECTED) {
                statusStr += " | IP: " + WiFi.localIP().toString();
            } else {
                statusStr += " | AP: GymTracker";
            }
        }
        canvas.drawString(statusStr, 10, 32);

        // Stats cards (3 columns)
        int bw = 74;
        canvas.fillRoundRect(5, 43, bw, 27, 4, C_CARD);
        canvas.fillRoundRect(83, 43, bw, 27, 4, C_CARD);
        canvas.fillRoundRect(161, 43, bw, 27, 4, C_CARD);
        
        canvas.setTextColor(C_LABEL); canvas.setTextSize(1);
        canvas.drawString("AVG", 10, 46);
        canvas.drawString("PEAK", 88, 46);
        canvas.drawString("CURRENT", 166, 46);
        
        // Calculate average and peak
        int hr_avg = 0;
        int hr_max = 0;
        if (show_live_curve) {
            int sum = 0, count = 0;
            for (int k = 0; k < 60; k++) {
                int val = hr_live_history[k];
                if (val > 0) {
                    sum += val;
                    count++;
                    if (val > hr_max) hr_max = val;
                }
            }
            if (count > 0) hr_avg = sum / count;
        } else {
            // Last active set metrics
            if (count_hr_active > 0) {
                hr_avg = sum_hr_active / count_hr_active;
                hr_max = max_hr_active;
            }
        }
        
        canvas.setTextColor(WHITE); canvas.setTextSize(1.5);
        if (hr_avg > 0) canvas.drawString(String(hr_avg), 10, 56);
        else canvas.drawString("--", 10, 56);
        
        if (hr_max > 0) canvas.drawString(String(hr_max), 88, 56);
        else canvas.drawString("--", 88, 56);
        
        if (connected && current_heart_rate > 0) {
            canvas.setTextColor(C_IRED);
            canvas.drawString(String(current_heart_rate), 166, 56);
        } else {
            canvas.setTextColor(C_IGRAY);
            canvas.drawString("--", 166, 56);
        }
        
        // Draw chart boundary
        canvas.drawRoundRect(5, 74, 230, 31, 4, C_CARD);
        
        // Choose source data
        int data_len = 0;
        int hr_min_val = 200;
        int hr_max_val = 40;
        
        if (show_live_curve) {
            // Live 60s
            data_len = hr_live_history_count;
            for (int k = 60 - hr_live_history_count; k < 60; k++) {
                int val = hr_live_history[k];
                if (val > 0) {
                    if (val < hr_min_val) hr_min_val = val;
                    if (val > hr_max_val) hr_max_val = val;
                }
            }
        } else {
            // Active series
            data_len = hr_active_series.size();
            for (int val : hr_active_series) {
                if (val > 0) {
                    if (val < hr_min_val) hr_min_val = val;
                    if (val > hr_max_val) hr_max_val = val;
                }
            }
        }
        
        if (data_len >= 2) {
            int yMax = hr_max_val + 2;
            int yMin = max(40, hr_min_val - 2);
            int yRange = yMax - yMin;
            if (yRange <= 0) yRange = 1;
            
            // Draw a subtle baseline
            int midY = 74 + 15;
            canvas.drawLine(7, midY, 233, midY, M5Cardputer.Display.color565(35, 35, 35));
            
            int prevX = 0, prevY = 0;
            for (int i = 0; i < data_len; i++) {
                int val = 0;
                if (show_live_curve) {
                    val = hr_live_history[60 - hr_live_history_count + i];
                } else {
                    val = hr_active_series[i];
                }
                
                if (val > 0) {
                    int px = 7 + (i * 226) / (data_len - 1);
                    int py = 101 - ((val - yMin) * 23) / yRange; // 23 pixels vertical span inside the 31px high card
                    
                    if (i > 0 && prevY > 0) {
                        canvas.drawLine(prevX, prevY, px, py, C_IRED);
                    }
                    prevX = px;
                    prevY = py;
                }
            }
        } else {
            canvas.setTextColor(C_IGRAY); canvas.setTextSize(1);
            if (show_live_curve) {
                canvas.drawString("Waiting for BLE data...", 55, 85);
            } else {
                canvas.drawString("No set data. Press ENTER.", 45, 85);
            }
        }
        
        // Mode indicator at bottom
        canvas.setTextColor(C_IGRAY); canvas.setTextSize(1);
        if (show_live_curve) {
            canvas.drawString("Mode: Live 60s (ENT: toggle)", 10, 107);
        } else {
            canvas.drawString("Mode: Last Set (ENT: toggle)", 10, 107);
        }
        
    } else if (current_view == VIEW_HISTORY) {
        canvas.setTextColor(WHITE); canvas.setTextSize(1.5);
        canvas.drawString("Workout Log", 10, 18);
        if (history_count == 0) {
            canvas.fillRoundRect(20, 45, 200, 50, 10, C_CARD);
            canvas.setTextColor(C_IGRAY); canvas.setTextSize(1);
            canvas.drawString("No sessions saved", 64, 60);
            canvas.drawString("Press R to reload", 70, 75);
        } else {
            const int VIS = 3, RH = 22;
            for (int vi = 0; vi < VIS; vi++) {
                int idx = history_scroll + vi;
                if (idx >= history_count) break;
                int y = 34 + (vi * (RH + 2));
                
                bool isSel = (idx == history_selected_idx);
                // Card background
                canvas.fillRoundRect(5, y, 228, RH, 6, isSel ? C_CARD_HI : C_CARD);
                if (isSel) canvas.drawRoundRect(5, y, 228, RH, 6, C_ACCENT);
                
                SessionLog &sess = history_sessions[idx];
                
                // Format Date / Timestamp
                String titleStr = "";
                if (sess.t > 1000000000) {
                    time_t sec = sess.t / 1000;
                    struct tm *tm_info = localtime(&sec);
                    char dateBuf[20];
                    sprintf(dateBuf, "%02d/%02d/%d %02d:%02d", tm_info->tm_mday, tm_info->tm_mon + 1, tm_info->tm_year + 1900, tm_info->tm_hour, tm_info->tm_min);
                    titleStr = String(dateBuf);
                } else {
                    titleStr = "Session #" + String(history_count - idx) + " (Millis: " + String(sess.t / 1000) + "s)";
                }
                
                canvas.setTextColor(WHITE); canvas.setTextSize(1);
                canvas.drawString(titleStr, 10, y + 3);
                
                // Exercise and sets count
                int totSets = 0, totVol = 0;
                for (const auto &ex : sess.exercises) {
                    totSets += ex.sets.size();
                    for (const auto &s : ex.sets) totVol += s.v;
                }
                canvas.setTextColor(C_IGRAY); canvas.setTextSize(1);
                canvas.drawString(String(sess.exercises.size()) + " Ex | " + String(totSets) + " Sets | " + String(totVol) + "kg", 10, y + 12);
            }
            if (history_scroll > 0) canvas.fillTriangle(228, 34, 224, 40, 232, 40, C_ACCENT);
            if (history_scroll + VIS < history_count) canvas.fillTriangle(228, 112, 224, 106, 232, 106, C_ACCENT);
        }
        if (history_confirm_delete && history_count > 0) {
            canvas.fillRoundRect(15, 30, 210, 80, 8, C_CARD);
            canvas.drawRoundRect(15, 30, 210, 80, 8, C_IRED);
            
            canvas.setTextColor(C_IRED); canvas.setTextSize(1.2);
            canvas.drawString("DELETE SESSION LOG?", 30, 38);
            
            canvas.setTextColor(WHITE); canvas.setTextSize(1);
            SessionLog &sess = history_sessions[history_selected_idx];
            canvas.drawString("Session of: " + String(sess.t), 30, 56);
            canvas.drawString("All sets will be removed", 30, 68);
            
            canvas.setTextColor(C_IYELLOW);
            canvas.drawString("ENT: Delete | DEL: Cancel", 24, 88);
        } else {
            canvas.setTextColor(C_IGRAY); canvas.setTextSize(1);
            canvas.drawString("R:refresh  ENT:open  D:delete", 10, 108);
        }
        
    } else if (current_view == VIEW_HISTORY_DETAIL) {
        if (history_count > 0 && history_selected_idx >= 0 && history_selected_idx < history_count) {
            SessionLog &sess = history_sessions[history_selected_idx];
            
            if (sess.exercises.empty()) {
                canvas.setTextColor(C_IGRAY); canvas.setTextSize(1);
                canvas.drawString("Sessione vuota", 50, 60);
                canvas.drawString("DEL: Indietro", 10, 108);
            } else {
                // Bounds safety
                if (detail_selected_ex_idx < 0) detail_selected_ex_idx = 0;
                if (detail_selected_ex_idx >= (int)sess.exercises.size()) detail_selected_ex_idx = sess.exercises.size() - 1;
                
                SessionExercise &ex = sess.exercises[detail_selected_ex_idx];
                
                // Header (Exercise Name + Left/Right arrows)
                canvas.setTextColor(C_ACCENT); canvas.setTextSize(1.5);
                String headerStr = "< " + ex.name + " >";
                if (headerStr.length() > 24) headerStr = ex.name.substring(0, 20) + "..";
                canvas.drawString(headerStr, 5, 18);
                
                canvas.setTextColor(C_IGRAY); canvas.setTextSize(1);
                canvas.drawString("Ex " + String(detail_selected_ex_idx + 1) + "/" + String(sess.exercises.size()) + " ([-/])", 140, 19);
                
                // Left pane: Scrollable sets list
                const int VIS_SETS = 3, RH_SETS = 22;
                if (menu_scroll_offset < 0) menu_scroll_offset = 0;
                if (menu_exercise_idx < 0) menu_exercise_idx = 0;
                if (menu_exercise_idx >= (int)ex.sets.size()) menu_exercise_idx = ex.sets.size() - 1;
                
                if (menu_exercise_idx < menu_scroll_offset) menu_scroll_offset = menu_exercise_idx;
                if (menu_exercise_idx >= menu_scroll_offset + VIS_SETS) menu_scroll_offset = menu_exercise_idx - VIS_SETS + 1;
                
                for (int vi = 0; vi < VIS_SETS; vi++) {
                    int sIdx = menu_scroll_offset + vi;
                    if (sIdx >= (int)ex.sets.size()) break;
                    int y = 34 + (vi * (RH_SETS + 2));
                    bool isSel = (sIdx == menu_exercise_idx);
                    
                    canvas.fillRoundRect(5, y, 120, RH_SETS, 6, isSel ? C_CARD_HI : C_CARD);
                    if (isSel) canvas.drawRoundRect(5, y, 120, RH_SETS, 6, C_ACCENT);
                    
                    SessionSet &s = ex.sets[sIdx];
                    canvas.setTextColor(WHITE); canvas.setTextSize(1);
                    canvas.drawString("Set " + String(s.s) + ": " + String(s.w) + "kg x" + String(s.r), 10, y + 3);
                    
                    canvas.setTextColor(C_IGRAY); canvas.setTextSize(1);
                    canvas.drawString("Vol: " + String(s.v) + "kg | 1RM: " + String(s.est_1rm) + "k", 10, y + 12);
                }
                
                // Right pane: Details & Heart Rate Live Plot for selected set
                SessionSet &selSet = ex.sets[menu_exercise_idx];
                int ry = 34;
                canvas.fillRoundRect(130, ry, 105, 70, 6, C_CARD);
                
                // Draw form dot
                uint16_t dotCol = (selSet.pf == 0) ? C_IGREEN : (selSet.pf == selSet.r ? C_IRED : C_IYELLOW);
                canvas.fillCircle(140, ry + 12, 4, dotCol);
                canvas.setTextColor(WHITE); canvas.setTextSize(1);
                canvas.drawString("Qualita': " + String((selSet.r - selSet.pf) * 100 / max(1, selSet.r)) + "%", 150, ry + 8);
                
                canvas.setTextColor(C_LABEL); canvas.setTextSize(1);
                canvas.drawString("Avg/Max HR: " + (selSet.hr_avg > 0 ? String(selSet.hr_avg) : "--") + "/" + (selSet.hr_max > 0 ? String(selSet.hr_max) : "--"), 135, ry + 24);
                canvas.drawString("Rec Avg/Mx: " + (selSet.hr_rec_avg > 0 ? String(selSet.hr_rec_avg) : "--") + "/" + (selSet.hr_rec_max > 0 ? String(selSet.hr_rec_max) : "--"), 135, ry + 36);
                
                // Tiny HR Mini Chart in right panel
                int n1 = selSet.hr_series.size();
                int n2 = selSet.hr_rec_series.size();
                int total_pts = n1 + n2;
                if (total_pts >= 2) {
                    int hr_min = 200, hr_max = 40;
                    for (int val : selSet.hr_series) { if (val > 0) { if (val < hr_min) hr_min = val; if (val > hr_max) hr_max = val; } }
                    for (int val : selSet.hr_rec_series) { if (val > 0) { if (val < hr_min) hr_min = val; if (val > hr_max) hr_max = val; } }
                    int yMax = hr_max + 2, yMin = max(40, hr_min - 2);
                    int yRange = yMax - yMin; if (yRange <= 0) yRange = 1;
                    
                    int prevX = 0, prevY = 0;
                    for (int i = 0; i < total_pts; i++) {
                        int val = (i < n1) ? selSet.hr_series[i] : selSet.hr_rec_series[i - n1];
                        if (val > 0) {
                            int px = 135 + (i * 95) / (total_pts - 1);
                            int py = ry + 64 - ((val - yMin) * 16) / yRange;
                            if (i > 0 && prevY > 0) {
                                canvas.drawLine(prevX, prevY, px, py, (i < n1) ? C_IRED : C_IGREEN);
                            }
                            prevX = px; prevY = py;
                        }
                    }
                } else {
                    canvas.setTextColor(C_IGRAY); canvas.setTextSize(1);
                    canvas.drawString("No HR Data", 155, ry + 52);
                }
                
                canvas.setTextColor(C_IGRAY); canvas.setTextSize(1);
                canvas.drawString("UP/DOWN: sets  DEL: indietro", 10, 108);
            }
        } else {
            canvas.setTextColor(C_IGRAY); canvas.setTextSize(1);
            canvas.drawString("Nessun dato selezionato", 50, 60);
        }
    } else if (current_view == VIEW_EXERCISES) {
        if (creating_custom_exercise) {
            // Glassmorphic Custom Exercise typing modal
            canvas.fillRoundRect(10, 20, 220, 95, 8, C_CARD);
            canvas.drawRoundRect(10, 20, 220, 95, 8, C_ACCENT);
            
            canvas.setTextColor(C_IORANGE); canvas.setTextSize(1.2);
            canvas.drawCentreString("Create Custom Exercise", 120, 28);
            
            canvas.setTextColor(WHITE); canvas.setTextSize(1);
            canvas.drawString("Type exercise name:", 20, 48);
            
            // Text Input Box
            canvas.fillRoundRect(20, 60, 200, 20, 4, BLACK);
            canvas.drawRoundRect(20, 60, 200, 20, 4, C_CARD_HI);
            
            canvas.setTextColor(C_IGREEN); canvas.setTextSize(1.2);
            canvas.drawString(custom_exercise_input_str + "_", 26, 64);
            
            canvas.setTextColor(C_IGRAY); canvas.setTextSize(1);
            canvas.drawCentreString("ENTER: Save  |  DEL: Cancel", 120, 92);
        } else {
            const int VR = 5, RH = 18;
            const int LIST_Y = 34; // below header
            
            // Smooth scroll: lerp towards target
            scroll_anim_offset += (menu_scroll_offset - scroll_anim_offset) * 0.35f;
            float sOff = (menu_scroll_offset - scroll_anim_offset) * RH;
            
            if (search_query.length() > 0) {
                canvas.setTextColor(C_ACCENT); canvas.setTextSize(1.5);
                canvas.drawString("Search: " + search_query + "_", 10, 18);
                if (menu_exercise_idx < menu_scroll_offset) menu_scroll_offset = menu_exercise_idx;
                if (menu_exercise_idx >= menu_scroll_offset + VR) menu_scroll_offset = menu_exercise_idx - VR + 1;
                for (int vi = 0; vi < VR; vi++) {
                    int idx = menu_scroll_offset + vi;
                    if (idx >= search_count) break;
                    int y = LIST_Y + (vi * RH) + (int)sOff;
                    if (y < 15 || y > 115) continue;
                    int sm = search_results[idx].m;
                    int se = search_results[idx].e;
                    if (idx == menu_exercise_idx) {
                        canvas.fillRoundRect(5, y, 230, RH - 2, 6, C_ACCENT);
                        canvas.setTextColor(WHITE);
                    } else {
                        canvas.fillRoundRect(5, y, 230, RH - 2, 6, C_CARD);
                        canvas.setTextColor(C_LABEL);
                    }
                    canvas.setTextSize(1.5);
                    canvas.drawString(getExerciseName(sm, se), 15, y + 3);
                    // Equipment icon on right
                    uint16_t mc_s[11] = {C_IRED, C_ACCENT, C_IGREEN, M5Cardputer.Display.color565(0,200,200), C_IORANGE,
                                         M5Cardputer.Display.color565(200,0,200), C_ACCENT, M5Cardputer.Display.color565(140,60,200), C_IYELLOW, C_IGRAY, C_IORANGE};
                    drawEquipIcon(canvas, 206, y - 3, getExerciseEquip(sm, se), mc_s[sm]);
                }
            } else if (selecting_muscle) {
                canvas.setTextColor(WHITE); canvas.setTextSize(1.5);
                canvas.drawString("Muscle Group", 10, 18);
                if (menu_muscle_idx < menu_scroll_offset) menu_scroll_offset = menu_muscle_idx;
                if (menu_muscle_idx >= menu_scroll_offset + VR) menu_scroll_offset = menu_muscle_idx - VR + 1;
                uint16_t mc[11] = {C_IRED, C_ACCENT, C_IGREEN, M5Cardputer.Display.color565(0,200,200), C_IORANGE,
                                   M5Cardputer.Display.color565(200,0,200), C_ACCENT, M5Cardputer.Display.color565(140,60,200), C_IYELLOW, C_IGRAY, C_IORANGE};
                for (int vi = 0; vi < VR; vi++) {
                    int idx = menu_scroll_offset + vi;
                    if (idx >= NUM_MUSCLES) break;
                    int y = LIST_Y + (vi * RH) + (int)sOff;
                    if (y < 15 || y > 115) continue;
                    if (idx == menu_muscle_idx) {
                        canvas.fillRoundRect(5, y, 170, RH - 2, 6, mc[idx]);
                        canvas.setTextColor(BLACK);
                    } else {
                        canvas.fillRoundRect(5, y, 170, RH - 2, 6, C_CARD);
                        canvas.setTextColor(WHITE);
                    }
                    canvas.setTextSize(1.5);
                    canvas.drawString(muscle_names[idx], 15, y + 3);
                }
                drawMuscleIcon(canvas, 183, LIST_Y, menu_muscle_idx, mc[menu_muscle_idx]);
                if (menu_scroll_offset > 0) canvas.fillTriangle(228, LIST_Y, 224, LIST_Y+6, 232, LIST_Y+6, C_ACCENT);
                if (menu_scroll_offset + VR < NUM_MUSCLES) canvas.fillTriangle(228, 112, 224, 106, 232, 106, C_ACCENT);
            } else {
                canvas.setTextColor(C_IORANGE); canvas.setTextSize(1.5);
                canvas.drawString(String(muscle_names[menu_muscle_idx]), 10, 18);
                int maxEx = current_ex_count;
                if (menu_exercise_idx < menu_scroll_offset) menu_scroll_offset = menu_exercise_idx;
                if (menu_exercise_idx >= menu_scroll_offset + VR) menu_scroll_offset = menu_exercise_idx - VR + 1;
                uint16_t mc2[11] = {C_IRED, C_ACCENT, C_IGREEN, M5Cardputer.Display.color565(0,200,200), C_IORANGE,
                                     M5Cardputer.Display.color565(200,0,200), C_ACCENT, M5Cardputer.Display.color565(140,60,200), C_IYELLOW, C_IGRAY, C_IORANGE};
                for (int vi = 0; vi < VR; vi++) {
                    int idx = menu_scroll_offset + vi;
                    if (idx >= maxEx) break;
                    int y = LIST_Y + (vi * RH) + (int)sOff;
                    if (y < 15 || y > 115) continue;
                    int real_ex_idx = current_ex_list[idx];
                    bool isFav = (menu_muscle_idx == 10) ? false : is_favorite[menu_muscle_idx][real_ex_idx];
                    if (idx == menu_exercise_idx) {
                        canvas.fillRoundRect(5, y, 170, RH - 2, 6, C_ACCENT);
                        canvas.setTextColor(WHITE);
                    } else {
                        canvas.fillRoundRect(5, y, 170, RH - 2, 6, C_CARD);
                        canvas.setTextColor(C_LABEL);
                    }
                    canvas.setTextSize(1.5);
                    canvas.drawString(getExerciseName(menu_muscle_idx, real_ex_idx), 15, y + 3);
                    if (isFav) {
                        canvas.setTextColor(C_IRED);
                        canvas.drawString("<3", 150, y + 3);
                    }
                    // Equipment icon on right side of row
                    drawEquipIcon(canvas, 150, y - 3, getExerciseEquip(menu_muscle_idx, real_ex_idx), mc2[menu_muscle_idx]);
                }
                drawMuscleIcon(canvas, 183, LIST_Y+8, menu_muscle_idx, mc2[menu_muscle_idx]);
                if (menu_scroll_offset > 0) canvas.fillTriangle(228, LIST_Y, 224, LIST_Y+6, 232, LIST_Y+6, C_ACCENT);
                if (menu_scroll_offset + VR < maxEx) canvas.fillTriangle(228, 112, 224, 106, 232, 106, C_ACCENT);
                
                canvas.setTextColor(C_IGRAY); canvas.setTextSize(1);
                canvas.drawString("SPC: Fav", 183, 100);
                canvas.drawString(" N: New", 183, 112);
            }
        }
    } else if (current_view == VIEW_PR) {
        canvas.setTextColor(WHITE); canvas.setTextSize(1.5);
        canvas.drawString("Personal Records", 10, 18);
        int total_prs = pr_doc.size();
        if (total_prs == 0) {
            canvas.fillRoundRect(20, 45, 200, 50, 10, C_CARD);
            canvas.setTextColor(C_IGRAY); canvas.setTextSize(1);
            canvas.drawString("No PRs yet", 80, 60);
        } else {
            const int VIS = 4, RH = 20;
            int drawn = 0;
            int idx = 0;
            for (JsonPair kv : pr_doc.as<JsonObject>()) {
                if (idx >= pr_scroll && drawn < VIS) {
                    int y = 34 + (drawn * (RH + 2));
                    canvas.fillRoundRect(5, y, 230, RH, 6, C_CARD);
                    
                    // Trophy icon
                    canvas.fillCircle(18, y + 10, 5, C_IORANGE);
                    canvas.setTextColor(BLACK); canvas.setTextSize(1);
                    canvas.drawString("P", 15, y + 7);
                    
                    canvas.setTextColor(WHITE); canvas.setTextSize(1);
                    String prName = String(kv.key().c_str());
                    if (prName.length() > 18) prName = prName.substring(0, 16) + "..";
                    canvas.drawString(prName, 28, y + 3);
                    
                    canvas.setTextColor(C_IORANGE); canvas.setTextSize(1.5);
                    canvas.drawString(String(kv.value().as<int>()) + "kg", 180, y + 4);
                    drawn++;
                }
                idx++;
            }
            if (pr_scroll > 0) canvas.fillTriangle(228, 34, 224, 40, 232, 40, C_ACCENT);
            if (pr_scroll + VIS < total_prs) canvas.fillTriangle(228, 112, 224, 106, 232, 106, C_ACCENT);
        }
    } else if (current_view == VIEW_ROUTINES) {
        if (routine_subview == ROUTINE_LIST) {
            canvas.setTextColor(WHITE); canvas.setTextSize(1.5);
            canvas.drawString("Routines", 10, 18);
            if (routines_count > 0 && selected_routine_idx >= 0 && selected_routine_idx < routines_count) {
                Routine &r = routines[selected_routine_idx];
                int est_sec = getRoutineEstimatedTimeSec(r);
                canvas.setTextColor(C_IORANGE); canvas.setTextSize(1);
                canvas.drawString("Est: " + formatDuration(est_sec), 140, 20);
            }
            if (routines_count == 0) {
                canvas.fillRoundRect(20, 45, 200, 50, 10, C_CARD);
                canvas.setTextColor(C_IGRAY); canvas.setTextSize(1);
                canvas.drawString("No routines yet", 68, 60);
                canvas.drawString("Press N to create", 65, 75);
            } else {
                // Split screen
                for (int i = 0; i < routines_count; i++) {
                    int y = 36 + (i * 22);
                    bool sel = (i == selected_routine_idx);
                    canvas.fillRoundRect(5, y, 95, 20, 4, sel ? C_ACCENT : C_CARD);
                    canvas.setTextColor(sel ? WHITE : C_LABEL); canvas.setTextSize(1);
                    canvas.drawString(routines[i].name, 10, y + 5);
                }
                
                // Right Preview Card
                canvas.fillRoundRect(106, 36, 128, 76, 6, C_CARD);
                canvas.setTextColor(WHITE); canvas.setTextSize(1);
                canvas.drawString("Exercises:", 112, 40);
                
                Routine &r = routines[selected_routine_idx];
                int max_disp = min(4, r.exercise_count);
                for (int j = 0; j < max_disp; j++) {
                    int py = 52 + (j * 11);
                    canvas.setTextColor(C_IGRAY); canvas.setTextSize(1);
                    String exName = r.exercises[j].name;
                    if (exName.length() > 14) exName = exName.substring(0, 12) + "..";
                    canvas.drawString(exName, 112, py);
                    canvas.setTextColor(C_IORANGE);
                    canvas.drawString(String(r.exercises[j].target_sets) + "x" + String(r.exercises[j].target_reps), 205, py);
                }
                if (r.exercise_count > 4) {
                    canvas.setTextColor(C_IGRAY); canvas.setTextSize(1);
                    canvas.drawString("... and " + String(r.exercise_count - 4) + " more", 112, 96);
                }
            }
            if (routine_confirm_delete && routines_count > 0) {
                // Draw a high-contrast modal alert box over the routines list
                canvas.fillRoundRect(15, 30, 210, 80, 8, C_CARD);
                canvas.drawRoundRect(15, 30, 210, 80, 8, C_IRED);
                
                canvas.setTextColor(C_IRED); canvas.setTextSize(1.2);
                canvas.drawString("DELETE THIS ROUTINE?", 24, 38);
                
                canvas.setTextColor(WHITE); canvas.setTextSize(1);
                String itemStr = routines[selected_routine_idx].name;
                canvas.drawString(itemStr, 30, 56);
                canvas.drawString("Exercises in routine: " + String(routines[selected_routine_idx].exercise_count), 30, 68);
                
                canvas.setTextColor(C_IYELLOW);
                canvas.drawString("ENT: Delete | DEL: Cancel", 24, 88);
            } else {
                canvas.setTextColor(C_IGRAY); canvas.setTextSize(1);
                canvas.drawString("N:new  D:del  ENT:start", 10, 110);
            }
        } else if (routine_subview == ROUTINE_CREATOR) {
            canvas.setTextColor(WHITE); canvas.setTextSize(1.5);
            canvas.drawString("Edit: " + String(temp_routine.name), 10, 18);
            int est_sec = getRoutineEstimatedTimeSec(temp_routine);
            canvas.setTextColor(C_IORANGE); canvas.setTextSize(1);
            canvas.drawString("Est: " + formatDuration(est_sec), 160, 20);
            
            if (temp_routine.exercise_count == 0) {
                canvas.fillRoundRect(20, 45, 200, 50, 10, C_CARD);
                canvas.setTextColor(C_IGRAY); canvas.setTextSize(1);
                canvas.drawString("No exercises added", 60, 60);
                canvas.drawString("Press A to add", 75, 75);
            } else {
                const int VIS = 3;
                int start = max(0, creator_selected_idx - 2);
                for (int vi = 0; vi < min(VIS, temp_routine.exercise_count); vi++) {
                    int idx = start + vi;
                    if (idx >= temp_routine.exercise_count) break;
                    int y = 36 + (vi * 23);
                    bool sel = (idx == creator_selected_idx);
                    canvas.fillRoundRect(5, y, 230, 21, 4, sel ? C_CARD_HI : C_CARD);
                    if (sel) canvas.drawRoundRect(5, y, 230, 21, 4, C_ACCENT);
                    
                    canvas.setTextColor(WHITE); canvas.setTextSize(1);
                    String exName = temp_routine.exercises[idx].name;
                    if (exName.length() > 18) exName = exName.substring(0, 16) + "..";
                    canvas.drawString(exName, 10, y + 5);
                    
                    canvas.setTextColor(C_IORANGE);
                    int r_m = temp_routine.exercises[idx].target_rest / 60;
                    int r_s = temp_routine.exercises[idx].target_rest % 60;
                    char r_buf[16];
                    snprintf(r_buf, sizeof(r_buf), "%d:%02d", r_m, r_s);
                    canvas.drawString(String(temp_routine.exercises[idx].target_sets) + "x" + String(temp_routine.exercises[idx].target_reps) + " | " + String(r_buf), 145, y + 5);
                }
            }
            canvas.setTextColor(C_IGRAY); canvas.setTextSize(1);
            canvas.drawString("A:add D:del E:edit S:save ESC:cancel", 10, 110);
        } else if (routine_subview == ROUTINE_PICK_EXERCISE) {
            canvas.setTextColor(C_ACCENT); canvas.setTextSize(1.5);
            canvas.drawString("Add Exercise", 10, 18);
            
            const int VR = 4, RH = 16;
            
            // Sync muscle list scroll offset for routines picker
            if (selecting_muscle) {
                if (menu_muscle_idx < menu_scroll_offset) menu_scroll_offset = menu_muscle_idx;
                if (menu_muscle_idx >= menu_scroll_offset + VR) menu_scroll_offset = menu_muscle_idx - VR + 1;
            }
            
            canvas.fillRoundRect(5, 34, 100, 72, 4, C_CARD);
            canvas.fillRoundRect(110, 34, 125, 72, 4, C_CARD);
            
            for (int vi = 0; vi < VR; vi++) {
                int idx = menu_scroll_offset + vi;
                if (idx >= NUM_MUSCLES) break;
                int y = 36 + (vi * RH);
                bool sel = (idx == menu_muscle_idx && selecting_muscle);
                if (sel) canvas.fillRect(7, y, 96, RH, C_ACCENT);
                canvas.setTextColor(sel ? WHITE : C_LABEL); canvas.setTextSize(1);
                canvas.drawString(muscle_names[idx], 10, y + 3);
            }
            
            if (!selecting_muscle) {
                int maxEx = (menu_muscle_idx == 10) ? custom_exercises.size() : ex_count[menu_muscle_idx];
                int estart = max(0, menu_exercise_idx - 3);
                for (int vi = 0; vi < VR; vi++) {
                    int idx = estart + vi;
                    if (idx >= maxEx) break;
                    int y = 36 + (vi * RH);
                    bool sel = (idx == menu_exercise_idx);
                    if (sel) canvas.fillRect(112, y, 121, RH, C_ACCENT);
                    canvas.setTextColor(sel ? WHITE : C_LABEL); canvas.setTextSize(1);
                    String exName = getExerciseName(menu_muscle_idx, idx);
                    if (exName.length() > 18) exName = exName.substring(0, 16) + "..";
                    canvas.drawString(exName, 115, y + 3);
                }
            } else {
                canvas.setTextColor(C_IGRAY); canvas.setTextSize(1);
                canvas.drawString("Select Muscle Group", 115, 60);
                canvas.drawString("then press ENTER", 115, 72);
            }
            
            canvas.setTextColor(C_IGRAY); canvas.setTextSize(1);
            canvas.drawString("ENTER:Select  DEL:Back", 10, 110);
        } else if (routine_subview == ROUTINE_EDIT_ITEM) {
            RoutineExercise &ex = temp_routine.exercises[creator_selected_idx];
            canvas.setTextColor(WHITE); canvas.setTextSize(1.5);
            canvas.drawString("Edit Details", 10, 18);
            canvas.drawString(ex.name, 10, 32);
            
            const char* fields[] = {"Target Sets", "Target Reps", "Rest Timer (M:SS)"};
            int vals[] = {ex.target_sets, ex.target_reps, ex.target_rest};
            for (int i = 0; i < 3; i++) {
                int y = 48 + (i * 20);
                bool sel = (i == edit_field_idx);
                canvas.fillRoundRect(5, y, 230, 18, 4, sel ? C_CARD_HI : C_CARD);
                if (sel) canvas.drawRoundRect(5, y, 230, 18, 4, C_ACCENT);
                
                canvas.setTextColor(sel ? C_ACCENT : C_LABEL); canvas.setTextSize(1);
                canvas.drawString(fields[i], 12, y + 4);
                
                canvas.setTextColor(WHITE); canvas.setTextSize(1);
                if (sel) {
                    if (i == 2) {
                        canvas.drawString(formatRestTimerInput(edit_input_str) + "_", 180, y + 4);
                    } else {
                        canvas.drawString(edit_input_str + "_", 180, y + 4);
                    }
                } else {
                    if (i == 2) {
                        int m = vals[i] / 60;
                        int s = vals[i] % 60;
                        char buf[16];
                        snprintf(buf, sizeof(buf), "%d:%02d", m, s);
                        canvas.drawString(String(buf), 180, y + 4);
                    } else {
                        canvas.drawString(String(vals[i]), 180, y + 4);
                    }
                }
            }
            
            canvas.setTextColor(C_IGRAY); canvas.setTextSize(1);
            canvas.drawString("UP/DOWN:field ENT:save DEL:cancel", 10, 110);
        }
    } else if (current_view == VIEW_HELP) {
        struct HelpItem {
            const char* key;
            const char* desc;
            uint16_t color;
        };

        static const HelpItem help_items[] = {
            {"1-7", "Switch Views (Workout, Stats, Exs...)", C_IORANGE},
            {"h/H", "Toggle Help screen (Open/Close)", C_IYELLOW},
            {"w/W", "Toggle Wi-Fi (Start AP / Client)", C_IYELLOW},
            {"b/B", "Toggle BLE scanning for Mi Band", C_IYELLOW},
            {"m/M", "Toggle Speaker Mute (Sound Toggle)", C_IGREEN},
            {"e/E", "Edit Weight (only in Ready state)", C_IYELLOW},
            {"0-9", "Manual override reps (Active state)", C_IYELLOW},
            {"SPC", "Toggle Favorite exercise in list", C_IYELLOW},
            {"ENT", "Start Set / OK / Save Input", C_IYELLOW},
            {"DEL", "Cancel / Back / Delete completed Set", C_IYELLOW},
            {"q/Q", "Complete Set / End workout session", C_IYELLOW},
            {"VBT", "Beep feedback: Green/Yellow/Red reps", C_IORANGE},
            {"AP", "WiFi Dashboard at http://192.168.4.1", C_ACCENT},
            {"WiFi", "Client WiFi: sync logs & curves", C_ACCENT}
        };
        static const int HELP_ITEMS_COUNT = sizeof(help_items) / sizeof(help_items[0]);

        // Draw header
        canvas.setTextColor(C_ACCENT); canvas.setTextSize(1.2);
        canvas.drawString("GymTracker Help (Scroll with ;, .)", 10, 10);
        canvas.drawLine(0, 22, 240, 22, C_CARD);

        // Adjust scroll offset
        if (help_selected_idx < 0) help_selected_idx = 0;
        if (help_selected_idx >= HELP_ITEMS_COUNT) help_selected_idx = HELP_ITEMS_COUNT - 1;
        if (help_selected_idx < help_scroll_offset) {
            help_scroll_offset = help_selected_idx;
        }
        if (help_selected_idx >= help_scroll_offset + 5) {
            help_scroll_offset = help_selected_idx - 5 + 1;
        }

        // Render visible items
        for (int i = 0; i < 5; i++) {
            int idx = help_scroll_offset + i;
            if (idx >= HELP_ITEMS_COUNT) break;

            const auto &item = help_items[idx];
            bool sel = (idx == help_selected_idx);
            int y = 26 + i * 21;

            canvas.fillRoundRect(6, y, 228, 18, 4, sel ? C_CARD_HI : C_CARD);
            if (sel) {
                canvas.drawRoundRect(6, y, 228, 18, 4, C_ACCENT);
            }

            canvas.fillRoundRect(10, y + 2, 42, 14, 3, item.color);
            canvas.setTextColor(BLACK); canvas.setTextSize(1);
            canvas.drawCentreString(item.key, 31, y + 5);

            canvas.setTextColor(sel ? WHITE : C_LABEL);
            canvas.drawString(item.desc, 58, y + 5);
        }
    }
    
    canvas.pushSprite(0, 0);
}

// --- DATA LOGGING ---
String getTodayFilename() {
    // Use millis-based session ID since we have no RTC/NTP
    return String(SD_DIR) + "/session.jsonl";
}

int getCompletedSetsCount(String exerciseName) {
    for (const auto &ex : active_session.exercises) {
        if (ex.name == exerciseName) {
            return ex.sets.size();
        }
    }
    return 0;
}

void saveSessionToLog() {
    if (active_session.exercises.empty()) return;
    
    fs::FS& fs = getWorkoutLogFS();
    
    // 1. Check if the last session in the file has the same timestamp as active_session.t
    bool last_matches = false;
    
    if (fs.exists("/workout_log.json")) {
        File f = fs.open("/workout_log.json", FILE_READ);
        if (f) {
            String lastLine;
            while (f.available()) {
                String line = f.readStringUntil('\n');
                line.trim();
                if (line.length() > 2) {
                    lastLine = line;
                }
            }
            f.close();
            
            if (lastLine.length() > 2) {
                JsonDocument lastDoc;
                if (deserializeJson(lastDoc, lastLine) == DeserializationError::Ok) {
                    unsigned long long last_t = lastDoc["t"] | 0ULL;
                    if (last_t == active_session.t) {
                        last_matches = true;
                    }
                }
            }
        }
    }
    
    JsonDocument doc;
    serializeSession(active_session, doc);
    String jsonStr;
    serializeJson(doc, jsonStr);
    
    if (last_matches) {
        // We need to rewrite workout_log.json, replacing the last line.
        // We do this by copying all lines except the last one to a temp file, then writing the new jsonStr, and renaming.
        File f = fs.open("/workout_log.json", FILE_READ);
        File temp = fs.open("/workout_log_temp.json", "w");
        if (f && temp) {
            String pendingLine = "";
            while (f.available()) {
                String line = f.readStringUntil('\n');
                line.trim();
                if (line.length() > 2) {
                    if (pendingLine.length() > 0) {
                        temp.println(pendingLine);
                    }
                    pendingLine = line;
                }
            }
            temp.println(jsonStr);
            f.close();
            temp.close();
            
            fs.remove("/workout_log.json");
            fs.rename("/workout_log_temp.json", "/workout_log.json");
        } else {
            if (f) f.close();
            if (temp) temp.close();
        }
    } else {
        // Just append to the file
        File f = fs.open("/workout_log.json", FILE_APPEND);
        if (f) {
            f.println(jsonStr);
            f.close();
        }
    }
}

void finishSession() {
    if (active_session.exercises.empty()) return;
    
    // The session is already saved/updated after each set, so we don't need to append here.
    Serial.println("Session Completed.");
    
    // Clear temporary backup
    getWorkoutLogFS().remove("/active_session.json");
    
    // Clear memory active session
    active_session.exercises.clear();
    active_session.t = 0;
    
    // Reset live session trackers
    current_set_number = 0;
    estimated_1rm = 0;
    
    // Go to history to see the new entry
    current_view = VIEW_HISTORY;
    loadHistory();
    bg_color = BLACK;
}

void saveSet() {
    if (current_reps == 0) return;
    
    // Calculate 1RM (Epley formula): 1RM = w * (1 + r/30)
    if (current_reps > 0 && current_reps <= 12) {
        float e1rm = current_weight * (1.0f + (float)current_reps / 30.0f);
        if (e1rm > estimated_1rm) estimated_1rm = e1rm;
    }
    
    if (estimated_1rm > 0) {
        checkAndUpdatePR(active_exercise, estimated_1rm);
    }
    
    last_weights_doc[active_exercise] = current_weight;
    File fw = LittleFS.open("/weights.json", "w");
    serializeJson(last_weights_doc, fw);
    fw.close();
    
    // Check if session timestamp needs initialization
    if (active_session.exercises.empty()) {
        active_session.t = is_time_synced ? (time(NULL) * 1000ULL) : millis();
    }
    
    // Find or add exercise in active_session
    int exIdx = -1;
    for (size_t i = 0; i < active_session.exercises.size(); i++) {
        if (active_session.exercises[i].name == active_exercise) {
            exIdx = i;
            break;
        }
    }
    if (exIdx == -1) {
        SessionExercise newEx;
        newEx.name = active_exercise;
        active_session.exercises.push_back(newEx);
        exIdx = active_session.exercises.size() - 1;
    }
    
    // Populate the new Set
    SessionSet newSet;
    newSet.s = current_set_number + 1;
    newSet.r = (int)current_reps;
    newSet.w = (int)current_weight;
    newSet.v = (int)(current_reps * current_weight);
    newSet.pf = (int)current_poor_form_reps;
    newSet.est_1rm = (int)estimated_1rm;
    
    int avg_hr = (count_hr_active > 0) ? (sum_hr_active / count_hr_active) : 0;
    newSet.hr_avg = avg_hr;
    newSet.hr_max = max_hr_active;
    newSet.hr_series = hr_active_series;
    
    int sum_rec = 0, max_rec = 0;
    for (int val : hr_recovery_series) {
        sum_rec += val;
        if (val > max_rec) max_rec = val;
    }
    newSet.hr_rec_avg = (hr_recovery_series.size() > 0) ? (sum_rec / hr_recovery_series.size()) : 0;
    newSet.hr_rec_max = max_rec;
    newSet.hr_rec_series = hr_recovery_series;
    newSet.rep_velocities = current_set_rep_velocities;
    
    active_session.exercises[exIdx].sets.push_back(newSet);
    
    // Save/Backup active session to temporary file for crash resilience on active filesystem
    JsonDocument tempDoc;
    serializeSession(active_session, tempDoc);
    File fTemp = getWorkoutLogFS().open("/active_session.json", "w");
    if (fTemp) {
        serializeJson(tempDoc, fTemp);
        fTemp.close();
    }
    
    // Update daily totals
    today_total_sets++;
    today_total_reps += current_reps;
    today_total_volume += (current_weight * current_reps);
    today_bad_reps += current_poor_form_reps;
    current_set_number++;
    
    // Save the updated session directly to the log file on SD/LittleFS
    saveSessionToLog();
}

// Load history from LittleFS into display cache
void loadHistory() {
    history_count = 0;
    history_scroll = 0;
    
    if (!getWorkoutLogFS().exists("/workout_log.json")) return;
    File f = getWorkoutLogFS().open("/workout_log.json", FILE_READ);
    if (!f) return;
    
    // Read all lines, keep last 10 sessions
    String allLines[10];
    int total = 0;
    while (f.available() && total < 100) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() > 2) {
            allLines[total % 10] = line;
            total++;
        }
    }
    f.close();
    
    int count = min(total, 10);
    int start = (total > 10) ? total - 10 : 0;
    
    for (int i = count - 1; i >= 0; i--) {
        int srcIdx = (start + i) % 10;
        JsonDocument d;
        if (deserializeJson(d, allLines[srcIdx]) == DeserializationError::Ok) {
            deserializeSession(d, history_sessions[history_count]);
            history_count++;
        }
    }
}

// Count total parsed sessions in workout_log.json
int getHistoryFileLinesCount() {
    if (!getWorkoutLogFS().exists("/workout_log.json")) return 0;
    File f = getWorkoutLogFS().open("/workout_log.json", FILE_READ);
    if (!f) return 0;
    int total = 0;
    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() > 2) {
            total++;
        }
    }
    f.close();
    return total;
}

// Delete a specific session from workout_log.json by chronological index
void deleteHistoryEntry(int targetLineIdx) {
    if (!getWorkoutLogFS().exists("/workout_log.json")) return;
    
    File f = getWorkoutLogFS().open("/workout_log.json", FILE_READ);
    if (!f) return;
    
    File temp = getWorkoutLogFS().open("/workout_log_temp.json", "w");
    if (!temp) {
        f.close();
        return;
    }
    
    int currentLine = 0;
    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() > 2) {
            if (currentLine != targetLineIdx) {
                temp.println(line);
            }
            currentLine++;
        }
    }
    
    f.close();
    temp.close();
    
    getWorkoutLogFS().remove("/workout_log.json");
    getWorkoutLogFS().rename("/workout_log_temp.json", "/workout_log.json");
}

// Load last 7 set volumes from workout_log.json for graph progression
void loadStatsGraphData() {
    stats_sets_count = 0;
    for (int i = 0; i < 7; i++) {
        stats_volumes[i] = 0;
        stats_labels[i] = "";
    }
    
    if (!getWorkoutLogFS().exists("/workout_log.json")) return;
    File f = getWorkoutLogFS().open("/workout_log.json", FILE_READ);
    if (!f) return;
    
    String lines[100];
    int total = 0;
    while (f.available() && total < 100) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() > 2) {
            lines[total % 100] = line;
            total++;
        }
    }
    f.close();
    
    int start = (total > 7) ? total - 7 : 0;
    stats_sets_count = (total > 7) ? 7 : total;
    
    for (int i = 0; i < stats_sets_count; i++) {
        JsonDocument d;
        if (deserializeJson(d, lines[(start + i) % 100]) == DeserializationError::Ok) {
            int sess_vol = 0;
            if (d["exercises"].is<JsonArray>()) {
                JsonArray exercises = d["exercises"].as<JsonArray>();
                for (JsonObject exObj : exercises) {
                    if (exObj["sets"].is<JsonArray>()) {
                        JsonArray sets = exObj["sets"].as<JsonArray>();
                        for (JsonObject sObj : sets) {
                            sess_vol += sObj["v"].as<int>();
                        }
                    }
                }
            }
            stats_volumes[i] = sess_vol;
            
            unsigned long long ts = d["t"] | 0ULL;
            if (ts > 1000000000) {
                time_t sec = ts / 1000;
                struct tm *tm_info = localtime(&sec);
                char dateBuf[10];
                sprintf(dateBuf, "%d/%d", tm_info->tm_mday, tm_info->tm_mon + 1);
                stats_labels[i] = String(dateBuf);
            } else {
                stats_labels[i] = "S#" + String(total - stats_sets_count + i + 1);
            }
        }
    }
}

// Update the RGB LED based on workout and rest state in a non-blocking manner
void updateLED() {
    if (workout_state == STATE_SUMMARY) {
        unsigned long rest_elapsed = millis() - rest_start_time;
        int rest_sec = rest_elapsed / 1000;
        bool done = rest_sec >= rest_target_sec;
        
        if (done) {
            ledPixel.setPixelColor(0, ledPixel.Color(0, 180, 0)); // Solid Green
        } else {
            // Pulse Blue
            float breath = (sin(millis() / 300.0f) + 1.0f) / 2.0f;
            int val = (int)(breath * 100.0f) + 10;
            ledPixel.setPixelColor(0, ledPixel.Color(0, 0, val));
        }
        ledPixel.show();
    } else if (led_flashing) {
        if (millis() > led_flash_end_time) {
            led_flashing = false;
            ledPixel.setPixelColor(0, ledPixel.Color(0, 0, 0)); // Turn off
            ledPixel.show();
        }
    } else {
        static bool was_off = false;
        if (workout_state == STATE_ACTIVE) {
            was_off = false; // reset so we turn off when finished
        } else if (!was_off) {
            ledPixel.setPixelColor(0, ledPixel.Color(0, 0, 0));
            ledPixel.show();
            was_off = true;
        }
    }
}

// --- IMU LOGIC ---
void updateIMU() {
    float ax, ay, az;
    M5.Imu.getAccelData(&ax, &ay, &az);
    
    // Use magnitude to be orientation independent
    float acc_mag = sqrt(ax*ax + ay*ay + az*az);
    
    // Low pass filter
    filtered_acc = ALPHA_LPF * acc_mag + (1.0f - ALPHA_LPF) * filtered_acc;
}

void processRepetition() {
    unsigned long now = millis();
    float acc = filtered_acc;
    
    // Neutral zone: acceleration must return near 1.0g between reps
    // Tighter neutral zone to detect small deviations on short-range reps
    const float NEUTRAL_LOW  = 0.97f;
    const float NEUTRAL_HIGH = 1.03f;
    const unsigned long MIN_PHASE_MS    = 200;   // slightly shorter phase validation (200ms) for fast short-stroke movements
    const unsigned long COOLDOWN_MS     = 300;   // lockout after counting a rep
    const unsigned long PHASE_TIMEOUT   = 5000;  // abort if stuck in a phase
    
    static unsigned long last_rep_time = 0;      // cooldown tracker
    static bool in_neutral = true;               // must pass through neutral zone
    static float peak_acc = 1.0f;                // track peak during concentric
    static float valley_acc = 1.0f;              // track valley during eccentric

    if (!is_eccentric_first) {
        // --- CONCENTRIC-FIRST WORKFLOW (Standard lifting first, e.g., curls, pulls) ---
        switch (rep_phase) {
            case PHASE_REST:
                if (acc >= NEUTRAL_LOW && acc <= NEUTRAL_HIGH) {
                    in_neutral = true;
                }
                if (in_neutral && acc > THRESHOLD_HIGH && (now - last_rep_time > COOLDOWN_MS)) {
                    rep_phase = PHASE_CONCENTRIC;
                    concentric_start_time = now;
                    peak_acc = acc;
                    in_neutral = false;
                    current_velocity = 0;
                    peak_velocity = 0;
                }
                break;
                
            case PHASE_CONCENTRIC:
                current_velocity += (acc - 1.0f) * 9.81f * 0.01f;
                if (current_velocity > peak_velocity) peak_velocity = current_velocity;
                if (acc > peak_acc) peak_acc = acc;
                
                if (acc < THRESHOLD_LOW) {
                    unsigned long duration = now - concentric_start_time;
                    if (duration >= MIN_PHASE_MS && peak_acc > THRESHOLD_HIGH + 0.005f) {
                        rep_phase = PHASE_ECCENTRIC;
                        current_concentric_duration = duration;
                        eccentric_start_time = now;
                        valley_acc = acc;
                    } else {
                        rep_phase = PHASE_REST;
                    }
                } else if (now - concentric_start_time > PHASE_TIMEOUT) {
                    rep_phase = PHASE_REST;
                }
                break;
                
            case PHASE_ECCENTRIC:
                if (acc < valley_acc) valley_acc = acc;
                
                if (acc > NEUTRAL_HIGH) {
                    unsigned long duration = now - eccentric_start_time;
                    if (duration >= MIN_PHASE_MS && valley_acc < THRESHOLD_LOW - 0.005f && (current_concentric_duration + duration) >= 700) {
                        current_eccentric_duration = duration;
                        current_reps++;
                        last_rep_time = now;
                        last_rep_peak_vel = peak_velocity;
                        current_set_rep_velocities.push_back(peak_velocity);
                        
                        float ratio = (float)current_eccentric_duration / (float)current_concentric_duration;
                        bool good_form = (ratio >= 1.2f) || (current_eccentric_duration >= 1500);
                        bool ok_form = (ratio >= 0.8f) || (current_eccentric_duration >= 1000);
                        
                        if (good_form) {
                            bg_color = M5Cardputer.Display.color565(0, 100, 0); // Green
                            rep_beep_type = 1;
                        } else if (ok_form) {
                            bg_color = M5Cardputer.Display.color565(150, 100, 0); // Yellow
                            current_poor_form_reps++;
                            rep_beep_type = 2;
                        } else {
                            bg_color = M5Cardputer.Display.color565(120, 0, 0); // Red
                            current_poor_form_reps++;
                            rep_beep_type = 3;
                        }
                    }
                    rep_phase = PHASE_REST;
                    in_neutral = false;
                } else if (now - eccentric_start_time > PHASE_TIMEOUT) {
                    rep_phase = PHASE_REST;
                    in_neutral = false;
                }
                break;
        }
    } else {
        // --- ECCENTRIC-FIRST WORKFLOW (Lowering first, e.g., squat, bench press, leg press) ---
        switch (rep_phase) {
            case PHASE_REST:
                if (acc >= NEUTRAL_LOW && acc <= NEUTRAL_HIGH) {
                    in_neutral = true;
                }
                if (in_neutral && acc < THRESHOLD_LOW && (now - last_rep_time > COOLDOWN_MS)) {
                    rep_phase = PHASE_ECCENTRIC;
                    eccentric_start_time = now;
                    valley_acc = acc;
                    in_neutral = false;
                    current_velocity = 0;
                    peak_velocity = 0;
                }
                break;
                
            case PHASE_ECCENTRIC:
                if (acc < valley_acc) valley_acc = acc;
                
                if (acc > THRESHOLD_HIGH) {
                    unsigned long duration = now - eccentric_start_time;
                    if (duration >= MIN_PHASE_MS && valley_acc < THRESHOLD_LOW - 0.005f) {
                        rep_phase = PHASE_CONCENTRIC;
                        current_eccentric_duration = duration;
                        concentric_start_time = now;
                        peak_acc = acc;
                    } else {
                        rep_phase = PHASE_REST;
                    }
                } else if (now - eccentric_start_time > PHASE_TIMEOUT) {
                    rep_phase = PHASE_REST;
                }
                break;
                
            case PHASE_CONCENTRIC:
                current_velocity += (acc - 1.0f) * 9.81f * 0.01f;
                if (current_velocity > peak_velocity) peak_velocity = current_velocity;
                if (acc > peak_acc) peak_acc = acc;
                
                if (acc < NEUTRAL_LOW) {
                    unsigned long duration = now - concentric_start_time;
                    if (duration >= MIN_PHASE_MS && peak_acc > THRESHOLD_HIGH + 0.005f && (current_eccentric_duration + duration) >= 700) {
                        current_concentric_duration = duration;
                        current_reps++;
                        last_rep_time = now;
                        last_rep_peak_vel = peak_velocity;
                        current_set_rep_velocities.push_back(peak_velocity);
                        
                        float ratio = (float)current_eccentric_duration / (float)current_concentric_duration;
                        bool good_form = (ratio >= 1.2f) || (current_eccentric_duration >= 1500);
                        bool ok_form = (ratio >= 0.8f) || (current_eccentric_duration >= 1000);
                        
                        if (good_form) {
                            bg_color = M5Cardputer.Display.color565(0, 100, 0); // Green
                            rep_beep_type = 1;
                        } else if (ok_form) {
                            bg_color = M5Cardputer.Display.color565(150, 100, 0); // Yellow
                            current_poor_form_reps++;
                            rep_beep_type = 2;
                        } else {
                            bg_color = M5Cardputer.Display.color565(120, 0, 0); // Red
                            current_poor_form_reps++;
                            rep_beep_type = 3;
                        }
                    }
                    rep_phase = PHASE_REST;
                    in_neutral = false;
                } else if (now - concentric_start_time > PHASE_TIMEOUT) {
                    rep_phase = PHASE_REST;
                    in_neutral = false;
                }
                break;
        }
    }
}

// --- IMU FREE-RTOS TASK (CORE 0) ---
TaskHandle_t IMUTaskHandle;

void imuTask(void *pvParameters) {
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(10); // 100Hz exact timing
    xLastWakeTime = xTaskGetTickCount();

    for(;;) {
        updateIMU();
        
        if (workout_state == STATE_ACTIVE) {
            processRepetition();
        }
        
        // Non-blocking delay, guarantees exact 10ms execution loop
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

// --- WI-FI MANAGER SUPPORTING HOME STATION (STA) & ACCESS POINT (AP) CONCURRENCY ---
void startWiFi() {
    WiFi.disconnect(true);
    
    String ssid = "";
    String pass = "";
    if (LittleFS.exists("/wifi_config.json")) {
        File f = LittleFS.open("/wifi_config.json", FILE_READ);
        if (f) {
            JsonDocument doc;
            if (deserializeJson(doc, f) == DeserializationError::Ok) {
                ssid = doc["ssid"] | "";
                pass = doc["pass"] | "";
            }
            f.close();
        }
    }
    
    bool connected_sta = false;
    if (ssid.length() > 0) {
        WiFi.mode(WIFI_AP_STA);
        Serial.print("Connecting to Home Wi-Fi: ");
        Serial.println(ssid);
        WiFi.begin(ssid.c_str(), pass.c_str());
        
        // Wait up to 10 seconds for connection
        unsigned long start_attempt = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start_attempt < 10000) {
            delay(100);
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            Serial.print("Connected! IP: ");
            Serial.println(WiFi.localIP());
            connected_sta = true;
        } else {
            Serial.println("Connection failed. Starting pure Access Point mode...");
        }
    }
    
    if (!connected_sta) {
        WiFi.mode(WIFI_AP);
    }
    
    // Explicitly configure IP for the Access Point to ensure mobile routing stability
    IPAddress local_IP(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(local_IP, gateway, subnet);
    
    WiFi.softAP(AP_SSID, AP_PASS);
    
    // Start DNS server for captive portal
    dnsServer.start(DNS_PORT, "*", local_IP);
    
    server.begin();
    wifi_enabled = true;
    
    // Save wifi state
    JsonDocument doc;
    if (LittleFS.exists("/wifi_config.json")) {
        File f = LittleFS.open("/wifi_config.json", FILE_READ);
        if (f) {
            deserializeJson(doc, f);
            f.close();
        }
    }
    doc["wifi_on_boot"] = true;
    File f = LittleFS.open("/wifi_config.json", "w");
    if (f) {
        serializeJson(doc, f);
        f.close();
    }
}

void stopWiFi() {
    dnsServer.stop();
    server.end();
    WiFi.disconnect(true);
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    wifi_enabled = false;
    
    // Save wifi state
    JsonDocument doc;
    if (LittleFS.exists("/wifi_config.json")) {
        File f = LittleFS.open("/wifi_config.json", FILE_READ);
        if (f) {
            deserializeJson(doc, f);
            f.close();
        }
    }
    doc["wifi_on_boot"] = false;
    File f = LittleFS.open("/wifi_config.json", "w");
    if (f) {
        serializeJson(doc, f);
        f.close();
    }
}

// --- SETUP & LOOP (CORE 1) ---
void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);
    
    // Initialize WS2812 RGB LED
    ledPixel.begin();
    ledPixel.setBrightness(40);
    ledPixel.setPixelColor(0, ledPixel.Color(0, 0, 0));
    ledPixel.show();
    
    // Configure speaker volume for rest timer
    M5Cardputer.Speaker.setVolume(128);
    
    // Set proper display rotation
    M5Cardputer.Display.setRotation(1);
    
    canvas.createSprite(240, 135);
    
    // Initialize LittleFS for workout data storage
    if(!LittleFS.begin(true)){
        Serial.println("LittleFS Mount Failed");
    }
    
    // Initialize SPI for M5Cardputer SD card slot
    SPI.begin(40, 39, 14, 12);
    if (SD.begin(12, SPI, 25000000)) {
        sd_available = true;
        Serial.println("SD Card mounted successfully!");
    } else {
        sd_available = false;
        Serial.println("SD Card mount failed or not present. Using LittleFS fallback.");
    }
    
    loadDataFiles();
    loadRoutines();
    loadCustomExercises();
    
    is_eccentric_first = determineIfEccentricFirst(active_exercise);
    
    // Initialize IMU
    M5.Imu.begin();
    
    // Wi-Fi is OFF by default to save power. Can be toggled with 'W' key.
    WiFi.mode(WIFI_OFF);
    
    // Setup Web Server
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/html", index_html);
    });
    
    // Catch-all for captive portal
    server.onNotFound([](AsyncWebServerRequest *request){
        request->redirect("http://192.168.4.1/");
    });
    
    server.on("/time", HTTP_POST, [](AsyncWebServerRequest *request){
        if (request->hasParam("ts")) {
            long long ts_ms = request->getParam("ts")->value().toDouble();
            struct timeval tv;
            tv.tv_sec = ts_ms / 1000;
            tv.tv_usec = (ts_ms % 1000) * 1000;
            settimeofday(&tv, NULL);
            is_time_synced = true;
        }
        request->send(200);
    });

    server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request){
        if (getWorkoutLogFS().exists("/workout_log.json")) {
            File f = getWorkoutLogFS().open("/workout_log.json", FILE_READ);
            if (f) {
                String resp = "[";
                resp.reserve(8192); // Pre-allocate memory to avoid OOM crashes from fragmentation
                bool first = true;
                while (f.available()) {
                    String line = f.readStringUntil('\n');
                    line.trim();
                    if (line.length() > 2) {
                        JsonDocument doc;
                        if (deserializeJson(doc, line) == DeserializationError::Ok) {
                            unsigned long long sess_t = doc["t"] | 0ULL;
                            if (doc["exercises"].is<JsonArray>()) {
                                JsonArray exercises = doc["exercises"].as<JsonArray>();
                                for (JsonObject exObj : exercises) {
                                    String exName = exObj["ex"] | "";
                                    if (exObj["sets"].is<JsonArray>()) {
                                        JsonArray sets = exObj["sets"].as<JsonArray>();
                                        for (JsonObject sObj : sets) {
                                            if (!first) resp += ",";
                                            sObj["ex"] = exName;
                                            sObj["t"] = sess_t;
                                            String setStr;
                                            serializeJson(sObj, setStr);
                                            resp += setStr;
                                            first = false;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                f.close();
                resp += "]";
                request->send(200, "application/json", resp);
                return;
            }
        }
        request->send(200, "application/json", "[]");
    });
    
    server.on("/heartrate", HTTP_GET, [](AsyncWebServerRequest *request){
        String json = "{\"hr\":" + String(current_heart_rate) + ",\"connected\":" + (connected ? "true" : "false") + "}";
        request->send(200, "application/json", json);
    });

    server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request){
        String json = "{\"hr\":" + String(current_heart_rate) + 
                      ",\"connected\":" + (connected ? "true" : "false") + 
                      ",\"muted\":" + (sound_muted ? "true" : "false") + 
                      ",\"battery\":" + String(M5.Power.getBatteryLevel()) + "}";
        request->send(200, "application/json", json);
    });

    server.on("/mute", HTTP_POST, [](AsyncWebServerRequest *request){
        if (request->hasParam("muted", true)) {
            String val = request->getParam("muted", true)->value();
            sound_muted = (val == "true" || val == "1");
            saveSoundConfig();
        } else {
            sound_muted = !sound_muted;
            saveSoundConfig();
        }
        String json = "{\"muted\":" + String(sound_muted ? "true" : "false") + "}";
        request->send(200, "application/json", json);
    });
    
    server.on("/wifi_get", HTTP_GET, [](AsyncWebServerRequest *request){
        String ssid = "";
        bool has_pass = false;
        if (LittleFS.exists("/wifi_config.json")) {
            File f = LittleFS.open("/wifi_config.json", FILE_READ);
            if (f) {
                JsonDocument doc;
                if (deserializeJson(doc, f) == DeserializationError::Ok) {
                    ssid = doc["ssid"] | "";
                    String pass = doc["pass"] | "";
                    if (pass.length() > 0) has_pass = true;
                }
                f.close();
            }
        }
        String json = "{\"ssid\":\"" + ssid + "\",\"has_pass\":" + (has_pass ? "true" : "false") + "}";
        request->send(200, "application/json", json);
    });
    
    server.on("/wifi_save", HTTP_POST, [](AsyncWebServerRequest *request){
        String ssid = "";
        String pass = "";
        if (request->hasParam("ssid", true)) {
            ssid = request->getParam("ssid", true)->value();
        }
        if (request->hasParam("pass", true)) {
            pass = request->getParam("pass", true)->value();
        }
        
        if (ssid.length() > 0) {
            JsonDocument doc;
            doc["ssid"] = ssid;
            if (pass.length() == 0 && LittleFS.exists("/wifi_config.json")) {
                File f = LittleFS.open("/wifi_config.json", FILE_READ);
                if (f) {
                    JsonDocument oldDoc;
                    if (deserializeJson(oldDoc, f) == DeserializationError::Ok) {
                        pass = oldDoc["pass"] | "";
                    }
                    f.close();
                }
            }
            doc["pass"] = pass;
            doc["wifi_on_boot"] = true;
            
            File f = LittleFS.open("/wifi_config.json", "w");
            if (f) {
                serializeJson(doc, f);
                f.close();
            }
            request->send(200, "text/plain", "OK");
            
            request->onDisconnect([](){
                vTaskDelay(pdMS_TO_TICKS(1500));
                ESP.restart();
            });
        } else {
            request->send(400, "text/plain", "Missing SSID");
        }
    });
    
    // CSV Export endpoint
    server.on("/export", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!getWorkoutLogFS().exists("/workout_log.json")) {
            request->send(200, "text/plain", "No data");
            return;
        }
        File f = getWorkoutLogFS().open("/workout_log.json", FILE_READ);
        if (!f) { request->send(500, "text/plain", "Error"); return; }
        
        String csv = "Exercise,Weight_kg,Reps,Volume_kg,Poor_Form,Set,Est_1RM,HR_Avg,HR_Max,HR_Series,HR_Rec_Avg,HR_Rec_Max,HR_Rec_Series,Rep_Velocities,Time_ms\n";
        csv.reserve(16384); // Pre-allocate memory to avoid heap fragmentation and OOM crashes
        while (f.available()) {
            String line = f.readStringUntil('\n');
            line.trim();
            if (line.length() < 3) continue;
            JsonDocument doc;
            if (deserializeJson(doc, line) == DeserializationError::Ok) {
                unsigned long long sess_t = doc["t"] | 0ULL;
                if (doc["exercises"].is<JsonArray>()) {
                    JsonArray exercises = doc["exercises"].as<JsonArray>();
                    for (JsonObject exObj : exercises) {
                        String exName = exObj["ex"] | "";
                        if (exObj["sets"].is<JsonArray>()) {
                            JsonArray sets = exObj["sets"].as<JsonArray>();
                            for (JsonObject sObj : sets) {
                                csv += exName + ",";
                                csv += String((int)(sObj["w"] | 0)) + ",";
                                csv += String((int)(sObj["r"] | 0)) + ",";
                                csv += String((int)(sObj["v"] | 0)) + ",";
                                csv += String((int)(sObj["pf"] | 0)) + ",";
                                csv += String((int)(sObj["s"] | 0)) + ",";
                                csv += String((int)(sObj["1rm"] | 0)) + ",";
                                csv += String((int)(sObj["hr_avg"] | 0)) + ",";
                                csv += String((int)(sObj["hr_max"] | 0)) + ",";
                                
                                String seriesStr = "";
                                if (sObj["hr_series"].is<JsonArray>()) {
                                    JsonArray arr = sObj["hr_series"].as<JsonArray>();
                                    for (size_t i = 0; i < arr.size(); i++) {
                                        if (i > 0) seriesStr += ";";
                                        seriesStr += String((int)arr[i]);
                                    }
                                }
                                csv += "\"" + seriesStr + "\",";
                                
                                csv += String((int)(sObj["hr_rec_avg"] | 0)) + ",";
                                csv += String((int)(sObj["hr_rec_max"] | 0)) + ",";
                                
                                String recSeriesStr = "";
                                if (sObj["hr_rec_series"].is<JsonArray>()) {
                                    JsonArray arr = sObj["hr_rec_series"].as<JsonArray>();
                                    for (size_t i = 0; i < arr.size(); i++) {
                                        if (i > 0) recSeriesStr += ";";
                                        recSeriesStr += String((int)arr[i]);
                                    }
                                }
                                csv += "\"" + recSeriesStr + "\",";
                                
                                String velSeriesStr = "";
                                if (sObj["rep_vel"].is<JsonArray>()) {
                                    JsonArray arr = sObj["rep_vel"].as<JsonArray>();
                                    for (size_t i = 0; i < arr.size(); i++) {
                                        if (i > 0) velSeriesStr += ";";
                                        velSeriesStr += String((float)arr[i], 2);
                                    }
                                }
                                csv += "\"" + velSeriesStr + "\",";
                                
                                csv += String(sess_t) + "\n";
                            }
                        }
                    }
                }
            }
        }
        f.close();
        
        AsyncWebServerResponse *response = request->beginResponse(200, "text/csv", csv);
        response->addHeader("Content-Disposition", "attachment; filename=workout_log.csv");
        request->send(response);
    });
    
    session_start_time = millis();
    
    // Initialize BLE client device
    BLEDevice::init("CardputerGym");
    
    // Pin BLE Task to Core 1 so it doesn't conflict with IMU task on Core 0
    xTaskCreatePinnedToCore(
        bleTask,          // Task function
        "BLE_Task",       // Task name
        4096,             // Stack size (words)
        NULL,             // Task parameters
        1,                // Priority (Lower than IMU, runs background communications)
        &BLETaskHandle,   // Task handle
        1                 // Core ID (1)
    );
    
    // Pin IMU Task to Core 0 (App Core) to isolate it from UI and File I/O delays
    xTaskCreatePinnedToCore(
        imuTask,          // Task function
        "IMU_Task",       // Task name
        4096,             // Stack size (words)
        NULL,             // Task parameters
        2,                // Priority (Higher than default UI task)
        &IMUTaskHandle,   // Task handle
        0                 // Core ID (0)
    );
    
    // Check if Wi-Fi should be enabled on boot
    bool wifiOnBoot = false;
    if (LittleFS.exists("/wifi_config.json")) {
        File f = LittleFS.open("/wifi_config.json", FILE_READ);
        if (f) {
            JsonDocument doc;
            if (deserializeJson(doc, f) == DeserializationError::Ok) {
                wifiOnBoot = doc["wifi_on_boot"] | false;
            }
            f.close();
        }
    }
    if (wifiOnBoot) {
        startWiFi();
    }
}

void loop() {
    if (wifi_enabled) {
        dnsServer.processNextRequest();
    }
    M5Cardputer.update();
    
    if (M5Cardputer.Keyboard.isPressed()) {
        last_key_activity_time = millis();
        if (current_backlight_brightness != 160) {
            current_backlight_brightness = 160;
            M5Cardputer.Display.setBrightness(current_backlight_brightness);
        }
        // Dismiss Wi-Fi QR overlay on any keypress (except W which toggles)
        if (show_wifi_qr) {
            // Will be handled in key processing below, but for non-character keys:
            show_wifi_qr = false;
        }
    }
    
    // Handle Input
    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
        auto status = M5Cardputer.Keyboard.keysState();
        
        if (status.enter) {
            if (creating_custom_exercise) {
                if (custom_exercise_input_str.length() > 0) {
                    custom_exercise_input_str.trim();
                    bool exists = false;
                    for (const auto &ex : custom_exercises) {
                        if (ex.equalsIgnoreCase(custom_exercise_input_str)) {
                            exists = true;
                            break;
                        }
                    }
                    if (!exists) {
                        custom_exercises.push_back(custom_exercise_input_str);
                        saveCustomExercises();
                    }
                    creating_custom_exercise = false;
                    menu_muscle_idx = 10; // Select Custom group
                    selecting_muscle = false;
                    buildExerciseList(menu_muscle_idx);
                    // Find the newly created exercise in the list to select it
                    for (int k = 0; k < current_ex_count; k++) {
                        if (getExerciseName(menu_muscle_idx, current_ex_list[k]) == custom_exercise_input_str) {
                            menu_exercise_idx = k;
                            break;
                        }
                    }
                } else {
                    creating_custom_exercise = false;
                }
            } else if (editing_weight) {
                editing_weight = false;
                if (weight_input_str.length() > 0) {
                    current_weight = weight_input_str.toInt();
                }
            } else if (current_view == VIEW_STATS) {
                stats_show_graph = !stats_show_graph;
            } else if (current_view == VIEW_WORKOUT) {
                if (workout_state == STATE_READY) {
                    if (routine_active && !routine_exercising) {
                        routine_exercising = true;
                        int ex_idx = routine_selected_ex_idx;
                        active_exercise = String(routines[active_routine_idx].exercises[ex_idx].name);
                        current_weight = last_weights_doc[active_exercise] | 20;
                        rest_target_sec = routines[active_routine_idx].exercises[ex_idx].target_rest;
                        current_set_number = routines[active_routine_idx].exercises[ex_idx].completed_sets;
                    } else {
                        workout_state = STATE_ACTIVE;
                        is_eccentric_first = determineIfEccentricFirst(active_exercise);
                        current_reps = 0;
                        current_poor_form_reps = 0;
                        current_set_rep_velocities.clear();
                        bg_color = BLACK;
                        set_start_time = millis();
                        
                        // Reset heart rate trackers for the new active set
                        sum_hr_active = 0;
                        count_hr_active = 0;
                        max_hr_active = 0;
                        hr_active_series.clear();
                        
                        // Sync set counter and 1RM if exercise changed
                        if (active_exercise != prev_exercise) {
                            current_set_number = getCompletedSetsCount(active_exercise);
                            
                            // Restore max 1RM for the new exercise from active session if present
                            estimated_1rm = 0;
                            for (const auto &ex : active_session.exercises) {
                                if (ex.name == active_exercise) {
                                    for (const auto &s : ex.sets) {
                                        if (s.est_1rm > estimated_1rm) {
                                            estimated_1rm = s.est_1rm;
                                        }
                                    }
                                    break;
                                }
                            }
                            prev_exercise = active_exercise;
                        }
                    }
                } else if (workout_state == STATE_ACTIVE) {
                    if (editing_reps) {
                        editing_reps = false;
                        if (reps_input_str.length() > 0) {
                            current_reps = reps_input_str.toInt();
                        }
                    }
                    workout_state = STATE_SUMMARY;
                    bg_color = BLACK;
                    rest_start_time = millis();
                    hr_recovery_series.clear(); // Clear recovery buffer for the new rest period
                } else if (workout_state == STATE_SUMMARY) {
                    saveSet(); // Save set WITH full recovery heart rate history!
                    if (routine_active) {
                        int ex_idx = routine_selected_ex_idx;
                        routines[active_routine_idx].exercises[ex_idx].completed_sets++;
                        routine_exercising = false; // return to routine exercise list
                    }
                    workout_state = STATE_READY;
                    bg_color = BLACK;
                }
            } else if (current_view == VIEW_EXERCISES) {
                if (search_query.length() > 0) {
                    if (search_count > 0) {
                        active_exercise = getExerciseName(search_results[menu_exercise_idx].m, search_results[menu_exercise_idx].e);
                        current_weight = last_weights_doc[active_exercise] | 20;
                        current_view = VIEW_WORKOUT;
                        bg_color = BLACK;
                        search_query = "";
                    }
                } else if (selecting_muscle) {
                    selecting_muscle = false;
                    menu_exercise_idx = 0;
                    menu_scroll_offset = 0;
                    buildExerciseList(menu_muscle_idx);
                } else {
                    int real_idx = current_ex_list[menu_exercise_idx];
                    active_exercise = getExerciseName(menu_muscle_idx, real_idx);
                    current_weight = last_weights_doc[active_exercise] | 20;
                    current_view = VIEW_WORKOUT;
                    bg_color = BLACK;
                }
            } else if (current_view == VIEW_ROUTINES) {
                if (routine_subview == ROUTINE_LIST) {
                    if (routine_confirm_delete) {
                        if (routines_count > 0) {
                            for (int k = selected_routine_idx; k < routines_count - 1; k++) {
                                routines[k] = routines[k + 1];
                            }
                            routines_count--;
                            if (selected_routine_idx >= routines_count) {
                                selected_routine_idx = max(0, routines_count - 1);
                            }
                            saveRoutines();
                        }
                        routine_confirm_delete = false;
                    } else {
                        if (routines_count > 0) {
                            routine_active = true;
                            active_routine_idx = selected_routine_idx;
                            routine_selected_ex_idx = 0;
                            routine_exercising = false;
                            for (int k = 0; k < routines[active_routine_idx].exercise_count; k++) {
                                routines[active_routine_idx].exercises[k].completed_sets = 0;
                            }
                            current_view = VIEW_WORKOUT;
                            bg_color = BLACK;
                        }
                    }
                } else if (routine_subview == ROUTINE_PICK_EXERCISE) {
                    if (selecting_muscle) {
                        selecting_muscle = false;
                        menu_exercise_idx = 0;
                    } else {
                        if (temp_routine.exercise_count < 10) {
                            int r_idx = temp_routine.exercise_count;
                            String exName = getExerciseName(menu_muscle_idx, menu_exercise_idx);
                            strncpy(temp_routine.exercises[r_idx].name, exName.c_str(), 31);
                            temp_routine.exercises[r_idx].target_sets = 4;
                            temp_routine.exercises[r_idx].target_reps = 10;
                            temp_routine.exercises[r_idx].target_rest = 90;
                            temp_routine.exercises[r_idx].completed_sets = 0;
                            temp_routine.exercise_count++;
                        }
                        routine_subview = ROUTINE_CREATOR;
                    }
                } else if (routine_subview == ROUTINE_EDIT_ITEM) {
                    RoutineExercise &ex = temp_routine.exercises[creator_selected_idx];
                    if (edit_field_idx == 2) {
                        int val = parseRestTimerInput(edit_input_str);
                        if (val > 0) ex.target_rest = val;
                    } else {
                        int val = edit_input_str.toInt();
                        if (val > 0) {
                            if (edit_field_idx == 0) ex.target_sets = val;
                            else if (edit_field_idx == 1) ex.target_reps = val;
                        }
                    }
                    routine_subview = ROUTINE_CREATOR;
                }
            } else if (current_view == VIEW_CARDIO) {
                show_live_curve = !show_live_curve;
            } else if (current_view == VIEW_HISTORY) {
                if (history_confirm_delete) {
                    int total = getHistoryFileLinesCount();
                    if (total > 0 && history_selected_idx >= 0 && history_selected_idx < history_count) {
                        int start = (total > 20) ? total - 20 : 0;
                        int count = (total > 20) ? 20 : total;
                        int fileIdx = start + (count - 1 - history_selected_idx);
                        deleteHistoryEntry(fileIdx);
                        loadHistory();
                        history_confirm_delete = false;
                        if (history_selected_idx >= history_count) {
                            history_selected_idx = max(0, history_count - 1);
                        }
                    }
                } else {
                    if (history_count > 0 && history_selected_idx >= 0 && history_selected_idx < history_count) {
                        current_view = VIEW_HISTORY_DETAIL;
                    }
                }
            }
        } else if (status.space) {
            if (creating_custom_exercise) {
                if (custom_exercise_input_str.length() < 31) {
                    custom_exercise_input_str += " ";
                }
            } else if (current_view == VIEW_EXERCISES && !selecting_muscle && search_query.length() == 0) {
                int real_idx = current_ex_list[menu_exercise_idx];
                is_favorite[menu_muscle_idx][real_idx] = !is_favorite[menu_muscle_idx][real_idx];
                saveFavorites();
                buildExerciseList(menu_muscle_idx); // Refresh list to apply new order
                menu_exercise_idx = 0;
                menu_scroll_offset = 0;
            } else if (current_view == VIEW_EXERCISES && search_query.length() > 0) {
                search_query += " ";
                doSearch();
                menu_exercise_idx = 0;
                menu_scroll_offset = 0;
            }
        } else {
            // Handle other keys
            for (auto i : status.word) {
                if (creating_custom_exercise) {
                    if (i >= 32 && i <= 126) {
                        if (custom_exercise_input_str.length() < 31) {
                            custom_exercise_input_str += (char)i;
                        }
                    }
                } else if (editing_weight) {
                    if (isDigit(i)) {
                        weight_input_str += i;
                    }
                } else if (editing_reps) {
                    if (isDigit(i)) {
                        reps_input_str += i;
                    }
                } else if (current_view == VIEW_ROUTINES && routine_subview == ROUTINE_EDIT_ITEM) {
                    if (isDigit(i)) {
                        edit_input_str += i;
                    }
                } else {
                    if (current_view == VIEW_WORKOUT && workout_state == STATE_ACTIVE) {
                        if (isDigit(i)) {
                            editing_reps = true;
                            reps_input_str = String((char)i);
                            continue;
                        }
                    }
                    
                    if (i == '1') {
                        current_view = VIEW_WORKOUT;
                        bg_color = BLACK;
                        history_confirm_delete = false;
                        routine_confirm_delete = false;
                    } else if (i == '2') {
                        current_view = VIEW_STATS;
                        bg_color = BLACK;
                        history_confirm_delete = false;
                        routine_confirm_delete = false;
                        loadStatsGraphData();
                    } else if (i == '7') {
                        current_view = VIEW_CARDIO;
                        bg_color = BLACK;
                        history_confirm_delete = false;
                        routine_confirm_delete = false;
                    } else if (i == '3') {
                        current_view = VIEW_EXERCISES;
                        selecting_muscle = true;
                        menu_scroll_offset = 0;
                        search_query = "";
                        bg_color = BLACK;
                        history_confirm_delete = false;
                        routine_confirm_delete = false;
                    } else if (i == '4') {
                        current_view = VIEW_HISTORY;
                        bg_color = BLACK;
                        loadHistory();
                        history_confirm_delete = false;
                        routine_confirm_delete = false;
                    } else if (i == '5') {
                        current_view = VIEW_PR;
                        bg_color = BLACK;
                        history_confirm_delete = false;
                        routine_confirm_delete = false;
                    } else if (i == '6') {
                        current_view = VIEW_ROUTINES;
                        bg_color = BLACK;
                        loadRoutines();
                        history_confirm_delete = false;
                        routine_confirm_delete = false;
                    } else if (i == 'h' || i == 'H') {
                        if (current_view == VIEW_HELP) {
                            current_view = VIEW_WORKOUT;
                        } else {
                            current_view = VIEW_HELP;
                            help_selected_idx = 0;
                            help_scroll_offset = 0;
                        }
                        bg_color = BLACK;
                        history_confirm_delete = false;
                        routine_confirm_delete = false;
                    } else if (i == 'w' || i == 'W') {
                        if (show_wifi_qr) {
                            show_wifi_qr = false;
                        } else if (!wifi_enabled) {
                            startWiFi();
                            show_wifi_qr = true;
                        } else {
                            stopWiFi();
                        }
                    } else if (i == 'b' || i == 'B') {
                        ble_enabled = !ble_enabled;
                        if (!ble_enabled) {
                            disconnectBLE();
                        } else {
                            doScan = true;
                        }
                    } else if (i == 'm' || i == 'M') {
                        sound_muted = !sound_muted;
                        saveSoundConfig();
                    } else if (current_view == VIEW_WORKOUT && workout_state == STATE_READY) {
                        if (i == 'e' || i == 'E') {
                            editing_weight = true;
                            weight_input_str = String(current_weight);
                        }
                    } else if (current_view == VIEW_EXERCISES) {
                        if ((i == 'n' || i == 'N') && search_query.length() == 0) {
                            creating_custom_exercise = true;
                            custom_exercise_input_str = "";
                        } else if ((i >= 'a' && i <= 'z') || (i >= 'A' && i <= 'Z')) {
                            search_query += (char)i;
                            doSearch();
                            menu_exercise_idx = 0;
                            menu_scroll_offset = 0;
                        }
                    }
                }
                
                if (current_view == VIEW_WORKOUT) {
                    if (workout_state == STATE_READY) {
                        if (i == 'q' || i == 'Q') {
                            if (routine_active) routine_active = false;
                            finishSession();
                            continue;
                        }
                        if (routine_active && !routine_exercising) {
                            if (i == ';' || i == ',') { // UP
                                if (routine_selected_ex_idx > 0) routine_selected_ex_idx--;
                            }
                            if (i == '.' || i == '/') { // DOWN
                                if (routine_selected_ex_idx < routines[active_routine_idx].exercise_count - 1) routine_selected_ex_idx++;
                            }
                        } else {
                            if (i == '-' || i == '_' || i == '.' || i == ',') {
                                if (current_weight >= 5) current_weight -= 5;
                            }
                            if (i == '=' || i == '+' || i == ';' || i == '/') {
                                current_weight += 5;
                            }
                            if (i == 't' || i == 'T') {
                                rest_target_idx = (rest_target_idx + 1) % NUM_REST_PRESETS;
                                rest_target_sec = REST_PRESETS[rest_target_idx];
                            }
                        }
                    }
                } else if (current_view == VIEW_EXERCISES && !creating_custom_exercise) {
                    if (i == ';' || i == ',') { // UP
                        if (search_query.length() > 0) {
                            if (menu_exercise_idx > 0) menu_exercise_idx--;
                        } else if (selecting_muscle) {
                            if (menu_muscle_idx > 0) menu_muscle_idx--;
                        } else {
                            if (menu_exercise_idx > 0) menu_exercise_idx--;
                        }
                    }
                    if (i == '.' || i == '/') { // DOWN
                        if (search_query.length() > 0) {
                            if (menu_exercise_idx < search_count - 1) menu_exercise_idx++;
                        } else if (selecting_muscle) {
                            if (menu_muscle_idx < NUM_MUSCLES - 1) menu_muscle_idx++;
                        } else {
                            if (menu_exercise_idx < current_ex_count - 1) menu_exercise_idx++;
                        }
                    }
                } else if (current_view == VIEW_PR) {
                    if (i == ';' || i == ',') { if (pr_scroll > 0) pr_scroll--; }
                    if (i == '.' || i == '/') { if (pr_scroll < (int)pr_doc.size() - 5) pr_scroll++; }
                } else if (current_view == VIEW_HISTORY) {
                    if (history_confirm_delete) {
                        // locked
                    } else {
                        if (i == ';' || i == ',') { // UP
                            if (history_selected_idx > 0) {
                                history_selected_idx--;
                                if (history_selected_idx < history_scroll) {
                                    history_scroll = history_selected_idx;
                                }
                            }
                        }
                        if (i == '.' || i == '/') { // DOWN
                            if (history_selected_idx < history_count - 1) {
                                history_selected_idx++;
                                if (history_selected_idx >= history_scroll + 3) { // 3 is VIS in VIEW_HISTORY
                                    history_scroll = history_selected_idx - 3 + 1;
                                }
                            }
                        }
                        if (i == 'r' || i == 'R') {
                            loadHistory();
                            history_selected_idx = 0;
                            history_scroll = 0;
                        }
                        if (i == 'd' || i == 'D') {
                            if (history_count > 0) {
                                history_confirm_delete = true;
                            }
                        }
                    }
                } else if (current_view == VIEW_HISTORY_DETAIL) {
                    SessionLog &sess = history_sessions[history_selected_idx];
                    if (!sess.exercises.empty()) {
                        if (i == ';' || i == ',') { // UP - scroll sets
                            if (menu_exercise_idx > 0) {
                                menu_exercise_idx--;
                            }
                        }
                        if (i == '.' || i == '/') { // DOWN - scroll sets
                            if (menu_exercise_idx < (int)sess.exercises[detail_selected_ex_idx].sets.size() - 1) {
                                menu_exercise_idx++;
                            }
                        }
                        if (i == '[' || i == '{') { // LEFT - previous exercise
                            if (detail_selected_ex_idx > 0) {
                                detail_selected_ex_idx--;
                                menu_exercise_idx = 0;
                                menu_scroll_offset = 0;
                            }
                        }
                        if (i == ']' || i == '}') { // RIGHT - next exercise
                            if (detail_selected_ex_idx < (int)sess.exercises.size() - 1) {
                                detail_selected_ex_idx++;
                                menu_exercise_idx = 0;
                                menu_scroll_offset = 0;
                            }
                        }
                    }
                } else if (current_view == VIEW_ROUTINES) {
                    if (routine_subview == ROUTINE_LIST) {
                        if (i == 'n' || i == 'N') {
                            if (routines_count < 5) {
                                routine_subview = ROUTINE_CREATOR;
                                creator_selected_idx = 0;
                                temp_routine.exercise_count = 0;
                                selected_routine_idx = routines_count;
                                String defaultName = "Routine " + String(routines_count + 1);
                                strncpy(temp_routine.name, defaultName.c_str(), 19);
                            }
                        } else if (i == 'd' || i == 'D') {
                            if (routines_count > 0) {
                                routine_confirm_delete = true;
                            }
                        } else if (i == ';' || i == ',') { // UP
                            if (selected_routine_idx > 0) selected_routine_idx--;
                        } else if (i == '.' || i == '/') { // DOWN
                            if (selected_routine_idx < routines_count - 1) selected_routine_idx++;
                        }
                    } else if (routine_subview == ROUTINE_CREATOR) {
                        if (i == 'a' || i == 'A') {
                            routine_subview = ROUTINE_PICK_EXERCISE;
                            selecting_muscle = true;
                            menu_muscle_idx = 0;
                            menu_exercise_idx = 0;
                            menu_scroll_offset = 0;
                        } else if (i == 'd' || i == 'D') {
                            if (temp_routine.exercise_count > 0) {
                                for (int k = creator_selected_idx; k < temp_routine.exercise_count - 1; k++) {
                                    temp_routine.exercises[k] = temp_routine.exercises[k + 1];
                                }
                                temp_routine.exercise_count--;
                                if (creator_selected_idx >= temp_routine.exercise_count) {
                                    creator_selected_idx = max(0, temp_routine.exercise_count - 1);
                                }
                            }
                        } else if (i == 'e' || i == 'E') {
                            if (temp_routine.exercise_count > 0) {
                                routine_subview = ROUTINE_EDIT_ITEM;
                                edit_field_idx = 0;
                                backup_exercise = temp_routine.exercises[creator_selected_idx];
                                edit_input_str = String(backup_exercise.target_sets);
                            }
                        } else if (i == 's' || i == 'S') {
                            if (selected_routine_idx == routines_count) {
                                routines[selected_routine_idx] = temp_routine;
                                routines_count++;
                            } else {
                                routines[selected_routine_idx] = temp_routine;
                            }
                            saveRoutines();
                            routine_subview = ROUTINE_LIST;
                        } else if (i == ';' || i == ',') { // UP
                            if (creator_selected_idx > 0) creator_selected_idx--;
                        } else if (i == '.' || i == '/') { // DOWN
                            if (creator_selected_idx < temp_routine.exercise_count - 1) creator_selected_idx++;
                        }
                    } else if (routine_subview == ROUTINE_PICK_EXERCISE) {
                        if (i == ';' || i == ',') { // UP
                            if (selecting_muscle) {
                                if (menu_muscle_idx > 0) menu_muscle_idx--;
                            } else {
                                if (menu_exercise_idx > 0) menu_exercise_idx--;
                            }
                        } else if (i == '.' || i == '/') { // DOWN
                            if (selecting_muscle) {
                                if (menu_muscle_idx < NUM_MUSCLES - 1) menu_muscle_idx++;
                            } else {
                                int maxEx = ex_count[menu_muscle_idx];
                                if (menu_exercise_idx < maxEx - 1) menu_exercise_idx++;
                            }
                        }
                    } else if (routine_subview == ROUTINE_EDIT_ITEM) {
                        RoutineExercise &ex = temp_routine.exercises[creator_selected_idx];
                        if (i == ';' || i == ',') { // UP
                            if (edit_field_idx > 0) {
                                if (edit_field_idx == 2) {
                                    int val = parseRestTimerInput(edit_input_str);
                                    if (val > 0) ex.target_rest = val;
                                } else {
                                    int val = edit_input_str.toInt();
                                    if (val > 0) {
                                        if (edit_field_idx == 0) ex.target_sets = val;
                                        else if (edit_field_idx == 1) ex.target_reps = val;
                                    }
                                }
                                edit_field_idx--;
                                if (edit_field_idx == 0) edit_input_str = String(ex.target_sets);
                                else if (edit_field_idx == 1) edit_input_str = String(ex.target_reps);
                                else if (edit_field_idx == 2) edit_input_str = secondsToTimerInput(ex.target_rest);
                            }
                        } else if (i == '.' || i == '/') { // DOWN
                            if (edit_field_idx < 2) {
                                if (edit_field_idx == 2) {
                                    int val = parseRestTimerInput(edit_input_str);
                                    if (val > 0) ex.target_rest = val;
                                } else {
                                    int val = edit_input_str.toInt();
                                    if (val > 0) {
                                        if (edit_field_idx == 0) ex.target_sets = val;
                                        else if (edit_field_idx == 1) ex.target_reps = val;
                                    }
                                }
                                edit_field_idx++;
                                if (edit_field_idx == 0) edit_input_str = String(ex.target_sets);
                                else if (edit_field_idx == 1) edit_input_str = String(ex.target_reps);
                                else if (edit_field_idx == 2) edit_input_str = secondsToTimerInput(ex.target_rest);
                            }
                        }
                    } else if (current_view == VIEW_HELP) {
                        if (i == ';' || i == ',') { // UP
                            if (help_selected_idx > 0) help_selected_idx--;
                            else help_selected_idx = 13; // Wrap to last
                        }
                        if (i == '.' || i == '/') { // DOWN
                            if (help_selected_idx < 13) help_selected_idx++;
                            else help_selected_idx = 0; // Wrap to first
                        }
                    }
                }
            }
            
            // Handle ESC/Delete key in menu
            if (status.del) {
                if (creating_custom_exercise) {
                    if (custom_exercise_input_str.length() > 0) {
                        custom_exercise_input_str.remove(custom_exercise_input_str.length() - 1);
                    } else {
                        creating_custom_exercise = false;
                    }
                } else if (current_view == VIEW_EXERCISES) {
                    if (search_query.length() > 0) {
                        search_query.remove(search_query.length() - 1);
                        doSearch();
                        menu_exercise_idx = 0;
                        menu_scroll_offset = 0;
                    } else if (!selecting_muscle) {
                        selecting_muscle = true;
                    }
                } else if (current_view == VIEW_ROUTINES) {
                    if (routine_subview == ROUTINE_CREATOR) {
                        routine_subview = ROUTINE_LIST;
                    } else if (routine_subview == ROUTINE_PICK_EXERCISE) {
                        if (!selecting_muscle) {
                            selecting_muscle = true;
                        } else {
                            routine_subview = ROUTINE_CREATOR;
                        }
                    } else if (routine_subview == ROUTINE_EDIT_ITEM) {
                        if (edit_input_str.length() > 0) {
                            edit_input_str.remove(edit_input_str.length() - 1);
                        } else {
                            // Cancel edit and restore original backed-up parameters
                            temp_routine.exercises[creator_selected_idx] = backup_exercise;
                            routine_subview = ROUTINE_CREATOR;
                        }
                    }
                } else if (current_view == VIEW_HISTORY && history_confirm_delete) {
                    history_confirm_delete = false;
                } else if (current_view == VIEW_ROUTINES && routine_confirm_delete) {
                    routine_confirm_delete = false;
                } else if (current_view == VIEW_HISTORY_DETAIL) {
                    current_view = VIEW_HISTORY;
                } else if (editing_weight && weight_input_str.length() > 0) {
                    weight_input_str.remove(weight_input_str.length() - 1);
                } else if (editing_reps && reps_input_str.length() > 0) {
                    reps_input_str.remove(reps_input_str.length() - 1);
                }
            }
        }
    }
    
    // Handle rep beep from Core 0 (Audio & WS2812 Visual VBT feedback)
    if (rep_beep_type > 0) {
        if (rep_beep_type == 1) {
            M5Cardputer.Speaker.tone(2000, 80); // High crisp beep for Good form
            ledPixel.setPixelColor(0, ledPixel.Color(0, 180, 0)); // Green flash
            led_flash_end_time = millis() + 450;
            led_flashing = true;
        } else if (rep_beep_type == 2) {
            M5Cardputer.Speaker.tone(1000, 120); // Medium beep for Ok form
            ledPixel.setPixelColor(0, ledPixel.Color(150, 150, 0)); // Yellow flash
            led_flash_end_time = millis() + 450;
            led_flashing = true;
        } else if (rep_beep_type == 3) {
            M5Cardputer.Speaker.tone(400, 250); // Low, long buzz for Bad form (too fast)
            ledPixel.setPixelColor(0, ledPixel.Color(180, 0, 0)); // Red flash
            led_flash_end_time = millis() + 600;
            led_flashing = true;
        }
        ledPixel.show();
        rep_beep_type = 0;
    }
    
    // Update WS2812 RGB LED states (pulsing rest light, timer indicator)
    updateLED();
    
    // UI Refresh (every ~50ms)
    static unsigned long last_ui_time = 0;
    if (millis() - last_ui_time > 50) {
        drawUI();
        last_ui_time = millis();
    }
}
