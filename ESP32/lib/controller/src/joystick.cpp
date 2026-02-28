#include "Joystick.h"

void Joystick::begin()
{
    pinMode(this->_pin_x, INPUT);
    pinMode(this->_pin_y, INPUT);
    pinMode(this->_pin_z, INPUT_PULLUP);

    analogReadResolution(JOYSTICK_RESOLUTION_BITS);

    long sum_x = 0;
    long sum_y = 0;

    const uint8_t average_factor = 10;
    // Average location to decide on initial position
    for (int i = 0; i < average_factor; i++)
    {
        sum_x += analogRead(this->_pin_x);
        sum_y += analogRead(this->_pin_y);
        delay(10);
    }
    this->_center_x = sum_x / average_factor;
    this->_center_y = sum_y / average_factor;
}

int Joystick::_read_raw(uint8_t pin) const
{
    long sum = 0;
    for (int i = 0; i < JOYSTICK_SAMPLES; i++)
    {
        sum += analogRead(pin);
    }
    return sum / JOYSTICK_SAMPLES;
}

int Joystick::_process_axis(int raw_val, int center) const
{
    int deflection = raw_val - center;

    // Filter out values inside the deadzone
    if (abs(deflection) < this->_deadzone)
    {
        return 0;
    }
    return deflection;
}

int Joystick::_map_speed(int val, int min_out, int max_out) const
{
    if (val == 0)
    {
        return 0;
    }

    // Map the 12-bit range to your desired speed range
    return map(val, -2048, 2048, min_out, max_out);
}

bool Joystick::is_z_pressed()
{
    bool current_reading = digitalRead(this->_pin_z);
    bool pressed = false;

    // 1. If the physical state changed, reset the timer
    if (current_reading != _last_btn_state)
    {
        _last_debounce_time = millis();
    }

    // 2. Check if the state has been stable for long enough
    if ((millis() - _last_debounce_time) > BUTTON_DEBOUNCE_MS)
    {
        // We only care if the confirmed stable state is different from
        // what we previously processed.
        static bool confirmed_state = HIGH;

        if (current_reading != confirmed_state)
        {
            confirmed_state = current_reading;

            // Trigger only when the confirmed state goes LOW (Pressed)
            if (confirmed_state == LOW)
            {
                pressed = true;
            }
        }
    }

    _last_btn_state = current_reading; // Always track the very last reading
    return pressed;
}

bool Joystick::is_active() const { return (get_x() != 0 || get_y() != 0); }