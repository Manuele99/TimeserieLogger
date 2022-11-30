//
// Created by Manuele Pola on 30/11/22.
//

#ifndef TIMESERIE_LOGGER_H
#define TIMESERIE_LOGGER_H

#include "BaseLogger.h"

class TimeserieLogger : public BaseLogger<double> {
public:
    explicit TimeserieLogger(const char *name, FS &fs = LittleFS) :
            BaseLogger<double>(name, fs) {};
};

#endif //TIMESERIE_LOGGER_H
