#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// Change this to your treadmill MAC address
static std::string treadmillMac = "d9:ed:78:01:b2:dc";

// Other treadmill definitions
static BLEUUID treadmillSvcUUID("1826");
static BLEUUID treadmillDataUUID("2ACD");
static BLEUUID treadmillCtrlUUID("2AD9");

// Globals
BLEServer* pServer     = nullptr;
BLECharacteristic* pMeasure    = nullptr;
BLEClient* pClient     = nullptr;

volatile bool connected    = false;    // status of garmin connection
volatile bool wasConnected = false;
bool doConnecttreadmill        = true;     // status of treadmill connection

uint16_t currentSpeedRaw   = 0;        // speed from treadmill
uint32_t lastNotifyMs      = 0;
uint32_t connectMs         = 0;

const uint32_t NOTIFY_INTERVAL   = 1000;
const uint32_t POST_CONNECT_WAIT = 1500; 

// RSC Flags & Settings
// bit0=stride present, bit1=distance present, bit2=0(walking)
#define RSC_FLAGS 0x03
const uint8_t  CADENCE    = 85;   // mock cadence, irrelevant if we have speed?
const uint16_t STRIDE_CM  = 80;   

// Treadmill callback
static void treadmillNotifyCallback(BLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
    if (length < 4) return;
    // the speed is in the 2nd and the 3rd byte
    currentSpeedRaw = (uint16_t)pData[2] | ((uint16_t)pData[3] << 8);
}

// Garmin server callbacks
class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer*) override {
    connected  = true;
    connectMs  = millis();
    lastNotifyMs = millis();
    Serial.println("=== GARMIN CONNECTED ===");
  }
  void onDisconnect(BLEServer*) override {
    connected = false;
    Serial.println("=== GARMIN DISCONNECTED ===");
  }
};

// Advertising
void startAdvertising() {
  BLEAdvertising* pAdv = BLEDevice::getAdvertising();
  pAdv->stop();
  delay(200);

  // Specific batch for Garmin
  uint8_t adv[] = {
    0x02, 0x01, 0x06,
    0x03, 0x19, 0x41, 0x04,        // Appearance 0x0441
    0x03, 0x03, 0x14, 0x18,        // UUID 0x1814
    0x08, 0x08, 'R','S','C',' ','P','o','d'
  };
  uint8_t scan[] = {
    0x0B, 0x09, 'R','S','C',' ','S','e','n','s','o','r'
  };

  BLEAdvertisementData advData, scanData;
  advData.addData((char*)adv,  sizeof(adv));
  scanData.addData((char*)scan, sizeof(scan));
  pAdv->setAdvertisementData(advData);
  pAdv->setScanResponseData(scanData);
  pAdv->setMinInterval(160);
  pAdv->setMaxInterval(240);
  pAdv->start();
  Serial.println("Advertising RSC Sensor...");
}

// RSC package for Garmin
void sendMeasurement() {
  if (!connected || !pMeasure) return;

  // Converting treadmill speed (1/100 km) to RSC format (1/256 m/s)  
  float speedKmh = currentSpeedRaw / 100.0f;
  float speedMs = speedKmh / 3.6f;
  uint16_t rscSpeed = (uint16_t)(speedMs * 256.0f + 0.5f); // +0.5 is here to round to the nearest int

  uint8_t buf[10];
  memset(buf, 0, 10);
  buf[0] = RSC_FLAGS;
  buf[1] = rscSpeed & 0xFF;
  buf[2] = rscSpeed >> 8;
  buf[3] = CADENCE; 
  buf[4] = STRIDE_CM & 0xFF;
  buf[5] = STRIDE_CM >> 8;
  // distance is sent as zero

  pMeasure->setValue(buf, 10);
  pMeasure->notify(true); // // sending the packet not waiting for CCCD

  Serial.printf("[Garmin] Speed: %.2f km/h | Raw: %u\n", speedKmh, rscSpeed);
}

// Setup
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== Treadmill-Garmin Bridge ===");

  BLEDevice::init("RSC Sensor");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  // RSC Service
  BLEService* pRSC = pServer->createService(BLEUUID((uint16_t)0x1814), 20);
  pMeasure = pRSC->createCharacteristic(
    BLEUUID((uint16_t)0x2A53),
    BLECharacteristic::PROPERTY_NOTIFY
  );

  // Setting up CCCD
  BLE2902* cccd = new BLE2902();
  cccd->setNotifications(true); 
  pMeasure->addDescriptor(cccd);

  // Mandatory chars of the RSC Feature
  BLECharacteristic* pFeat = pRSC->createCharacteristic(
    BLEUUID((uint16_t)0x2A54),
    BLECharacteristic::PROPERTY_READ
  );
  uint16_t feat = 0x0007; // We support speed, cadence and steps
  pFeat->setValue((uint8_t*)&feat, 2);

  pRSC->start();
  startAdvertising();
}

// Loop
void loop() {
  uint32_t now = millis();

  // Handling the Garmin part
  if (connected) {
    bool waitDone = (now - connectMs) >= POST_CONNECT_WAIT;
    if (waitDone && (now - lastNotifyMs) >= NOTIFY_INTERVAL) {
      sendMeasurement();
      lastNotifyMs = now;
    }
  }

  // If disconnected - readvertise
  if (!connected && wasConnected) {
    delay(300);
    startAdvertising();
    wasConnected = false;
  }
  if (connected && !wasConnected) {
    wasConnected = true;
  }

  // 2. Handling hte Treadmill part
  if (doConnecttreadmill) {
    doConnecttreadmill = false;
    pClient = BLEDevice::createClient();
    Serial.println("Connecting to Treadmill...");
    
    if (pClient->connect(BLEAddress(treadmillMac.c_str()))) {
      BLERemoteService* pRSvc = pClient->getService(treadmillSvcUUID);
      if (pRSvc) {
        BLERemoteCharacteristic* pDataChar = pRSvc->getCharacteristic(treadmillDataUUID);
        BLERemoteCharacteristic* pCtrlChar = pRSvc->getCharacteristic(treadmillCtrlUUID);
        if (pDataChar) pDataChar->registerForNotify(treadmillNotifyCallback);
        if (pCtrlChar) {
          uint8_t req[] = {0x00};
          pCtrlChar->writeValue(req, 1, true);
        }
        Serial.println("Treadmill connected!");
      }
    } else {
      Serial.println("Treadmill not found. Will retry...");
      doConnecttreadmill = true;
      delay(5000);
    }
  }

  delay(10);
}
