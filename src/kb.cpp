// Using https://gist.github.com/MightyPork/6da26e382a7ad91b5496ee55fdc73db2
// https://cdn.sparkfun.com/datasheets/Wireless/Bluetooth/RN-HID-User-Guide-v1.0r.pdf
// https://github.com/astronautr/60p-keyboard-layout

#define US_KEYBOARD 1


#include <Arduino.h>
#include "BLEDevice.h"
#include "BLEHIDDevice.h"
#include "HIDTypes.h"
#include "MightyPork.h"
// #include "HIDKeyboardTypes.h"


#define DEVICE_NAME "Rad Keyboard"

#define KEY_MAP_SIZE 256
#define KEY_LIMIT 6 // Set by Bluetooth HID limit

unsigned long key_last_event_time[KEY_MAP_SIZE] = {0};
int key_event[KEY_MAP_SIZE] = {0};

uint8_t bt_key_map[KEY_LIMIT] = {0};
uint8_t bt_key_map_last[KEY_LIMIT] = {0};

unsigned int bounce_time = 10; //ms
unsigned int tap_expire_time = 500; //ms

// uint8_t key_code_status_map[256] = {0};
#define KEY_FN1 KEY_F13
uint8_t key_code_map[5][13] = {
    { KEY_LEFTCTRL, KEY_LEFTMETA, KEY_LEFTALT, KEY_SPACE, 0, 0, 0, KEY_FN1, KEY_LEFT, KEY_DOWN, KEY_RIGHT, KEY_BACKSLASH, KEY_BACKSPACE },
    // { KEY_LEFTCTRL, KEY_LEFTMETA, KEY_LEFTALT, KEY_SPACE, 0, 0, 0, KEY_RIGHTALT, KEY_RIGHTMETA, KEY_PROPS, KEY_RIGHTCTRL, KEY_BACKSLASH, KEY_BACKSPACE },
    { KEY_LEFTSHIFT, KEY_Z, KEY_X, KEY_C, KEY_V, KEY_B, KEY_N, KEY_M, KEY_COMMA, KEY_DOT, KEY_SLASH, KEY_UP },
    // { KEY_LEFTSHIFT, KEY_Z, KEY_X, KEY_C, KEY_V, KEY_B, KEY_N, KEY_M, KEY_COMMA, KEY_DOT, KEY_SLASH, KEY_RIGHTSHIFT },
    { KEY_GRAVE, KEY_A, KEY_S, KEY_D, KEY_F, KEY_G, KEY_H, KEY_J, KEY_K, KEY_L, KEY_SEMICOLON, KEY_APOSTROPHE, KEY_ENTER },
    // { KEY_CAPSLOCK, KEY_A, KEY_S, KEY_D, KEY_F, KEY_G, KEY_H, KEY_J, KEY_K, KEY_L, KEY_SEMICOLON, KEY_APOSTROPHE, KEY_ENTER },
    { KEY_TAB, KEY_Q, KEY_W, KEY_E, KEY_R, KEY_T, KEY_Y, KEY_U, KEY_I, KEY_O, KEY_P, KEY_LEFTBRACE, KEY_RIGHTBRACE },
    { KEY_ESC, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9, KEY_0, KEY_MINUS, KEY_EQUAL },
};


#define MAX_KEYS_PER_KEYSTROKE 10
// // #define MAX_KEYSTROKE_PER_CHORD 4
// // #define TAPS 8
union keystroke_result {
    uint8_t keys[KEY_LIMIT];
	void (*function)(void);
};
struct keystroke
{
    uint8_t keys[MAX_KEYS_PER_KEYSTROKE];
    union keystroke_result result;
	bool result_is_function;
};

void dump_debug_status() {
    Serial.println("DUMPING STATUS");
}

struct keystroke keystroke_map[] = {
    {{ KEY_FN1, KEY_LEFT }, {KEY_HOME}},
    {{ KEY_FN1, KEY_RIGHT }, {KEY_END}},
    {{ KEY_FN1, KEY_UP }, {KEY_PAGEUP}},
    {{ KEY_FN1, KEY_DOWN }, {KEY_PAGEDOWN}},
    {{ KEY_FN1, KEY_BACKSPACE }, {KEY_DELETE}},

    {{ KEY_FN1, KEY_1 }, {KEY_F1}},
    {{ KEY_FN1, KEY_2 }, {KEY_F2}},
    {{ KEY_FN1, KEY_3 }, {KEY_F3}},
    {{ KEY_FN1, KEY_4 }, {KEY_F4}},
    {{ KEY_FN1, KEY_5 }, {KEY_F5}},
    {{ KEY_FN1, KEY_6 }, {KEY_F6}},
    {{ KEY_FN1, KEY_7 }, {KEY_F7}},
    {{ KEY_FN1, KEY_8 }, {KEY_F8}},
    {{ KEY_FN1, KEY_9 }, {KEY_F9}},
    {{ KEY_FN1, KEY_0 }, {KEY_F10}},
    {{ KEY_FN1, KEY_MINUS }, {KEY_F11}},
    {{ KEY_FN1, KEY_EQUAL }, {KEY_F12}},
    {{ KEY_FN1, KEY_GRAVE }, {KEY_CAPSLOCK}},

    {{ KEY_LEFTALT, KEY_LEFTSHIFT, KEY_Q}, {KEY_LEFTALT, KEY_F4}},

    { { KEY_FN1, KEY_LEFTSHIFT, KEY_SLASH}, { .function=dump_debug_status }, true },
	// { .keys = { KEY_FN1, KEY_LEFTSHIFT, KEY_COMMA}, .result.function=dump_debug_status, .result_is_function=true}
};
size_t key_stroke_size = sizeof(keystroke_map) / sizeof(struct keystroke); 
//dffsdfsd  ?
int output_ports[] = {
    26,
    13,
    12,
    27,
    33,
    15,
    32,
    14,
    23,
    22,
    16,
    17,
    21,
};
int output_port_count = sizeof(output_ports) / sizeof(int);

int input_ports[] = {
    36,
    4,
    5,
    18,
    19
};
int input_port_count = sizeof(input_ports) / sizeof(int);


void typeText(const char* text);

void bt_send_update();

// Tasks
void bluetoothTask(void*);
void read_keys(void*);


bool isBleConnected = false;

void setup() {
    Serial.begin(115200);

    for (int i; i < KEY_LIMIT; i++) {
        bt_key_map[i] = 0;
        bt_key_map_last[i] = 0;
    }

    xTaskCreate(read_keys, "read_keys", 5000, NULL, 4, NULL);
    xTaskCreate(bluetoothTask, "bluetooth", 20000, NULL, 5, NULL);
}

void loop() {}

void read_keys(void *)
{

    Serial.println("Setting up GPIO");

    for (int i=0;i<input_port_count;i++)
        pinMode(input_ports[i], INPUT);

    for (int i=0;i<output_port_count;i++)
        pinMode(output_ports[i], OUTPUT); 

    Serial.println("Starting key read main task");
    
    bool function_latch = true;

    for (;;)
    {

        for (int i=0;i<output_port_count;i++){
            for (int j=0;j<input_port_count;j++) {
                uint8_t keycode = key_code_map[j][i];
                digitalWrite(output_ports[i], HIGH);
                // Reset event mappings for keys that haven't been used in a while
                if (key_event[keycode] != 0) {
                    if (millis() - key_last_event_time[keycode] > tap_expire_time  && key_event[keycode] % 2 == LOW) {
                        key_event[keycode] = digitalRead(input_ports[j]);
                    }
                }

                // Detect key state changes and update the event map
                if (millis() - key_last_event_time[keycode] > bounce_time  && key_event[keycode] % 2 != digitalRead(input_ports[j])) {
                    key_event[keycode]++;
                    key_last_event_time[keycode] = millis();
                }

                digitalWrite(output_ports[i], LOW);
            }
        }


        // Reset buttons that are no longer held 
        for (int i = 0; i < KEY_LIMIT; i++) {
            bt_key_map[i] = 0;
        }


        int any_stroke_triggered = 0;
        // Detect keystrokes before trying to send anything over BT
        for (int stroke=0; stroke<key_stroke_size; stroke++) {
            int stroke_triggered = 1;
            for (int i=0; i<MAX_KEYS_PER_KEYSTROKE; i++) {
                uint8_t key = keystroke_map[stroke].keys[i];
                if (key == 0) continue;
                stroke_triggered = stroke_triggered && key_event[key]%2==1;
            }
            if (stroke_triggered) {
                if (function_latch && keystroke_map[stroke].result_is_function) {
                    (keystroke_map[stroke].result.function)();
                    function_latch = false;
                } else {
                    for (int j=0; j<KEY_LIMIT;j++) {
                        bt_key_map[j] = keystroke_map[stroke].result.keys[j];
                    }
                }
                any_stroke_triggered = 1;
            }
        }

        if ( !any_stroke_triggered ) {
            function_latch = true;
            int bti = 0;
			// Grab the first 6 keys and add them to the BT send list
            for (int keycode = 0; keycode < KEY_MAP_SIZE && bti < KEY_LIMIT; keycode++) {
                if (key_event[keycode] % 2 == 1) {
                    bt_key_map[bti] = keycode;
                    bti++;
                }
            }
        }


        // Check the last sent BT map, if it's not the same, send the update
        int send_update = 0;
        for (int i = 0; i < KEY_LIMIT; i++) {
            if (bt_key_map[i] != bt_key_map_last[i]) {
                send_update = 1;
            }
        }

        if ( send_update ) {
            for (int i = 0; i < KEY_LIMIT; i++) {
                bt_key_map_last[i] = bt_key_map[i];
            }
            bt_send_update();
        }
        
        delay(1);
    }
}

// Message (report) sent when a key is pressed or released
struct InputReport {
    uint8_t modifiers;	     // bitmask: CTRL = 1, SHIFT = 2, ALT = 4
    uint8_t reserved;        // must be 0
    uint8_t pressedKeys[6];  // up to six concurrenlty pressed keys
};

// Message (report) received when an LED's state changed
struct OutputReport {
    uint8_t leds;            // bitmask: num lock = 1, caps lock = 2, scroll lock = 4, compose = 8, kana = 16
};


// The report map describes the HID device (a keyboard in this case) and
// the messages (reports in HID terms) sent and received.
static const uint8_t REPORT_MAP[] = {
    USAGE_PAGE(1),      0x01,       // Generic Desktop Controls
    USAGE(1),           0x06,       // Keyboard
    COLLECTION(1),      0x01,       // Application
    REPORT_ID(1),       0x01,       //   Report ID (1)
    USAGE_PAGE(1),      0x07,       //   Keyboard/Keypad
    USAGE_MINIMUM(1),   0xE0,       //   Keyboard Left Control
    USAGE_MAXIMUM(1),   0xE7,       //   Keyboard Right Control
    LOGICAL_MINIMUM(1), 0x00,       //   Each bit is either 0 or 1
    LOGICAL_MAXIMUM(1), 0x01,
    REPORT_COUNT(1),    0x08,       //   8 bits for the modifier keys
    REPORT_SIZE(1),     0x01,       
    HIDINPUT(1),        0x02,       //   Data, Var, Abs
    REPORT_COUNT(1),    0x01,       //   1 byte (unused)
    REPORT_SIZE(1),     0x08,
    HIDINPUT(1),        0x01,       //   Const, Array, Abs
    REPORT_COUNT(1),    0x06,       //   6 bytes (for up to 6 concurrently pressed keys)
    REPORT_SIZE(1),     0x08,
    LOGICAL_MINIMUM(1), 0x00,
    LOGICAL_MAXIMUM(1), 0x65,       //   101 keys
    USAGE_MINIMUM(1),   0x00,
    USAGE_MAXIMUM(1),   0x65,
    HIDINPUT(1),        0x00,       //   Data, Array, Abs
    REPORT_COUNT(1),    0x05,       //   5 bits (Num lock, Caps lock, Scroll lock, Compose, Kana)
    REPORT_SIZE(1),     0x01,
    USAGE_PAGE(1),      0x08,       //   LEDs
    USAGE_MINIMUM(1),   0x01,       //   Num Lock
    USAGE_MAXIMUM(1),   0x05,       //   Kana
    LOGICAL_MINIMUM(1), 0x00,
    LOGICAL_MAXIMUM(1), 0x01,
    HIDOUTPUT(1),       0x02,       //   Data, Var, Abs
    REPORT_COUNT(1),    0x01,       //   3 bits (Padding)
    REPORT_SIZE(1),     0x03,
    HIDOUTPUT(1),       0x01,       //   Const, Array, Abs
    END_COLLECTION(0)               // End application collection
};


BLEHIDDevice* hid;
BLECharacteristic* input;
BLECharacteristic* output;

const InputReport NO_KEY_PRESSED = { };


/*
 * Callbacks related to BLE connection+
 */
class BleKeyboardCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* server) {
        isBleConnected = true;

        // Allow notifications for characteristics
        BLE2902* cccDesc = (BLE2902*)input->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
        cccDesc->setNotifications(true);

        Serial.println("Client has connected");
    }



    void onDisconnect(BLEServer* server) {
        isBleConnected = false;

        // Disallow notifications for characteristics
        BLE2902* cccDesc = (BLE2902*)input->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
        cccDesc->setNotifications(false);

        Serial.println("Client has disconnected");
    }
};


/*
 * Called when the client (computer, smart phone) wants to turn on or off
 * the LEDs in the keyboard.
 * 
 * bit 0 - NUM LOCK
 * bit 1 - CAPS LOCK
 * bit 2 - SCROLL LOCK
 */
 class OutputCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* characteristic) {
        OutputReport* report = (OutputReport*) characteristic->getData();
        Serial.print("LED state: ");
        Serial.print((int) report->leds);
        Serial.println();
    }
};

// Whats next... hmmm
// arrow keys?


void bluetoothTask(void*) {

    // initialize the device
    BLEDevice::init(DEVICE_NAME);
    BLEServer* server = BLEDevice::createServer();
    server->setCallbacks(new BleKeyboardCallbacks());

    // create an HID device
    hid = new BLEHIDDevice(server);
    input = hid->inputReport(1); // report ID
    output = hid->outputReport(1); // report ID
    output->setCallbacks(new OutputCallbacks());

    // set manufacturer name
    hid->manufacturer()->setValue("Conrad");
    // set USB vendor and product ID
    hid->pnp(0x02, 0xe502, 0xa111, 0x0420);
    // information about HID device: device is not localized, device can be connected
    hid->hidInfo(0x00, 0x02);

    // Security: device requires bonding
    BLESecurity* security = new BLESecurity();
    security->setAuthenticationMode(ESP_LE_AUTH_BOND);

    // set report map
    hid->reportMap((uint8_t*)REPORT_MAP, sizeof(REPORT_MAP));
    hid->startServices();

    // set battery level to 100%
    hid->setBatteryLevel(101);

    // advertise the services
    BLEAdvertising* advertising = server->getAdvertising();
    advertising->setAppearance(HID_KEYBOARD);
    advertising->addServiceUUID(hid->hidService()->getUUID());
    advertising->addServiceUUID(hid->deviceInfo()->getUUID());
    advertising->addServiceUUID(hid->batteryService()->getUUID());
    advertising->start();

    Serial.println("BLE ready"); 
    delay(portMAX_DELAY);
};

void bt_send_update() {
    // Serial.println("Trying to send update");
    if (!isBleConnected) return;

    uint8_t mods =
        (key_event[ KEY_LEFTCTRL   ] % 2) << 0 |
        (key_event[ KEY_LEFTSHIFT  ] % 2) << 1 |
        (key_event[ KEY_LEFTALT    ] % 2) << 2 |
        (key_event[ KEY_LEFTMETA   ] % 2) << 3 |
        (key_event[ KEY_RIGHTCTRL  ] % 2) << 4 |
        (key_event[ KEY_RIGHTSHIFT ] % 2) << 5 |
        (key_event[ KEY_RIGHTALT   ] % 2) << 6 |
        (key_event[ KEY_RIGHTMETA  ] % 2) << 7 ;

    // create input report
    InputReport report = {
        .modifiers = mods,
        .reserved = 0,
        .pressedKeys = {
            bt_key_map[0],
            bt_key_map[1],
            bt_key_map[2],
            bt_key_map[3],
            bt_key_map[4],
            bt_key_map[5],
        }
    };

    // send the input report
    input->setValue((uint8_t*)&report, sizeof(report));
    input->notify();
}

// void typeText(const char* text) {
//     int len = strlen(text);
//     for (int i = 0; i < len; i++) {

//         // translate character to key combination
//         uint8_t val = (uint8_t)text[i];
//         if (val > KEYMAP_SIZE)
//             continue; // character not available on keyboard - skip
//         KEYMAP map = keymap[val];

//         // create input report
//         InputReport report = {
//             .modifiers = map.modifier,
//             .reserved = 0,
//             .pressedKeys = {
//                 map.usage,
//                 0, 0, 0, 0, 0
//             }
//         };

//         // send the input report
//         input->setValue((uint8_t*)&report, sizeof(report));
//         input->notify();

//         delay(5);

//         // release all keys between two characters; otherwise two identical
//         // consecutive characters are treated as just one key press
//         input->setValue((uint8_t*)&NO_KEY_PRESSED, sizeof(NO_KEY_PRESSED));
//         input->notify();

//         delay(5);
//     }
// }