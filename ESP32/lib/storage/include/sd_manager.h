/**
 * @file SDManager.h
 * @brief Handles SD card operations using the SD_MMC interface.
 */

#pragma once

#include "FS.h"
#include "SD_MMC.h"

/**
 * @class SDManager
 * @brief Manages the SD card filesystem for logging and image storage.
 */
class SDManager
{
private:
    const char* _root = "/";
    bool _is_mounted = false;

public:
    /**
     * @brief Mounts the SD card and initializes the MMC peripheral.
     * @return true if successfully mounted, false otherwise.
     */
    bool begin();

    /**
     * @brief Writes a camera frame buffer to a JPEG file on the SD card.
     * @param buf Pointer to the image data buffer.
     * @param len Size of the buffer in bytes.
     * @param path The full destination path (e.g., "/img/photo.jpg").
     * @return true if the file was written successfully.
     */
    bool save_frame(uint8_t* buf, size_t len, const char* path);

    /**
     * @brief Appends a string of text to a log file.
     * @param message The text to be appended.
     * @return true if written successfully.
     */
    bool append_log(const char* message);

    /**
     * @brief Calculates the total used space on the SD card in bytes.
     * @return Used space in bytes.
     */
    uint64_t get_used_space();

    /**
     * @brief Checks if the SD card is currently mounted and accessible.
     * @return true if present and mounted.
     */
    bool is_present();
};