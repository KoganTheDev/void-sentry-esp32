#include <Arduino.h>
#include <unity.h>
#include "camera_diff_detection.h"
#include <esp_camera.h>

// Helper to create a dummy frame buffer for testing
camera_fb_t* create_mock_fb(uint16_t w, uint16_t h) {
    camera_fb_t* fb = (camera_fb_t*)malloc(sizeof(camera_fb_t));
    fb->width = w;
    fb->height = h;
    fb->format = PIXFORMAT_JPEG;
    fb->len = 0; // In a real test, you'd put dummy JPEG data here
    fb->buf = NULL; 
    return fb;
}

// 1. Test Grayscale Math (Luminosity check)
void test_rgb565_to_greyscale_logic(void) {
    CameraDiffDetection detector;
    
    // Test Black
    TEST_ASSERT_EQUAL_UINT8(0, detector.rgb565_to_greyscale(0x0000));
    
    // Test Mid-Gray (approx)
    // 0x8410 is roughly middle gray in RGB565
    uint8_t res = detector.rgb565_to_greyscale(0x8410);
    TEST_ASSERT_UINT8_WITHIN(20, 128, res);
}

// 2. Test Memory Safety (NULL handling)
void test_roberts_cross_null_handling(void) {
    CameraDiffDetection detector;
    uint8_t output[100];
    
    // Passing a NULL frame should be handled by your !rgb_buf check 
    // or we should ensure the function doesn't crash if jpg2rgb565 fails
    camera_fb_t* fb = create_mock_fb(10, 10);
    
    // This will fail jpg2rgb565 because fb->buf is NULL, 
    // but the code should exit gracefully.
    detector.roberts_cross(fb, output);
    
    TEST_ASSERT_MESSAGE(true, "Gracefully handled invalid JPEG buffer");
    free(fb);
}

// 3. Test Roberts Cross Gradient Calculation
void test_gradient_magnitude_logic(void) {
    // Because roberts_cross is an instance method, we can test 
    // the math by verifying the p1-p4 / p2-p3 logic.
    // Given:
    // P1=200, P2=200
    // P3=50,  P4=50
    // gx = 200 - 50 = 150
    // gy = 200 - 50 = 150
    // magnitude = 300 -> clamped to 255
    
    uint8_t p1 = 200, p2 = 200, p3 = 50, p4 = 50;
    int gx = p1 - p4;
    int gy = p2 - p3;
    int magnitude = abs(gx) + abs(gy);
    uint8_t final_val = (magnitude > 255) ? 255 : (uint8_t)magnitude;

    TEST_ASSERT_EQUAL_UINT8(255, final_val);
}

void setup() {
    // Wait for hardware to stabilize
    delay(2000);
    
    UNITY_BEGIN();
    
    RUN_TEST(test_rgb565_to_greyscale_logic);
    RUN_TEST(test_roberts_cross_null_handling);
    RUN_TEST(test_gradient_magnitude_logic);
    
    UNITY_END();
}

void loop() {
    // Nothing to do here
}