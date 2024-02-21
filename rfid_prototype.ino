/* This program was developed for the Quantitative Conservation Lab at the University of Washington. The 
 * code should detect RFID PIT Tags (134.2 kHz) that are within 1 inch of the antenna coil.
 * 
 * RFID V+              -   5V
 * RFID V-              -   GND
 * MicroSD VCC          -   5V
 * MicroSD GND          -   GND
 * RTC VCC (PCF8523)    -   3.3V
 * RTC GND (PCF8523)    -   GND
 * MicroSD CS           -   pin 4
 * RFID 1 TX            -   pin 5
 * RFID 1 RX            -   pin 6
 * RFID 2 TX            -   pin 7
 * RFID 2 RX            -   pin 8
 * MicroSD DI (MOSI)    -   pin 11
 * MicroSD DO (MISO)    -   pin 12
 * MicroSD SCK (SCK)    -   pin 13
 * RTC SDA (PCF8523)    -   A4
 * RTC SCL (PCF8523)    -   A5
 */

#include <SoftwareSerial.h>
#include <SD.h>
#include "RTClib.h"
#include <EEPROM.h>

#define DEVICE_ID_ADDR          0x00
#define DEVICE_ID               0x01
              
#define SD_CHIP_SELECT          4
#define RFID_1_TX               5
#define RFID_1_RX               6
#define RFID_2_TX               7
#define RFID_2_RX               8

#define DETECTION_SLEEP_TIME    100

SoftwareSerial rfid_1_serial(RFID_1_TX, RFID_1_RX);
SoftwareSerial rfid_2_serial(RFID_2_TX, RFID_2_RX);
RTC_PCF8523 rtc;

File logFile;
String logFilename;

uint8_t device_id;

void setup()
{
    String output;
    bool file_exists = false;
    
    Serial.begin(115200);
    rfid_1_serial.begin(9600);
    rfid_2_serial.begin(9600);
    init_rfid();
      
    if (! rtc.begin()) 
    {
        //Serial.println("Couldn't find RTC");
        abort();
    }

    //if(1)
    if (! rtc.initialized() || rtc.lostPower()) 
    {
        Serial.println("RTC is NOT initialized, let's set the time!");
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
    
    if (!SD.begin(SD_CHIP_SELECT))
    {
        //Serial.println("Card init. failed!");
        abort();
    }

    device_id = EEPROM.read(DEVICE_ID_ADDR);

    if (device_id == 0xFF)
    {
        EEPROM.write(DEVICE_ID_ADDR, DEVICE_ID);
    }

    DateTime now = rtc.now();
    logFilename = String(now.year()) + String(now.month()) + String(now.day()) + ".csv";
    
    if (SD.exists(logFilename))
    {
        file_exists = true;
    }
    logFile = SD.open(logFilename, FILE_WRITE);
    if (logFile)
    {
        if (!file_exists)
        {
            //Serial.println("New File created");
            output = "timestamp,device_num,antenna_num,rfid_id";
            logFile.println(output);
            logFile.flush();
            logFile.close();
        }
    }
    else
    {
        //Serial.println("File not created");
        abort();
    }

    rtc.start();
}

void loop() 
{
    uint32_t returned_id = 0;
    static unsigned long detection_time_1 = 0;
    static unsigned long detection_time_2 = 0;
    
    if (millis () - detection_time_1 >= DETECTION_SLEEP_TIME || detection_time_1 == 0)
    {
        start_rfid_capture(&rfid_1_serial);
        delay(100);
        returned_id = stop_rfid_capture(&rfid_1_serial, 1);

        /* If an ID was found, reset the detection timestamp */
        if (returned_id != 0)
        {
            detection_time_1 = millis();
        }
        delay (10);
    }

    if (millis() - detection_time_2 >= DETECTION_SLEEP_TIME || detection_time_2 == 0)
    {
        start_rfid_capture(&rfid_2_serial);
        delay(100);
        returned_id = stop_rfid_capture(&rfid_2_serial, 2);

        /* If an ID was found, reset the detection timestamp */
        if (returned_id != 0)
        {
            detection_time_2 = millis();
        }
        delay (10);
    }
}

void flush_buffer(SoftwareSerial *rfid_serial)
{
    /* Flush all characters in buffer */
    rfid_serial->listen();
    while (rfid_serial->available())
    {
        rfid_serial->read();
    }
}

void init_rfid()
{
    float freq_1 = 0.0;
    float freq_2 = 0.0;
    rfid_1_serial.print("SD2\r");
    rfid_1_serial.print("SRD\r");
    rfid_1_serial.print("RSD\r");
    delay(300);

    flush_buffer(&rfid_1_serial);
    
    /* Check the frequency of the antenna */ 
    freq_1 = get_rfid_freq(&rfid_1_serial);
    Serial.println("Antenna 1 Frequency: " + String(freq_1) + " kHz");
    
    rfid_2_serial.print("SD2\r");
    rfid_2_serial.print("SRD\r");
    rfid_2_serial.print("RSD\r");
    delay(300);

    flush_buffer(&rfid_2_serial);

    freq_2 = get_rfid_freq(&rfid_2_serial);
    Serial.println("Antenna 2 Frequency: " + String(freq_2) + " kHz");
}

float get_rfid_freq(SoftwareSerial *rfid_serial)
{
    float retv = 0.0;
    rfid_serial->print("MOF\r");
    
    delay(1400);
    rfid_serial->listen();
    if(rfid_serial->available())
    {
        retv = rfid_serial->parseFloat();
    }
    
    return retv;
}

void start_rfid_capture(SoftwareSerial *rfid_serial)
{
    rfid_serial->print("SRA\r");
    flush_buffer(rfid_serial);
}

uint32_t stop_rfid_capture(SoftwareSerial *rfid_serial, int antenna_num)
{
    DateTime now;
    String epochString;
    String output;
    
    uint32_t captured_id = check_rfid_id(rfid_serial);
    if(captured_id)
    {
        //Serial.println("ID: " + String(captured_id));

        now = rtc.now();
        epochString = String(now.unixtime());
        output = epochString + "," + String(device_id) + "," + String(antenna_num) + "," + String(captured_id);
        
        logFile = SD.open(logFilename, FILE_WRITE);
        if (logFile)
        {       
            logFile.println(output);
            logFile.flush();
            logFile.close();
        }
        else
        {
            Serial.println("What happened?");
        }    
    }
    rfid_serial->print("SRD\r");
    flush_buffer(rfid_serial);

    return captured_id;
}

uint32_t check_rfid_id(SoftwareSerial *rfid_serial)
{
    uint32_t retv = 0;
    bool found_id = false;
    uint8_t skipped_chars = 0;

    rfid_serial->listen();
    while(rfid_serial->available())
    {
        char c = rfid_serial->read();
        if(found_id)
        {
            if(skipped_chars == 3)
            {
                retv = rfid_serial->parseInt();
                found_id = false;
                flush_buffer(rfid_serial);
                break;
            }
            skipped_chars++;
        }
        else if(c == '_')
        {
            found_id = true;
            skipped_chars = 0;
        }
    }
    
    return retv;
}
