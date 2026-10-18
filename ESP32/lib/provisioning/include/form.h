/**
 * @file form.h
 * @brief x-www-form-urlencoded decoding and input validation for the WiFi page (pure C++20, no heap).
 */

#pragma once

#include <cstddef>
#include <string_view>

namespace prov
{
constexpr size_t kMaxFormBody = 256;

/**
 * @brief Finds key=value in an x-www-form-urlencoded body, decodes %XX and '+', writes value + NUL.
 * @details False if the body is longer than kMaxFormBody, the key is missing, an escape is malformed (or decodes to
 * a NUL byte), or the decoded value (plus NUL) does not fit out_cap. @p out_len may be null. The views need no NUL
 * terminator; an empty body never matches.
 */
bool form_field(std::string_view body, std::string_view key, char* out, size_t out_cap, size_t* out_len);
/** @brief True if the body contains key= (even with an empty or malformed value). Lets callers tell missing from
 * invalid. */
bool form_has_key(std::string_view body, std::string_view key);
/** @brief SSID: 1-32 bytes. */
bool valid_ssid(std::string_view s);
/** @brief Password: empty, or 8-63 bytes of printable ASCII (32-126). */
bool valid_password(std::string_view s);
/** @brief PIN: 4-8 ASCII digits. */
bool valid_pin(std::string_view s);
} // namespace prov
