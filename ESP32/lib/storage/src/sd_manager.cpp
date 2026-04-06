#include "Sd_manager.h"
#include <Arduino.h>

bool SDManager::begin()
{
    if (!SD_MMC.begin("/sdcard", true))
    {
        Serial.println("[SD] Error: SD Card Mount Failed");
        this->_is_mounted = false;
        return false;
    }

    uint8_t cardType = SD_MMC.cardType();
    if (cardType == CARD_NONE)
    {
        Serial.println("[SD] Error: No SD Card attached");
        this->_is_mounted = false;
        return false;
    }

    Serial.println("[SD] SD Card mounted successfully");
    this->_is_mounted = true;
    return true;
}

bool SDManager::save_frame(uint8_t* buf, size_t len, const char* path)
{
    if (!this->_is_mounted)
        return false;

    File file = SD_MMC.open(path, FILE_WRITE);
    if (!file)
    {
        Serial.printf("[SD] Failed to open file for writing: %s\n", path);
        return false;
    }

    size_t written = file.write(buf, len);
    file.close();

    if (written != len)
    {
        Serial.println("[SD] Write Error: Incomplete file written");
        return false;
    }

    return true;
}

bool SDManager::append_log(const char* message)
{
    if (!this->_is_mounted)
        return false;

    // Open logs.txt in append mode
    File file = SD_MMC.open("/logs.txt", FILE_APPEND);
    if (!file)
    {
        Serial.println("[SD] Failed to open log file");
        return false;
    }

    if (file.println(message))
    {
        file.close();
        return true;
    } else
    {
        Serial.println("[SD] Append failed");
        file.close();
        return false;
    }
}

uint64_t SDManager::get_used_space()
{
    if (!this->_is_mounted)
    {
        return 0;
    }

    return SD_MMC.usedBytes();
}

bool SDManager::is_present() { return this->_is_mounted && (SD_MMC.cardType() != CARD_NONE); }