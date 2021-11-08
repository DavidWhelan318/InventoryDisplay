#include <bluefruit.h>
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
#include <Adafruit_ThinkInk.h>

using namespace Adafruit_LittleFS_Namespace;

uint8_t inventUuid[16] = {
  0xda, 0x39, 0x64, 0x22, 
  0x5d, 0x7c, 0x4e, 0xc3,
  0xba, 0x2c, 0x84, 0x6b,
  0xb4, 0x32, 0x33, 0xd3 
};

uint8_t itemIdUuid[16] = {
  0x46, 0x4c, 0x80, 0x30,
  0x34, 0x83, 0x41, 0x65,
  0xa1, 0x16, 0x81, 0x91, 
  0xa1, 0x72, 0x9b, 0xec
};

uint8_t quantityUuid[16] = {
  0x5a, 0xca, 0xe0, 0xcf,
  0x57, 0xc0, 0x49, 0xa0, 
  0xae, 0xcf, 0xd8, 0x66, 
  0xf0, 0xa5, 0x0d, 0x2e
};

uint8_t containerUuid[16] = {
  0xf1, 0x03, 0xba, 0x7b,
  0x29, 0x88, 0x4c, 0x27, 
  0xb1, 0xe0, 0x6f, 0xa0,
  0x03, 0xa9, 0x52, 0x29
};

uint8_t unitUuid[16] = {
  0x24, 0x5d, 0x4f, 0xe3, 
  0xb1, 0xd7, 0x4d, 0x0f, 
  0xa7, 0x7c, 0xe8, 0x96,
  0x69, 0x87, 0x1d, 0x2b
};

char* unitTypes[] = {
  "pcs", "lb", "g", "ft", "in", "m", "cm"
};

char* containerTypes[] = {
  "Pallet", "Box", "Shelf", "None"
};

BLEService invent = BLEService(inventUuid);
BLECharacteristic itemId = BLECharacteristic(itemIdUuid);
BLECharacteristic quantity = BLECharacteristic(quantityUuid);
BLECharacteristic container = BLECharacteristic(containerUuid);
BLECharacteristic unit = BLECharacteristic(unitUuid);

BLEDis bledis;
BLEBas blebas;

File file(InternalFS);

#define EPD_DC      10 // can be any pin, but required!
#define EPD_CS      9  // can be any pin, but required!
#define EPD_BUSY    -1  // can set to -1 to not use a pin (will wait a fixed delay)
#define SRAM_CS     6  // can set to -1 to not use a pin (uses a lot of RAM!)
#define EPD_RESET   -1  // can set to -1 and share with chip Reset (can't deep sleep)

ThinkInk_290_Grayscale4_T5 display(EPD_DC, EPD_RESET, EPD_CS, SRAM_CS, EPD_BUSY);

unsigned long lastUpdate = 0;
unsigned long nextUpdate = 0;
bool updateDisplay = true;

void writeValuesToDisplay()
{
  //Serial.println("Updating Display");
  //Serial.println(millis());

  display.clearBuffer();
  display.setTextColor(EPD_BLACK);
  display.setTextSize(3);
  display.setCursor(0, 0);

  uint8_t disContainer = container.read8();
  if(disContainer < 3)
    display.println(containerTypes[disContainer]);
  else
    display.println("None");

  display.println();

  char buffer[16] = {0};
  itemId.read(buffer, 16);
  display.print("Item:");
  display.println(buffer);

  display.println();
  
  uint16_t disQuantity = quantity.read16();
  display.print("Qty:");
  display.print(disQuantity);

  uint8_t disUnit = unit.read8();
  if(disUnit < 7)
    display.println(unitTypes[disUnit]);
  else
    display.println("pcs");
  
  display.display();
  
}

void checkForRefresh()
{
  unsigned long currentTime = millis();
  if(currentTime > nextUpdate && updateDisplay)
  {
    writeValuesToDisplay();
    lastUpdate = millis();
    updateDisplay = false;
  }
}

void setDisplayRefresh()
{
  if(!updateDisplay)
  {
    //Serial.println("Scheduling Update");
    unsigned long currentTime = millis();
    unsigned long elapsedTime = abs(currentTime-lastUpdate);
    //Serial.println(currentTime);
    if(elapsedTime > 180000)
    {
      nextUpdate = millis() + 3000;
      updateDisplay = true;
    }
    else
    {
      //possible mod for when time rolls over after 40 days
      nextUpdate = lastUpdate + 183000;
      updateDisplay = true;
    }
    //Serial.println(nextUpdate);
  }
}

void inventCallback(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len)
{
  const char* fileName = chr->uuid.toString().c_str();
  InternalFS.remove(fileName);
  
  if(file.open(fileName, FILE_O_WRITE))
  {
    file.write(data, len);
    file.close();
  }

  //Serial.println("Values Written");

  setDisplayRefresh();
  
  return;
}

void setupInvent()
{
  const char* fileName;
  
  invent.begin();

  itemId.setProperties(CHR_PROPS_READ | CHR_PROPS_WRITE);
  itemId.setPermission(SECMODE_OPEN, SECMODE_OPEN);
  itemId.setMaxLen(16);
  itemId.setWriteCallback(inventCallback);
  itemId.setPresentationFormatDescriptor(0x19, 0, 0x2700);
  itemId.begin();

  fileName = itemId.uuid.toString().c_str();
  file.open(fileName, FILE_O_READ);
  if(file)
  {
    char buffer[16] = {0};
    uint32_t temp = file.read(buffer, sizeof(buffer));
    
    itemId.write(buffer);
    file.close();
  }
  else
  {
    itemId.write("Test Item");
  }
  
  quantity.setProperties(CHR_PROPS_READ | CHR_PROPS_WRITE);
  quantity.setPermission(SECMODE_OPEN, SECMODE_OPEN);
  quantity.setFixedLen(2);
  quantity.setWriteCallback(inventCallback);
  quantity.setPresentationFormatDescriptor(0x08, 0, 0x2700);
  quantity.begin();

  fileName = quantity.uuid.toString().c_str();
  file.open(fileName, FILE_O_READ);
  if(file)
  {
    uint8_t* buffer = new uint8_t[2];

    file.read(buffer, 2);
    
    quantity.write(buffer, 2);
    file.close();
  }
  else
  {
    quantity.write16(0x0000);
  }
  
  container.setProperties(CHR_PROPS_READ | CHR_PROPS_WRITE);
  container.setPermission(SECMODE_OPEN, SECMODE_OPEN);
  container.setFixedLen(1);
  container.setWriteCallback(inventCallback);
  container.setPresentationFormatDescriptor(0x04, 0, 0x2700);
  container.begin();

  fileName = container.uuid.toString().c_str();
  file.open(fileName, FILE_O_READ);
  if(file)
  {
    uint8_t input = file.read();
    container.write8(input);
    file.close();
  }
  else
  {
    container.write8(0);
  }

  unit.setProperties(CHR_PROPS_READ | CHR_PROPS_WRITE);
  unit.setPermission(SECMODE_OPEN, SECMODE_OPEN);
  unit.setFixedLen(1);
  unit.setWriteCallback(inventCallback);
  unit.setPresentationFormatDescriptor(0x04, 0, 0x2700);
  unit.begin();

  fileName = unit.uuid.toString().c_str();
  file.open(fileName, FILE_O_READ);
  if(file)
  {
    uint8_t input = file.read();
    unit.write8(input);
    file.close();
  }
  else
  {
    unit.write8(0);
  }
  
  return;
}

void startAdv(void)
{
  // Advertising packet
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();

  Bluefruit.Advertising.addService(invent);
  
  Bluefruit.Advertising.addName();
  
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);    // in unit of 0.625 ms
  Bluefruit.Advertising.setFastTimeout(30);      // number of seconds in fast mode
  Bluefruit.Advertising.start(0);                // 0 = Don't stop advertising after n seconds
  return;  
}

void setup() {
  //Serial.begin(115200);

  //while(!Serial) delay(10);

  InternalFS.begin();
  
  Bluefruit.begin();

  display.begin(THINKINK_MONO);
  
  Bluefruit.autoConnLed(false);

  bledis.setManufacturer("Adafruit Industries");
  bledis.setModel("Bluefruit Feather52");
  bledis.begin();

  blebas.begin();
  blebas.write(100);

  setupInvent();

  startAdv();
}

void loop() {
  // put your main code here, to run repeatedly:
  checkForRefresh();
}
