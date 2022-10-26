//
// Created by Manuele Pola on 13/10/22.
//

#include "TimeserieLogger.h"

TimeserieLogger::TimeserieLogger(const char *tsName, FS &fs) :
        fs(fs),
        filePath("/" + String(tsName)) {
    ts = (TimeserieLogger::timeserie *) calloc(1, sizeof(TimeserieLogger::timeserie));
    ts->name = tsName;
}

TimeserieLogger::~TimeserieLogger() {
    // write values to file
    this->syncFs();

    // free memory
    TimeserieLogger::record *val = ts->values;
    TimeserieLogger::record *next;
    while (val != nullptr) {
        next = val->next;
        free(val);
        val = next;
    }
    ts->values = nullptr;
    free(ts);
    ts = nullptr;
}

bool TimeserieLogger::begin() {
    return (filePath.lastIndexOf('/') == 0
            || fs.exists(filePath.substring(0, filePath.lastIndexOf('/')))
           ) && this->loadFromFS();
}

bool TimeserieLogger::addValue(uint32_t timestamp, double value) {
    auto *newRecord = (TimeserieLogger::record *) calloc(1, sizeof(TimeserieLogger::record));
    newRecord->timestamp = timestamp;
    newRecord->value = value;

    if (ramRecordCount == maxRamRecords) {
        if (fileRecordCount == 0) {
            // first record to overflow to file.
            // put all the records in ram first
            appendToFile(ts->values);
        }
        // limit reached for ram: write directly to file the new record
        bool ok = appendToFile(newRecord);
        free(newRecord);
        return ok;
    }

    ramRecordCount++;
    if (ts->values == nullptr) {
        ts->values = newRecord;
        return true;
    }

    TimeserieLogger::record *r = ts->values;
    while (r->next != nullptr) { r = r->next; }
    r->next = newRecord;
    return true;
}

bool TimeserieLogger::syncFs() {
    if (ramRecordCount <= fileRecordCount) {
        // nothing to do, because all the records in ram
        // are already written to file (the first n records on file, are always
        // also the first n records in ram)
        return true;
    }

    // skip to the first record not written to file
    TimeserieLogger::record *r = ts->values;
    for (int i = 0; i < ramRecordCount - fileRecordCount && r != nullptr; i++) {
        r = r->next;
    }
    if (r != nullptr) {
        // append to file all the remaining records
        return appendToFile(r);
    }
    return false;
}

uint8_t *TimeserieLogger::compact(TimeserieLogger::record *r) {
    auto *ser = (uint8_t *) calloc(1, RECORD_COMPACT_SIZE);
    memcpy(ser, &(r->timestamp), sizeof(r->timestamp));
    memcpy(ser + sizeof(r->timestamp), &(r->value), sizeof(r->value));
    return ser;
}

// persist the record at the end of the file
bool TimeserieLogger::appendToFile(TimeserieLogger::record *r) {
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
    TimeserieLogger::record *currentRecord = r;
    while (currentRecord != nullptr) {
        uint8_t *compactRecord = compact(currentRecord);
        f.write(compactRecord, RECORD_COMPACT_SIZE);
        free(compactRecord);
        fileRecordCount++;
        currentRecord = currentRecord->next;
    }
    f.close();
    return true;
}

// fill the ram with records from file
bool TimeserieLogger::loadFromFS() {
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
    fileRecordCount = f.size() / RECORD_COMPACT_SIZE;
    if (fileRecordCount == 0) {
        // no records in ram
        f.close();
        return true;
    }

    // load all the possible records from file
    uint8_t buffer[RECORD_COMPACT_SIZE];
    // skip records already in ram
    f.seek(ramRecordCount * RECORD_COMPACT_SIZE);

    while (ramRecordCount < maxRamRecords && f.available()) {
        if (f.read(buffer, RECORD_COMPACT_SIZE) != RECORD_COMPACT_SIZE) {
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

bool TimeserieLogger::dropRecords(uint8_t n) {
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
    f.seek(n * RECORD_COMPACT_SIZE);
    // now copy the remaining record
    uint8_t buff[RECORD_COMPACT_SIZE];
    while (f.available()) {
        if (f.read(buff, RECORD_COMPACT_SIZE) == RECORD_COMPACT_SIZE) {
            // write to file
            tempFile.write(buff, RECORD_COMPACT_SIZE);
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
    TimeserieLogger::record *r;
    uint8_t i = 0;
    while (i < n && ts->values != nullptr) {
        // move the head to the next record
        r = ts->values->next;
        free(ts->values);
        ts->values = r;
        i++;
    }
    ramRecordCount -= n;
    // advance the buffer window
    this->loadFromFS();
    return true;
}

TimeserieLogger::timeserie *TimeserieLogger::getTs() const {
    return ts;
}

#ifdef TIMESERIE_LOGGER_TEST

const String &TimeserieLogger::getFilePath() const {
    return filePath;
}

uint8_t TimeserieLogger::getRamRecordCount() const {
    return ramRecordCount;
}

uint8_t TimeserieLogger::getFileRecordCount() const {
    return fileRecordCount;
}

uint8_t TimeserieLogger::getMaxRamRecords() const {
    return maxRamRecords;
}

#endif // TIMESERIE_LOGGER_TEST