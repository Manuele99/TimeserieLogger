//
// Created by Manuele Pola on 13/10/22.
//

#ifndef TIMESERIE_LOGGER_H
#define TIMESERIE_LOGGER_H

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>

#define RECORD_COMPACT_SIZE (sizeof(uint32_t) + sizeof(double))

class TimeserieLogger {
public:
    typedef struct record {
        uint32_t timestamp;
        double value;
        struct record *next;
    } record;

    typedef struct {
        const char *name;
        record *values;
    } timeserie;

    explicit TimeserieLogger(const char *tsName, FS &fs = LittleFS);

    ~TimeserieLogger();

    virtual bool begin();

    // append value to the timeserie
    bool addValue(uint32_t timestamp, double value);

    bool dropRecords(uint8_t n);

    timeserie *getTs() const;

    bool syncFs();

#ifdef TIMESERIE_LOGGER_TEST
    const String &getFilePath() const;

    uint8_t getRamRecordCount() const;

    uint8_t getFileRecordCount() const;

    uint8_t getMaxRamRecords() const;
#endif // TIMESERIE_LOGGER_TEST

protected:
    timeserie *ts;
    String filePath;
    FS &fs;

    // once exceeded this value, new records will be written directly to file
    uint8_t maxRamRecords = 20;

    // the current number of records in ram
    // the first n records in ram are always the same records in the fs. This is because we publish FIFO, so if the
    // publishing system fails, the first records are the most ridden
    uint8_t ramRecordCount = 0;

    // the current number of records in the fs
    uint8_t fileRecordCount = 0;

private:
    // return a serialized version of a record, of RECORD_COMPACT_SIZE bytes
    static uint8_t *compact(record *r);

    bool appendToFile(record *r);

    bool loadFromFS();
};

#endif //TIMESERIE_LOGGER_H
