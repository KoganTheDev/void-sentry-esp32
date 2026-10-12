// THROWAWAY: proves the native_sanitize CI job catches memory bugs. Never merge.
#include <array>
#include <cstddef>
#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

void test_out_of_bounds_read(void)
{
    std::array<int, 4> values{1, 2, 3, 4};
    volatile size_t index = 4; // one past the end: AddressSanitizer must report a stack-buffer-overflow
    int value = values.data()[index];
    TEST_ASSERT_TRUE(value == value);
}

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_out_of_bounds_read);
    return UNITY_END();
}
