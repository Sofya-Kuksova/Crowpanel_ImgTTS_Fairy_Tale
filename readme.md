# CrowPanel Image TTS Cards — ESP32-S3 Firmware for Voicing Fairy-Tale via tinyTTS

**Summary.** Firmware for the ELECROW CrowPanel Advance 5.0 (ESP32-S3, 800×480) with an LVGL UI that displays fairy-tale photo cards and plays the text associated with each card through the tinyTTS module over UART.  
The application operates on a single **Image screen**: it shows the current card image, automatically narrates a fragment of the story, and then offers two interactive choices that guide the branching storyline.

The current firmware build ships with a single **fairy_tale** card scenario (fairy-tale themed cards and texts).

---

## Repository Structure

- `firmware/` — flashing utility and prebuilt binaries. 
- `main/` — firmware sources (ESP-IDF): LVGL UI, UART manager, tinyTTS integration, asset/image subsystem.
- `components/ui/` — SquareLine Studio project and exported LVGL resources.
- `spiffs_root/` — asset sources:
   - `spiffs_root/assets/` — static UI assets (icons);
   - `spiffs_root/assets_fairy_tale/` — RAW frames (binary image representations) resources for the **fairy_tale** scenario.  
  All assets are packed into SPIFFS (`spiffs.bin`) at build time.

---

## Features

- **SPIFFS → PSRAM → LVGL pipeline:** RAW frames are stored in the SPIFFS flash FS. Before display, a frame is read **entirely** into a **PSRAM** buffer and rendered via the LVGL/driver stack.
- **Image ↔ text binding:** each card has a descriptive text for speech playback.
- **Embedded device focus:** Designed for ELECROW CrowPanel Advance 5.0-HMI. For detailed device hardware information, see [Device Hardware Documentation](https://www.elecrow.com/pub/wiki/CrowPanel_Advance_5.0-HMI_ESP32_AI_Display.html).  
- **tinyTTS control:** Load text into the buffer, trigger playback, monitor playback status, and adjust volume using the GRC tinyTTS module. For details, see [tinyTTS repository](https://github.com/Grovety/HxTTS).  
- **Persistent settings:** Saved to NVS / file for convenient reuse.  

---

# Usage 

> **IMPORTANT:** Order of actions matters — first connect the CrowPanel to your computer via USB/Serial, then flash the board, then connect the GRC tinyTTS module.

1. **Hardware setup (initial)**  
   - Set the function-select switch on the CrowPanel to **WM**(0,1) (UART1-OUT mode).  
   - Connect the CrowPanel to the PC via USB/Serial.  
   - **Do NOT connect the GRC tinyTTS module yet.** The tinyTTS module must be connected only after the panel firmware has been flashed (see note below).  

2. **Flashing the board**

#### Build from sources
1. Install ESP-IDF framework v5.4.  
2. ```git lfs install```
3. Clone the repository.
4. Dependencies are managed via the ESP-IDF component manager (`idf_component.yml`).  
5. Build and flash:
```bash
idf.py build
idf.py -p PORT flash
```

 ### Flashing Prebuilt Images
Use the strict flasher in `firmware/` (see `firmware/flash_tool.md`).

Required files inside the selected folder:
- `bootloader.bin` @ `0x0000`
- `partition-table.bin` @ `0x8000`
- `proj.bin` @ `0x10000`
- `spiffs.bin` @ `0x110000`

3. **Hardware setup (post-flash)**  
   - After flashing completes and the device has rebooted, connect the GRC tinyTTS module (UART0) and CrowPanel (UART1-OUT) using a 4-pin adapter.  
   - Connect the audio output (3.5 mm mini jack) from the tinyTTS module to the speaker/headphones.

---

# Asset Subsystem (SPIFFS → PSRAM → LVGL)

- **Storage:** RAW frames reside in SPIFFS.
- **Rendering:** a frame is read **entirely** into a PSRAM buffer, then displayed via LVGL/driver.

---

# UI (SquareLine Studio / LVGL)

- UI sources: `components/ui/` (SquareLine project + generated LVGL files).
- **Image screen:**
  1. When the story is set to a new state, the corresponding card image is loaded from SPIFFS and displayed in `ui_img`.  
  2. Approximately one second after the image is rendered, the firmware sends the next story fragment to tinyTTS and starts speech playback. During this phase, no choice buttons are shown. The user only listens to the narration.
  3. After tinyTTS finishes playing the current fragment, two LVGL buttons become visible:
    - `ui_choice1`
    - `ui_choice2`  
   Each button represents one of the possible actions or decisions for the story protagonist.
  4. When the user presses `ui_choice1` or `ui_choice2`, the UI layer notifies the story controller, which:
    - updates the internal story node according to the selected branch,
    - selects the next card image and text fragment,
    - triggers the same sequence again (display image → 1-second delay → narrate fragment → show choices).
  5. The branching logic is finite and leads to one of **16 distinct endings**.  
   The sequence of user choices across all decision points determines which of the 16 final endings is reached.
  6. When the story reaches a terminal node (one of the 16 distinct endings) and tinyTTS finishes playing the final fragment, an `ui_end` button is displayed. Pressing `ui_end` resets the story controller to the initial state, reloads the first card, and allows the user to replay the interactive story to explore an alternative path and reach a different ending.

![](images/ui_image1.png)
![](images/ui_image2.png)
---

## Dependencies

- **ESP-IDF Components:** all dependencies are declared in `idf_component.yml` and fetched automatically by the ESP-IDF component manager.
- **LVGL 8.4:** UI rendering (exported from SquareLine Studio).
- **GRC tinyTTS integration:** Serial/UART transport and the module’s register-level protocol.
