This branch incorporates changes that allow sending eyetracking data over USB to Lumineye, an ESP32S3 based LCD eye and central control system for fursuits.
This isn't necessary for getting Lumineye working and is an expensive, laborious feature that adds QoL for LCD eye users and more features.
Link to the Lumineye project: [link](https://github.com/WolfieTheWolfer/Lumineye)

A lot of the changes in here are made for my setup, which is not as fancy as Fraxul's, but is more accessible for those who are interested in the project.

My reference test/development platform:
- Jetson Orin Nano 8GB Developer Kit running L4T r36.5.0 / JetPack 6.2.2
  - If you're also using a Nano and plan on using eye/facetracking, the engines need to be recompiled to use GPU processing due to the Nano lacking DLA.
- 2x [Yahboom IMX219](https://www.amazon.com/dp/B0C5844MWQ) sensors: 2 lane, 720p, 60fps.
  - There has been no modification to the cameras beyond covering the FPC in electrical tape.
  - These are the main FPV cameras.
- 2x [Arducam OV9281](https://www.amazon.com/dp/B096M5DKY6) sensors: USB, 720p, 100fps
  - These are used for the eye trackers.
  - I recommend using IR LEDs in conjunction to illuminate your eyes for the camera.
- Samsung Odyssey+ (2019) HMD: 2880x1440 at 90hz.
- Depth processing using the Orin's OFA hardware.

# Original Description:

# hmdcam: An indirect vision system for fursuit pilots


This application is designed for camera passthrough to a head-mounted display.
It runs on the Nvidia Jetson Orin family of SoCs and has been developed/tested against 1st-gen Windows Mixed Reality HMDs (Lenovo Explorer, HP VR1000), but should work on any HMD supported by Monado.

Features:
- Distortion-corrected monoscopic and stereoscopic views positioned in 3d space
- Depth-mapped stereoscopic views to support off-axis viewing and multiview stitching
  - Both horizontal (left-right) and vertical (top-bottom) stereo pairs are supported
  - Depending on your camera configuration and platform, this may require an additional discrete GPU or VPU for processing.
  - Supports multiple depth processing backends:
    - Jetson Orin's integrated Optical Flow Accelerator hardware (Recommended, supports up to 2 stereo pairs, 1920x1080 at 90fps)
    - Luxonis DepthAI VPU modules
    - OpenCV+CUDA on a separate Nvidia discrete GPU (Not recommended for battery-powered systems)
- Configuration menu with built-in calibration tools
- Remote viewing via RTSP (embedded live555 server + nvenc)
- Remote depth processing debugging (`debug-client` binary)

Important repository structure:
| Path           | Description  |
| -------------- | ------------ |
| hmdcam         |  Main application -- runs on the Jetson |
| debug-client   | Remote-debugging application; streams uncompressed framebuffers from `hmdcam` over Ethernet. |
| dgpu-worker    | Stereo disparity computation worker for CUDA discrete GPUs. Runs under `hmdcam` or `debug-client` |
| depthai-worker | Stereo disparity computation worker for Luxonis DepthAI VPUs. Runs under `hmdcam` or `debug-client` |
| rhi            | Render Hardware Interface -- wrappers over OpenGL. |
| common         | Library functions shared between `hmdcam` and `debug-client` (mostly camera related) |

Requirements:
- A Jetson Orin board with one or more CSI cameras
  - The cameras should be capable of capturing at the display rate of your HMD (90fps for the Vive or Explorer)
  - For an 8-lane Jetson SoM, 2-lane IMX662 sensors are preferred: they can capture 1920x1080 at up to 90fps.
  - If you have CSI lanes to spare, 4-lane IMX290 sensors are a good option: they can capture 1920x1080 at up to 120fps
  - 2-lane IMX219 sensors are an acceptable fallback option, capturing 1280x720 at up to 120fps
- A head-mounted display supported by Monado
  - First-generation Windows Mixed Reality HMDs are excellent candidates: they are inexpensive, compact, and have reasonably good optics and displays.
  - A stripped-down WMR HMD can be made even smaller by replacing its captive cable with [this breakout board](https://github.com/Fraxul/Explorer-Breakout)
- (Optional) Luxonis DepthAI VPUs for stereo processing. Discrete Nvidia GPUs are also supported for CUDA-based stereo processing, but not recommended due to very high power consumption.
  - One VPU per stereo pair with 1:4 sampling (480x270 depth resolution). Each VPU consumes about 2 watts (tested using [OAK-FFC-3P](https://docs.luxonis.com/projects/hardware/en/latest/pages/DM1090.html) modules without attached cameras)
  - A GTX1050 MXM3 module can handle 2 stereo pairs with 1:4 sampling (480x270 depth resolution) at 90+ FPS, but consumes ~40 watts.

The reference test/development platform is:
- Jetson Orin NX running L4T r36.4.3 / JetPack 6.2
- 4x [Soho Enterprise](https://soho-enterprise.com) SE-SB2M-IMX662 sensors: 2-lane, 1920x1080, 90fps.
  - These sensors use a custom kernel driver that I haven't published yet
  - Lenses are [Commonlands CIL028-F2.6-M12ANIR](https://commonlands.com/products/wideangle-3mm-lens)
  - Lens holders: [M12, 7mm high](https://www.aliexpress.us/item/2255801127009273.html)
  - Arranged as two 90°-rotated horizontal stereo pairs.
- Lenovo Explorer HMD: 2880x1440 at 90fps.
- Depth processing using the Orin's OFA hardware.

