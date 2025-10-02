// CC0 1.0 Universal (CC0 1.0)
// Public Domain Dedication
// https://github.com/nmrr
// Inspired by the examples shared by the LILYGO team: https://github.com/Xinyuan-LilyGO/LilyGo-Modem-Series/tree/main/examples

//#define DUMP_AT_COMMANDS

#include "utilities.h"
#include <TinyGsmClient.h>
#include "Arduino.h"
#include <time.h>
#include "mbedtls/aes.h"
#include "mbedtls/md.h"

#define SERVER_ADDR "XX.XX.XX.XX"
#define SERVER_PORT 55100

// To generate keys: openssl rand -hex 32 | tee >(sed 's/../0x&, /g' | sed 's/, $/};/;s/^/{/' | sed '1s/^/\r\n/')
unsigned char keyAES[32] = { /* YOUR AES256 KEY*/ };
const unsigned char keyHMAC[32] = { /*YOUR HMAC SECRET*/ };

typedef struct {
  unsigned char* encrypted;
  unsigned char HMAC[32];
} structEncryption;

structEncryption AES256CTR_HMACSHA256(unsigned char keyAES[32], unsigned char ivAES[16], unsigned char* input,
                                      size_t size, const unsigned char* keyHMAC, size_t keyHMACsize) {
  // AES 256 CTR
  structEncryption output;
  output.encrypted = (unsigned char*)calloc(size, sizeof(unsigned char));
  unsigned char aesBlock[16];
  size_t aesCounter = 0;
  mbedtls_aes_context aes;
  unsigned char ivAESForEncryption[16];
  memcpy(ivAESForEncryption, ivAES, 16);

  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_enc(&aes, keyAES, 256);
  mbedtls_aes_crypt_ctr(&aes, size, &aesCounter, ivAESForEncryption, aesBlock, input, output.encrypted);
  mbedtls_aes_free(&aes);

  // HMAC ON IV+ENCRYPTED MESSAGE
  mbedtls_md_context_t ctx;
  const mbedtls_md_info_t* md_info;

  mbedtls_md_init(&ctx);
  md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  mbedtls_md_setup(&ctx, md_info, 1);
  mbedtls_md_hmac_starts(&ctx, keyHMAC, keyHMACsize);
  mbedtls_md_hmac_update(&ctx, ivAES, 16);
  mbedtls_md_hmac_update(&ctx, output.encrypted, size);
  mbedtls_md_hmac_finish(&ctx, output.HMAC);
  mbedtls_md_free(&ctx);

  return output;
}

#ifdef DUMP_AT_COMMANDS  // if enabled it requires the streamDebugger lib
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, Serial);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif

void rebootLilygo()
{
    modem.disableGPS(MODEM_GPS_ENABLE_GPIO, !MODEM_GPS_ENABLE_LEVEL);
    modem.poweroff();
    delay(15000);
    ESP.restart();
}

void setup()
{
    Serial.begin(115200); // Set console baud rate

    Serial.println("Start Sketch");

    SerialAT.begin(115200, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);

    /* Set Power control pin output
    * * @note      Known issues, ESP32 (V1.2) version of T-A7670, T-A7608,
    *            when using battery power supply mode, BOARD_POWERON_PIN (IO12) must be set to high level after esp32 starts, otherwise a reset will occur.
    * */
#ifdef BOARD_POWERON_PIN
    pinMode(BOARD_POWERON_PIN, OUTPUT);
    digitalWrite(BOARD_POWERON_PIN, HIGH);
#endif

    // Set modem reset pin ,reset modem
#ifdef MODEM_RESET_PIN
    pinMode(MODEM_RESET_PIN, OUTPUT);
    digitalWrite(MODEM_RESET_PIN, !MODEM_RESET_LEVEL); delay(100);
    digitalWrite(MODEM_RESET_PIN, MODEM_RESET_LEVEL); delay(2600);
    digitalWrite(MODEM_RESET_PIN, !MODEM_RESET_LEVEL);
#endif

    // Pull down DTR to ensure the modem is not in sleep state
    pinMode(MODEM_DTR_PIN, OUTPUT);
    digitalWrite(MODEM_DTR_PIN, LOW);
    
    // Turn on the modem
    pinMode(BOARD_PWRKEY_PIN, OUTPUT);
    digitalWrite(BOARD_PWRKEY_PIN, LOW);
    delay(100);
    digitalWrite(BOARD_PWRKEY_PIN, HIGH);
    delay(MODEM_POWERON_PULSE_WIDTH_MS);
    digitalWrite(BOARD_PWRKEY_PIN, LOW);

    //////////////////////////////

    Serial.println("Enabling GPS/GNSS/GLONASS");
    while (!modem.enableGPS(MODEM_GPS_ENABLE_GPIO, MODEM_GPS_ENABLE_LEVEL)) {
        Serial.print(".");
    }
    Serial.println();
    Serial.println("GPS Enabled");

    // Set GPS Baud to 115200
    modem.setGPSBaud(115200);

    //////////////////////////////
    
    // Test modem connected
    while (!modem.testAT()) {
        delay(1);
    }

    Serial.println("Modem has power on!");

    // Check if SIM card is online
    SimStatus sim = SIM_ERROR;
    while (sim != SIM_READY) {
        sim = modem.getSimStatus();
        switch (sim) {
        case SIM_READY:
            Serial.println("SIM card online");
            break;
        case SIM_LOCKED:
            Serial.println("The SIM card is locked. Please unlock the SIM card first.");
            // const char *SIMCARD_PIN_CODE = "123456";
            // modem.simUnlock(SIMCARD_PIN_CODE);
            break;
        default:
            break;
        }
        delay(1000);
    }

    int16_t sq ;
    int16_t counter = 0;
    Serial.print("Wait for the modem to register with the network.");
    RegStatus status = REG_NO_RESULT;
    while (status == REG_NO_RESULT || status == REG_SEARCHING || status == REG_UNREGISTERED) {
        status = modem.getRegistrationStatus();
        switch (status) {
        case REG_UNREGISTERED:
        case REG_SEARCHING:
            sq = modem.getSignalQuality();
            Serial.printf("[%lu] Signal Quality:%d\n", millis() / 1000, sq);
            delay(1000);
            break;
        case REG_DENIED:
            Serial.println("Network registration was rejected, please check if the APN is correct");
            return ;
        case REG_OK_HOME:
            Serial.println("Online registration successful");
            break;
        case REG_OK_ROAMING:
            Serial.println("Network registration successful, currently in roaming mode");
            break;
        default:
            Serial.printf("Registration Status:%d\n", status);
            delay(1000);
            break;
        }

        counter++;
        if (counter == 300)
        {
            rebootLilygo();
        }
    }
    Serial.println();

    String res;
    modem.sendAT("+CGREG?");
    modem.waitResponse(1000UL, res);
    Serial.println(res);

    modem.sendAT("+CGACT=1,1");
    if (modem.waitResponse(1000UL, GF("OK")) != 1)
    {
        Serial.printf("NETWORK ERROR 1 !!!\n");
        rebootLilygo();
    }

    modem.sendAT("+CIPMODE=1");
    if (modem.waitResponse(1000UL, GF("OK")) != 1)
    {
        Serial.printf("NETWORK ERROR 2 !!!\n");
        rebootLilygo();
    }

    modem.sendAT("+NETOPEN");
    if (modem.waitResponse(1000UL, GF("OK")) != 1)
    {
        Serial.printf("NETWORK ERROR 3 !!!\n");
        rebootLilygo();
    }
}


void loop()
{
    String res;
    float lat2      = 0;
    float lon2      = 0;
    float speed2    = 0;
    float alt2      = 0;
    int   vsat2     = 0;
    int   usat2     = 0;
    float accuracy2 = 0;
    int   year2     = 0;
    int   month2    = 0;
    int   day2      = 0;
    int   hour2     = 0;
    int   min2      = 0;
    int   sec2      = 0;
    uint8_t    fixMode   = 0;

    struct tm t;
    structEncryption encrypt;
    unsigned char* toTransmit = (unsigned char*)calloc(16 + 24 + 32, sizeof(unsigned char));

    uint16_t counter = 0;
    time_t previousTimestamp = 0;
    time_t previousDrift = 0;
    time_t previousDriftFeedback = 0;

    while(1)
    {
        for (;;) {
            Serial.printf("Requesting current GPS/GNSS/GLONASS location %hu/120\n", ++counter);
            if (modem.getGPS(&fixMode, &lat2, &lon2, &speed2, &alt2, &vsat2, &usat2, &accuracy2,
                            &year2, &month2, &day2, &hour2, &min2, &sec2)) {

                counter = 0;
                speed2 = speed2*1.85200428;

                Serial.print("Lon:"); Serial.print(lon2); Serial.print("\tLat:"); Serial.println(lat2);      
                Serial.print("Speed:"); Serial.print(speed2); Serial.print("\tAltitude:"); Serial.println(alt2);

                t.tm_year = year2-1900;
                t.tm_mon  = month2-1;
                t.tm_mday = day2;
                t.tm_hour = hour2;
                t.tm_min  = min2;
                t.tm_sec  = sec2;
                t.tm_isdst = 0;

                time_t timestamp = mktime(&t);

                if (timestamp-previousTimestamp >= 60)
                {
                    if (previousTimestamp != 0)
                    {
                        previousDrift = timestamp-previousTimestamp-60;
                        Serial.printf("previousDrift : %ld\n", previousDrift);

                        if (previousDrift == 1) previousDriftFeedback--;
                        else if (previousDrift >= 2) previousDriftFeedback -= 2;
                        else if (previousDrift == -1) previousDriftFeedback++;
                        else if (previousDrift <= -2) previousDriftFeedback += 2;
                    }

                    previousTimestamp = timestamp;
                    Serial.printf("TIMESTAMP : %ld\n", timestamp);

                    uint8_t secBuffer[8];
                    memcpy(secBuffer, &timestamp, 8);

                    unsigned char ivAES[16];
                    for (int i = 0; i < 16; i++) {
                        ivAES[i] = esp_random() & 0xFF;
                    }

                    unsigned char GPSCoord[24];
                    for (int i = 0; i < 8; i++) {
                        GPSCoord[i] = secBuffer[i];
                    }

                    unsigned char gpsBuf[4];

                    memcpy(gpsBuf, &lat2, 4);
                    for (int i = 8; i < 12; i++) {
                        GPSCoord[i] = gpsBuf[i-8];
                    }

                    memcpy(gpsBuf, &lon2, 4);
                    for (int i = 12; i < 16; i++) {
                        GPSCoord[i] = gpsBuf[i-12];
                    }

                    memcpy(gpsBuf, &speed2, 4);
                    for (int i = 16; i < 20; i++) {
                        GPSCoord[i] = gpsBuf[i-16];
                    }

                    memcpy(gpsBuf, &alt2, 4);
                    for (int i = 20; i < 24; i++) {
                        GPSCoord[i] = gpsBuf[i-20];
                    }

                    encrypt = AES256CTR_HMACSHA256(keyAES, ivAES, GPSCoord, 24, keyHMAC, 32);

                    for (int i = 0; i < 16; i++) {
                        toTransmit[i] = ivAES[i];
                    }

                    for (int i = 0; i < 24; i++) {
                        toTransmit[16 + i] = encrypt.encrypted[i];
                    }

                    for (int i = 0; i < 32; i++) {
                        toTransmit[16 + 24 + i] = encrypt.HMAC[i];
                    }

                    break;
                }
                else
                {
                    time_t waitTime = timestamp-previousTimestamp-previousDriftFeedback;
                    if (waitTime > 60 || waitTime < 0) waitTime = 30;
                    Serial.printf("waitTime : %ld\n", waitTime);
                    delay(1000*(60-waitTime)); 
                }
            } else {
                delay(5000L);
                if (counter == 120)
                {
                    rebootLilygo();
                }
            }
        }

        modem.sendAT(GF("+CIPOPEN="), 0, ',', GF("\"UDP"), GF("\",\""), SERVER_ADDR, GF("\","), SERVER_PORT ,',', 0);
        if (modem.waitResponse(1000UL, GF("CONNECT 115200")) != 1)
        {
            Serial.printf("SOCKET ERROR 1 !!!\n");
            break;
        }
        
        SerialAT.write(toTransmit,16+24+32);
        delay(100);

        SerialAT.print("+");
        delay(10);
        SerialAT.print("+");
        delay(10);
        SerialAT.print("+");
        delay(10);

        if (modem.waitResponse(1000UL, GF("OK")) != 1)
        {
            Serial.printf("SOCKET ERROR 2 !!!\n");
            break;
        }
        
        modem.sendAT("+CIPCLOSE=0"); 
        if (modem.waitResponse(1000UL, GF("CLOSED")) != 1)
        {
            Serial.printf("SOCKET ERROR 3 !!!\n");
            break;
        }

        delay(1000*15);
    }

    modem.sendAT("+NETCLOSE");  
    modem.waitResponse(1000UL, res);
    Serial.println(res);

    rebootLilygo();
}

#ifndef TINY_GSM_FORK_LIBRARY
#error "No correct definition detected, Please copy all the [lib directories](https://github.com/Xinyuan-LilyGO/LilyGO-T-A76XX/tree/main/lib) to the arduino libraries directory , See README"
#endif
