//
// Created by Manuele Pola on 29/11/22.
//

#ifndef BASE_LOGGER_H
#define BASE_LOGGER_H

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>

template<typename T>
class BaseLogger {
public:
    typedef struct record {
        uint32_t timestamp;
        T value;
        struct record *next;
    } record;

    typedef struct {
        const char *name;
        record *values;
    } records;

    explicit BaseLogger(const char *name, FS &fs = LittleFS) : fs(fs), filePath("/" + String(name)) {
        recordsBuffer = (records *) calloc(1, sizeof(records));
        recordsBuffer->name = name;
        compactRecordSize = sizeof(uint32_t) + sizeof(T);
    };

    ~BaseLogger() {
        // write values to file
        this->syncFs();

        // free memory
        record *val = recordsBuffer->values;
        record *next;
        while (val != nullptr) {
            next = val->next;
            free(val);
            val = next;
        }
        recordsBuffer->values = nullptr;
        free(recordsBuffer);
        recordsBuffer = nullptr;
    }

    virtual bool begin() {
        return (filePath.lastIndexOf('/') == 0
                || fs.exists(filePath.substring(0, filePath.lastIndexOf('/')))
               ) && loadFromFS();
    }

    bool addValue(uint32_t timestamp, T value) {
        auto *newRecord = (record *) calloc(1, sizeof(record));
        newRecord->timestamp = timestamp;
        newRecord->value = value;

        if (ramRecordCount == maxRamRecords) {
            if (fileRecordCount == 0) {
                // first record to overflow to file.
                // put all the records in ram first
                appendToFile(recordsBuffer->values);
            }
            // limit reached for ram: write directly to file the new record
            bool ok = appendToFile(newRecord);
            free(newRecord);
            return ok;
        }

        ramRecordCount++;
        if (recordsBuffer->values == nullptr) {
            recordsBuffer->values = newRecord;
            return true;
        }

        record *r = recordsBuffer->values;
        while (r->next != nullptr) { r = r->next; }
        r->next = newRecord;
        return true;
    }

    bool dropRecords(uint8_t n) {
        if (!fs.exists(filePath)) {
            File file = fs.open(filePath, FILE_WRITE, true);
            if (file) { file.close(); }
            // nothing to drop
            return true;
        }

        File f = fs.open(filePath, FILE_READ);
        if (!f) {
            // something went wrong while opening the file
            return false;
        }

        // create a new temp file
        String tempFilePath = filePath + ".temp";
        File tempFile = fs.open(tempFilePath, FILE_WRITE, true);

        // drop first n records from file:
        uint16_t droppableRecords = fileRecordCount;
        if (n > droppableRecords) {
            // limit n to the max amount
            n = droppableRecords;
        } else if (droppableRecords == n) {
            // just replace the file with the empty one
            f.close();
            tempFile.close();

            fs.remove(filePath);
            fs.rename(tempFilePath, filePath);
        }

        // skip the records already in ram
        f.seek(n * compactRecordSize);
        // now copy the remaining record
        uint8_t buff[compactRecordSize];
        while (f.available()) {
            if (f.read(buff, compactRecordSize) == compactRecordSize) {
                // write to file
                tempFile.write(buff, compactRecordSize);
            } else {
                // something went wrong
                f.close();
                tempFile.close();
                // delete temp file
                fs.remove(tempFilePath);
                return false;
            }
        }
        fileRecordCount -= n;
        f.close();
        tempFile.close();
        fs.remove(filePath);
        fs.rename(tempFilePath, filePath);

        // now drop records in ram
        record *r;
        uint8_t i = 0;
        while (i < n && recordsBuffer->values != nullptr) {
            // move the head to the next record
            r = recordsBuffer->values->next;
            free(recordsBuffer->values);
            recordsBuffer->values = r;
            i++;
        }
        ramRecordCount -= n;
        // advance the buffer window
        this->loadFromFS();
        return true;
    }

    records *getRecords() const {
        return recordsBuffer;
    }

    bool syncFs() {
        if (ramRecordCount <= fileRecordCount) {
            // nothing to do, because all the records in ram
            // are already written to file (the first n records on file, are always
            // also the first n records in ram)
            return true;
        }

        // skip to the first record not written to file
        record *r = recordsBuffer->values;
        for (int i = 0; i < ramRecordCount - fileRecordCount && r != nullptr; i++) {
            r = r->next;
        }
        if (r != nullptr) {
            // append to file all the remaining records
            return appendToFile(r);
        }
        return false;
    }

#ifdef TIMESERIE_LOGGER_TEST

    const String &getFilePath() {
        return filePath;
    }

    uint8_t getRamRecordCount() const {
        return ramRecordCount;
    }

    uint8_t getFileRecordCount() const {
        return fileRecordCount;
    }

    uint8_t getMaxRamRecords() const {
        return maxRamRecords;
    }

#endif // TIMESERIE_LOGGER_TEST

protected:
    String filePath;
    FS &fs;
    records *recordsBuffer;
    size_t compactRecordSize = 0;

    // once exceeded this value, new records will be written directly to file
    uint8_t maxRamRecords = 20;

    // the current number of records in ram
    // the first n records in ram are always the same records in the fs. This is because we publish FIFO, so if the
    // publishing system fails, the first records are the most ridden
    uint8_t ramRecordCount = 0;

    // the current number of records in the fs
    uint8_t fileRecordCount = 0;

private:
    uint8_t *compact(record *r) {
        auto *ser = (uint8_t *) calloc(1, compactRecordSize);
        memcpy(ser, &(r->timestamp), sizeof(r->timestamp));
        memcpy(ser + sizeof(r->timestamp), &(r->value), sizeof(r->value));
        return ser;
    }

    // persist the record at the end of the file
    bool appendToFile(record *r) {
        // Trick to be sure that an empty file exist, and it's dimension is 0 (BUG in esp32)
        if (!fs.exists(filePath)) {
            File file = fs.open(filePath, FILE_WRITE, true);
            if (file) {
                file.close();
            }
        }

        File f = fs.open(filePath, FILE_APPEND);
        if (!f) {
            return false;
        }

        // write all the available records
        record *currentRecord = r;
        while (currentRecord != nullptr) {
            uint8_t *compactRecord = compact(currentRecord);
            f.write(compactRecord, compactRecordSize);
            free(compactRecord);
            fileRecordCount++;
            currentRecord = currentRecord->next;
        }
        f.close();
        return true;
    }

    // fill the ram with records from file
    bool loadFromFS() {
        // example
        // ram limit: 5
        // ram:         A, B, C
        // file:        A, B, C, D, E, F, G
        // ram after:   A, B, C, D, E


        if (!fs.exists(filePath)) {
            File file = fs.open(filePath, FILE_WRITE, true);
            if (file) {
                file.close();
            }
            // nothing to load
            return true;
        }

        // count records on file and load in ram
        File f = fs.open(filePath, FILE_READ);
        if (!f) {
            return false;
        }
        fileRecordCount = f.size() / compactRecordSize;
        if (fileRecordCount == 0) {
            // no records in ram
            f.close();
            return true;
        }

        // load all the possible records from file
        uint8_t buffer[compactRecordSize];
        // skip records already in ram
        f.seek(ramRecordCount * compactRecordSize);

        while (ramRecordCount < maxRamRecords && f.available()) {
            if (f.read(buffer, compactRecordSize) != compactRecordSize) {
                // something wrong!
                f.close();
                return false;
            } else {
                // the first value in the buffer is the timestamp, the second is the actual value
                this->addValue(*buffer, *(buffer + sizeof(uint32_t)));
            }
        }
        f.close();
        return true;
    }
};

#endif //BASE_LOGGER_H
