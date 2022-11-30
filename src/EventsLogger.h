//
// Created by Manuele Pola on 29/11/22.
//

#ifndef EVENTS_LOGGER_H
#define EVENTS_LOGGER_H

#include "BaseLogger.h"

class EventsLogger : public BaseLogger<bool> {
public:
    explicit EventsLogger(const char *name, FS &fs = LittleFS) :
            BaseLogger<bool>(name, fs) {};
};

#endif //EVENTS_LOGGER_H
