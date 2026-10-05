# CocoaGuard — Virtual IoT + Gemini AI Prototype

CocoaGuard is a student smart-agriculture prototype for monitoring environmental conditions around cocoa plants and generating a cautious Gemini-based visual advisory from an optional cocoa image.

> **Prototype note:** The environmental thresholds are for demonstration only and are not a field-validated agronomic model. Gemini visual analysis is advisory and does not confirm a plant disease.

## What this project demonstrates

### IoT layer
- Seeed Studio XIAO ESP32-S3 in Wokwi
- Five adjustable virtual inputs represented by potentiometers:
  - temperature
  - humidity
  - soil moisture
  - leaf wetness
  - rain intensity
- OLED live status display
- Green / yellow / red risk LEDs
- Buzzer for HIGH risk
- Transparent local environmental risk engine

The current competition build uses **virtual potentiometers** for all five inputs. A DHT22 and physical moisture/wetness/rain sensors are part of the proposed physical implementation, not the current Wokwi simulation.

### AI layer
- Flask dashboard
- Live readings from the Wokwi RFC2217 serial port
- Optional cocoa pod / leaf / stem image upload
- Gemini multimodal analysis
- Structured advisory with:
  - overall risk
  - visual observation
  - sensor interpretation
  - recommended action
  - confidence / limitation note

## Repository structure

```text
CocoaGuard_GitHub_Ready/
├─ .gitignore
├─ README.md
├─ firmware/
│  ├─ .vscode/extensions.json
│  ├─ .gitignore
│  ├─ platformio.ini
│  ├─ wokwi.toml
│  ├─ diagram.json
│  └─ src/main.cpp
└─ dashboard/
   ├─ app.py
   ├─ gemini_prompt.txt
   ├─ requirements.txt
   ├─ .env.example
   ├─ templates/index.html
   └─ static/style.css
```

## 1. Run the Wokwi prototype

1. Open the `firmware` folder in VS Code.
2. Install the PlatformIO IDE and Wokwi Simulator extensions.
3. Build the PlatformIO project.
4. Start the Wokwi simulation.
5. Adjust the five virtual knobs for temperature, humidity, soil moisture, leaf wetness, and rain intensity.
6. Observe the OLED, LEDs, buzzer, and serial output.

The firmware emits machine-readable lines beginning with `CGJSON:`. `wokwi.toml` exposes the simulator serial port through RFC2217 on TCP port `4000`.

## 2. Run the Flask + Gemini dashboard

Open a second terminal:

```bash
cd dashboard
python -m venv .venv
```

### Windows

```bash
.venv\Scripts\activate
```

### macOS / Linux

```bash
source .venv/bin/activate
```

Install dependencies:

```bash
pip install -r requirements.txt
```

Copy `.env.example` to `.env`, then add your own Gemini API key:

```text
GEMINI_API_KEY=your_key_here
GEMINI_MODEL=gemini-3.8-flash
COCOAGUARD_SERIAL_URL=rfc2217://localhost:4000
```

Run the dashboard:

```bash
python app.py
```

Open:

```text
http://127.0.0.1:5000
```

## 3. Image analysis

The repository intentionally contains **no sample cocoa photos**. Images are selected by the user at runtime, read in memory, sent to Gemini for analysis, and are not saved by this Flask app.

Accepted upload formats:
- JPEG
- PNG
- WebP
- HEIC
- HEIF

Maximum upload size: **8 MB**.

## 4. Suggested demo flow

1. Start with a LOW-risk sensor combination.
2. Increase humidity, leaf wetness, and/or rain until MODERATE appears.
3. Increase the values further until HIGH appears and the red LED/buzzer activate.
4. Open the dashboard and show the same live readings.
5. Optionally upload a cocoa plant image.
6. Run Gemini analysis and explain the advisory plus its limitations.

## Security before publishing

- Never commit `dashboard/.env`.
- Never commit a real Gemini API key.
- Do not commit `.venv/` or `.pio/`; both are generated locally.
- Use `.env.example` only as a template.
- If a key has ever been exposed publicly, revoke/rotate it in Google AI Studio immediately.

## Competition integrity

Keep `dashboard/gemini_prompt.txt` with the project if the competition requires disclosure of the exact AI prompt. Confirm that the final implementation and any external assistance comply with the organizer's rules before submission.
