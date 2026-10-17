/**
 * @file form.h
 * @brief x-www-form-urlencoded decoding and input validation for the WiFi page (pure C++17, no heap).
 */

#pragma once

#include <cstddef>

namespace prov
{
constexpr size_t kMaxFormBody = 256;

/**
 * @brief Finds key=value in an x-www-form-urlencoded body, decodes %XX and '+', writes value + NUL.
 * @details False if the body is longer than kMaxFormBody, the key is missing, an escape is malformed (or decodes to
 * a NUL byte), or the decoded value (plus NUL) does not fit out_cap. @p out_len may be null.
 */
bool form_field(const char* body, size_t len, const char* key, char* out, size_t out_cap, size_t* out_len);
/** @brief SSID: 1-32 bytes. */
bool valid_ssid(const char* s, size_t len);
/** @brief Password: empty, or 8-63 bytes of printable ASCII (32-126). */
bool valid_password(const char* s, size_t len);
/** @brief PIN: 4-8 ASCII digits. */
bool valid_pin(const char* s, size_t len);
} // namespace prov
