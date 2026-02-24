<style>
  @import url('https://fonts.googleapis.com/css2?family=Orbitron:wght@900&display=swap');

  .title-gradient {
    font-family: 'Orbitron', sans-serif;
    font-weight: 900;
    font-style: italic;
    text-transform: uppercase;
    letter-spacing: 2px;
    background: linear-gradient(180deg, #ffffff 30%, #ff6b00 100%);
    -webkit-background-clip: text;
    -webkit-text-fill-color: transparent;
    display: inline-block;
    margin: 0;
    line-height: 1.2;
  }

  .header-container {
    display: flex;
    align-items: center;
    gap: 15px;
    padding: 20px;
    border-radius: 8px;
  }
</style>

<div class="header-container">
  <img src="logo/void_sentry_logo_for_md_file_400x400_pixels.png" alt="project_logo" height="80" width="80">
  <h1 class="title-gradient">VOID SENTRY</h1>
</div>


[![Cppcheck Analysis](https://github.com/KoganTheDev/Smart_Camera_ESP32/actions/workflows/static_analysis.yml/badge.svg)](https://github.com/KoganTheDev/Smart_Camera_ESP32/actions/workflows/static_analysis.yml) <!-- CPP check badge--> 
[![Doxygen - Documentation Generator](https://github.com/KoganTheDev/Smart_Camera_ESP32/actions/workflows/doxygen_docs_generator.yml/badge.svg)](https://github.com/KoganTheDev/Smart_Camera_ESP32/actions/workflows/doxygen_docs_generator.yml)

This project implements an autonomous motion-tracking system using an ESP32-CAM. Instead of static edge detection, the system uses Frame Differencing to isolate moving targets, calculate their centroids, and lock onto them in real-time. **Motion detection is visualized in real-time on the HTTP video stream with a green target overlay.**

---

## 🚀 Core Features
* **Frame Differencing**: Compares the current frame against a stored reference frame to identify pixels that have changed
* **Target Centroid Calculation**: Automatically calculates the X, Y center of the detected motion "blob" for precise tracking
* **Adaptive Thresholding**: Dynamically filters out sensor noise to prevent false motion triggers
* **Target Locking**: Maintains stable lock on moving objects while ignoring static backgrounds
* **Motion Visualization**: Real-time green crosshair + circle overlay on HTTP video stream showing detected motion
* **Servo Control**: Pan and tilt servos automatically track the detected target
* **Web Interface**: Live video streaming with motion detection visualization accessible via browser


