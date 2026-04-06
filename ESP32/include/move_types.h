/**
 * @file MoveTypes.h
 * @brief movement definitions for the Smart Camera project.
 * @details Uses a template base class to provide shared logic for axis-specific
 * movement classes while maintaining type safety and snake_case naming conventions.
 */

#pragma once

#include <stdint.h>

/**
 * @enum MoveXValue
 * @brief Raw underlying values for horizontal movement.
 */
enum class MoveXValue : uint8_t {
    None = 0,  /**< No horizontal movement. */
    Right = 1, /**< Movement to the right. */
    Left = 2   /**< Movement to the left. */
};

/** 
 * @enum MoveYValue
 * @brief Raw underlying values for vertical movement.
 */
enum class MoveYValue : uint8_t {
    None = 0, /**< No vertical movement. */
    Up = 1,   /**< Movement upwards. */
    Down = 2  /**< Movement downwards. */
};

/**
 * @class MoveDirection
 * @brief Base template providing common logic for movement directions.
 * @tparam EnumType The axis-specific enum (MoveXValue or MoveYValue).
 */
template <typename EnumType> class MoveDirection
{
public:
    /**
     * @brief Construct a new Move Direction object.
     * @param v The initial enum value.
     */
    constexpr MoveDirection(EnumType v) : _value(v) {}

    /** @brief Checks if the movement state is None (0). @return true if no movement. */
    bool is_none() const { return static_cast<uint8_t>(_value) == 0; }

    /** @brief Gets the underlying enum value. @return EnumType. */
    EnumType get_value() const { return _value; }

    /** @brief Logical NOT operator. @return true if direction is None. */
    bool operator!() const { return is_none(); }

    /** @brief Implicit conversion to uint8_t for hardware drivers. @return uint8_t value. */
    operator uint8_t() const { return static_cast<uint8_t>(_value); }

    /** @brief Equality comparison with underlying enum. @param v Value to compare. */
    bool operator==(EnumType v) const { return _value == v; }

    /** @brief Inequality comparison with underlying enum. @param v Value to compare. */
    bool operator!=(EnumType v) const { return _value != v; }

protected:
    EnumType _value; /**< Internal storage of the movement state. */
};

/**
 * @class MoveX
 * @brief Encapsulates horizontal movement states and utility methods.
 */
class MoveX : public MoveDirection<MoveXValue>
{
public:
    /** @brief Local alias for None state. */
    static constexpr MoveXValue None = MoveXValue::None;
    /** @brief Local alias for Right state. */
    static constexpr MoveXValue Right = MoveXValue::Right;
    /** @brief Local alias for Left state. */
    static constexpr MoveXValue Left = MoveXValue::Left;

    /** @brief Inherit base constructor logic. */
    using MoveDirection<MoveXValue>::MoveDirection;

    /** @brief Default constructor initializing to None. */
    constexpr MoveX() : MoveDirection(MoveXValue::None) {}

    /**
     * @brief Converts the state to a human-readable string.
     * @return const char* "Right", "Left", or "None".
     */
    const char* to_string() const
    {
        switch (this->_value)
        {
        case MoveXValue::Right:
            return "Right";
        case MoveXValue::Left:
            return "Left";
        default:
            return "None";
        }
    }
};

/**
 * @class MoveY
 * @brief Encapsulates vertical movement states and utility methods.
 */
class MoveY : public MoveDirection<MoveYValue>
{
public:
    /** @brief Local alias for None state. */
    static constexpr MoveYValue None = MoveYValue::None;
    /** @brief Local alias for Up state. */
    static constexpr MoveYValue Up = MoveYValue::Up;
    /** @brief Local alias for Down state. */
    static constexpr MoveYValue Down = MoveYValue::Down;

    /** @brief Inherit base constructor logic. */
    using MoveDirection<MoveYValue>::MoveDirection;

    /** @brief Default constructor initializing to None. */
    constexpr MoveY() : MoveDirection(MoveYValue::None) {}

    /**
     * @brief Converts the state to a human-readable string.
     * @return const char* "Up", "Down", or "None".
     */
    const char* to_string() const
    {
        switch (this->_value)
        {
        case MoveYValue::Up:
            return "Up";
        case MoveYValue::Down:
            return "Down";
        default:
            return "None";
        }
    }
};
