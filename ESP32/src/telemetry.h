/**
 * @file telemetry.h
 * @brief Periodic benchmark stats lines (every 5 s, from loop()).
 * @details Split out of main.cpp. The objects it reads stay globals in main.cpp and are passed by reference. The
 * lines are the Stage 1 benchmark format (see telemetry.cpp); never prints credentials.
 */

#pragma once

#include "camera_diff_detection.h"
#include "camera_task.h"
#include "controller.h"
#include "frame_store.h"
#include "turret.h"
#include "turret_server.h"
#include "wifi_manager.h"

namespace telemetry
{
/** @brief The objects the stats lines read. All references must outlive the program (they are globals). */
struct Sources {
    FrameStore& frame_store;
    CameraTaskStats& camera_stats;
    CameraDiffDetection& detection;
    HttpServer& http_server;
    Turret& turret;
    Controller& controller;
    WifiManager& wifi;
};

/** @brief Log the two stats lines if 5 s passed since the last call; cheap otherwise. Call from loop(). */
void log_stats(const Sources& src);
} // namespace telemetry
