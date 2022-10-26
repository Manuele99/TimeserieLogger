#include <Arduino.h>
#include <SPIFFS.h>
#include <FS.h>

#define TIMESERIE_LOGGER_TEST
#include <TimeserieLogger.h>

uint32_t freeHeap;
uint8_t loopCount = 0;
uint8_t tmpRecordCount;

void setup() {
    Serial.begin(115200);

    if (SPIFFS.begin(true)) {
        Serial.println("\nFS initialized!");
    } else {
        Serial.println("error whie initializing FS :/");
        return;
    }
    // make sure to start clean
    SPIFFS.remove("/testVal");
    {
        TimeserieLogger ts("testVal", SPIFFS);
        if (!ts.begin()) {
            Serial.println("error while starting ts logger");
            return;
        }
        Serial.printf(" [%s] : ram size test 1\n", ts.getRamRecordCount() == 0 ? "PASS" : " # FAIL");

        ts.addValue(10, 5.6);
        Serial.printf(" [%s] : ram size test 2\n", ts.getRamRecordCount() == 1 ? "PASS" : " # FAIL");

        Serial.printf(" [%s] : file size test 1\n", ts.getFileRecordCount() == 0 ? "PASS" : " # FAIL");


        // now reach the limit
        for (uint8_t i = 1; i < ts.getMaxRamRecords(); i++) {
            if (!ts.addValue(10 + i, 5.6 * (1, 1 * i))) {
                Serial.printf("failed to put value in for at %d\n", i);
            }
        }
        Serial.printf(" [%s] : ram size test 3\n", ts.getRamRecordCount() == ts.getMaxRamRecords() ? "PASS" : " # FAIL");
        Serial.printf(" [%s] : file size test 2\n", ts.getFileRecordCount() == 0 ? "PASS" : " # FAIL");

        if (!ts.addValue(50, 5.5)) {
            Serial.println("falied to put last value to file");
        }
        Serial.printf(" [%s] : ram size test 4\n", ts.getRamRecordCount() == ts.getMaxRamRecords() ? "PASS" : " # FAIL");
        Serial.printf(" [%s] : file size test 3\n", ts.getFileRecordCount() == ts.getMaxRamRecords() + 1 ? "PASS" : " # FAIL");

        testPersistance("testVal");

        // nothing should have changed
        Serial.printf(" [%s] : ram size test 4 BIS\n", ts.getRamRecordCount() == ts.getMaxRamRecords() ? "PASS" : " # FAIL");
        Serial.printf(" [%s] : file size test 3 BIS\n", ts.getFileRecordCount() == ts.getMaxRamRecords() + 1 ? "PASS" : " # FAIL");

        // currently stored ts.getMaxRamRecords() in ram and 1 on file
        ts.dropRecords(10);

        // there should be (ts.getMaxRamRecords() + 1) - 10 in ram (all the records minus 10 just dropped)
        tmpRecordCount = ts.getRamRecordCount();
        Serial.printf(" [%s] : ram size test 5\n", ts.getRamRecordCount() == (ts.getMaxRamRecords() + 1) - 10 ? "PASS" : " # FAIL");
        Serial.printf(" [%s] : file size test 3\n", ts.getFileRecordCount() == (ts.getMaxRamRecords() + 1) - 10 ? "PASS" : " # FAIL");
    }

    TimeserieLogger ts2("testVal", SPIFFS);
    if (!ts2.begin()) {
        Serial.println("error while starting ts2 logger");
        return;
    }
    Serial.printf(" [%s] : ram size after reconstruct test 1\n", ts2.getRamRecordCount() == tmpRecordCount ? "PASS" : " # FAIL");
}

void testPersistance(String varName) {
    TimeserieLogger ts(varName.c_str(), SPIFFS);
    if (!ts.begin()) {
        Serial.println("error while starting ts logger on sub function");
        return;
    }

    Serial.printf(" [%s] : SUB ram size test 1\n", ts.getRamRecordCount() == ts.getMaxRamRecords() ? "PASS" : " # FAIL");
    Serial.printf(" [%s] : SUB file size test 1\n", ts.getFileRecordCount() == ts.getMaxRamRecords() + 1 ? "PASS" : " # FAIL");
}

void loop() {
    freeHeap = ESP.getFreeHeap();
    if (loopCount < 100) {
        TimeserieLogger tsLoop("testVal", SPIFFS);
        if (!tsLoop.begin()) {
            Serial.println("error while starting tsLoop logger");
            return;
        }
        Serial.printf(" [%s] : ram size after reconstruct in tsLoop %d\n", tsLoop.getRamRecordCount() == tmpRecordCount ? "PASS" : " # FAIL", loopCount);
        loopCount++;
    }

    Serial.printf(" [%s] : heap at loop %d\n", ESP.getFreeHeap() == freeHeap ? "PASS" : " # FAIL", loopCount);
    delay(10);
    if (loopCount == 100) {
        SPIFFS.remove("/testVal");
        delay(60000);
    }
}