#include "button_classifier.h"

namespace aiming
{
ButtonEvent ButtonClassifier::update(bool pressed_raw, uint32_t now_ms)
{
    if (pressed_raw != raw_)
    {
        raw_ = pressed_raw;
        raw_since_ms_ = now_ms;
    }
    if (raw_ != stable_ && static_cast<uint32_t>(now_ms - raw_since_ms_) >= config_.debounce_ms)
    {
        stable_ = raw_;
        if (stable_)
        {
            pressed_at_ms_ = now_ms;
            long_fired_ = false;
            return ButtonEvent::None;
        }
        return long_fired_ ? ButtonEvent::None : ButtonEvent::Short;
    }
    if (stable_ && !long_fired_ && static_cast<uint32_t>(now_ms - pressed_at_ms_) >= config_.long_ms)
    {
        long_fired_ = true;
        return ButtonEvent::Long;
    }
    return ButtonEvent::None;
}
} // namespace aiming
