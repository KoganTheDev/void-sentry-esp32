#include <Arduino.h>
#include <unity.h>
#include "Sd_manager.h"

SDManager sd;

// Runs before every test
void setUp(void)
{
    // Note: SD_MMC.begin can only be called once successfully usually,
    // so we handle the actual begin in a dedicated test or here with a check.
    if (!sd.is_present())
    {
        sd.begin();
    }
}

// Runs after every test
void tearDown(void)
{
}

// 1. Test Hardware Connection
void test_sd_initialization(void)
{
    TEST_ASSERT_TRUE_MESSAGE(sd.is_present(), "SD Card is not detected or mounted");
    TEST_ASSERT_NOT_EQUAL(CARD_NONE, SD_MMC.cardType());
}

// 2. Test Binary Data Integrity (Simulating a camera frame)
void test_save_frame_integrity(void)
{
    const char *test_path = "/test_img.bin";
    const size_t test_size = 512;
    uint8_t fake_frame[test_size];

    // Fill buffer with recognizable pattern
    for (size_t i = 0; i < test_size; i++)
    {
        fake_frame[i] = i % 256;
    }

    // Test Writing
    TEST_ASSERT_TRUE_MESSAGE(sd.save_frame(fake_frame, test_size, test_path), "Failed to save binary frame");

    // Verify Integrity: Read back the file
    File file = SD_MMC.open(test_path, FILE_READ);
    TEST_ASSERT_TRUE_MESSAGE(file, "Failed to open file for verification");
    TEST_ASSERT_EQUAL_UINT32(test_size, file.size());

    uint8_t read_buffer[test_size];
    file.read(read_buffer, test_size);
    file.close();

    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(fake_frame, read_buffer, test_size, "Data read back does not match data written");
}

// 3. Test Log Appending
void test_append_log(void)
{
    const char *log_file = "/logs.txt";
    const char *msg1 = "Log entry 1";
    const char *msg2 = "Log entry 2";

    // Delete old logs for a fresh test
    if (SD_MMC.exists(log_file))
    {
        SD_MMC.remove(log_file);
    }

    TEST_ASSERT_TRUE(sd.append_log(msg1));
    TEST_ASSERT_TRUE(sd.append_log(msg2));

    File file = SD_MMC.open(log_file, FILE_READ);
    String content = file.readString();
    file.close();

    // Verify both messages exist in the file
    TEST_ASSERT_TRUE_MESSAGE(content.indexOf(msg1) >= 0, "First log entry missing");
    TEST_ASSERT_TRUE_MESSAGE(content.indexOf(msg2) >= 0, "Second log entry missing");
}

// 4. Test Utility Methods
void test_used_space(void)
{
    uint64_t used = sd.get_used_space();
    TEST_ASSERT_TRUE_MESSAGE(used > 0, "Used space reported as 0 on a card with files");
    Serial.printf("Total used space: %llu bytes\n", used);
}

// 5. Test Error Handling: Invalid Path
void test_invalid_path(void)
{
    uint8_t dummy = 0;
    // Attempting to write to a path that doesn't exist/invalid characters
    bool result = sd.save_frame(&dummy, 1, "");
    TEST_ASSERT_FALSE_MESSAGE(result, "Should have failed to write to empty path");
}

void setup()
{
    delay(2000); // Wait for board to stabilize
    Serial.begin(115200);

    UNITY_BEGIN();

    RUN_TEST(test_sd_initialization);
    RUN_TEST(test_save_frame_integrity);
    RUN_TEST(test_append_log);
    RUN_TEST(test_used_space);
    RUN_TEST(test_invalid_path);

    UNITY_END();
}

void loop() {}