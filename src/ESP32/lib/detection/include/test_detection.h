#pragma once

#include "base_detection_module.h"

#include <Arduino.h>
#include <stdlib.h>
#include <time.h>

#include "move_types.h"

class TestDetection : public BaseDetectionModule
{
public:
    TestDetection() {}
    ~TestDetection() {}

    std::tuple<MoveX, MoveY> detect_object(camera_fb_t* frame)
    {
        srand(time(0));

        int min = 0;
        int max = 2;

        MoveX random_number_x = MoveX((rand() % (max - min + 1)) + min);
        MoveY random_number_y = MoveY((rand() % (max - min + 1)) + min);

        return std::make_tuple(random_number_x, random_number_y);
    };
};
