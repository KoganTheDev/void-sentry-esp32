<!-- Header Section - Logo, Title and badges -->
<div align="center">
  <div style="display: flex; align-items: center; justify-content: center; gap: 20px;">
    <img src="assets/void_sentry_logo_for_md_file_400x400_pixels.png" width="150">
    <img src="assets/void_sentry_title.png" width="600">
  </div>
  
  <strong>The Autonomous Computer Vision & Kinetic Tracking System for ESP32-CAM</strong>

  ![Status](https://img.shields.io/badge/Status-Active_Development-ff6b00)
  [![Cppcheck Analysis](https://github.com/KoganTheDev/Smart_Camera_ESP32/actions/workflows/static_analysis.yml/badge.svg)](https://github.com/KoganTheDev/Smart_Camera_ESP32/actions/workflows/static_analysis.yml)
  [![Doxygen - Documentation Generator](https://github.com/KoganTheDev/Smart_Camera_ESP32/actions/workflows/doxygen_docs_generator.yml/badge.svg)](https://github.com/KoganTheDev/Smart_Camera_ESP32/actions/workflows/doxygen_docs_generator.yml)
</div>
<!-- Header Section - END -->

**Void Sentry** is a high-performance, embedded autonomous tracking solution built for the ESP32-CAM. By utilizing optimized **Frame Differencing** algorithms, the system performs real-time motion analysis on the "edge," isolating moving targets and generating kinetic coordinates for hardware-based tracking without the need for external processing.

---

## System Architecture
The system operates on a dual-core cycle to ensure low-latency video delivery while maintaining a high-frequency tracking loop. 


* **Computer Vision Pipeline**: Optimized for **QVGA (320x240)** resolution to maximize frames-per-second (FPS) and minimize computational jitter.
* **Kinetic Engine**: Translates motion "blob" centroids into PWM signals for high-torque servo response.
* **Visual Feedback**: A real-time Augmented Reality (AR) overlay is injected directly into the MJPEG stream, providing zero-latency target telemetry.

## Technical Specifications
| Feature | Implementation |
| :--- | :--- |
| **Detection Algorithm** | Temporal Frame Differencing (Greyscale) |
| **Tracking Logic** | Geometric Centroid Calculation (X, Y Coordinates) |
| **Noise Reduction** | Adaptive Hysteresis Thresholding |
| **I/O Protocol** | PWM (Pan/Tilt Servos) + HTTP MJPEG Stream |
| **Resolution** | 320x240 (Optimized for Edge Inference) |



## Core Capabilities
* **Intelligent Thresholding**: Dynamically filters out sensor noise and environment fluctuations (e.g., lighting changes) to prevent false positives.
* **Target Locking**: Implements "stickiness" logic to maintain focus on the primary moving mass while ignoring secondary background static.
* **Low-Latency Dashboard**: A web-based HUD designed for mobile and desktop, featuring real-time telemetry stats and manual override controls.
* **Modular Servo Control**: Easily adaptable for various Pan/Tilt hardware configurations.

<!-- TODO: Improve Setup when the wrapping things us-->

### Software Setup
1. Clone the repository:
   ```bash
   git clone [https://github.com/KoganTheDev/void-sentry-esp32.git](https://github.com/KoganTheDev/void-sentry-esp32.git)
   ```