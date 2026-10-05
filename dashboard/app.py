import json
import os
import re
import threading
import time
from pathlib import Path

import serial

from dotenv import load_dotenv
from flask import Flask, jsonify, render_template, request

from google import genai
from google.genai import types


# =========================================================
# COCOAGUARD AI DASHBOARD
# Wokwi IoT + Flask + Gemini AI
# =========================================================


# ---------------------------------------------------------
# LOAD ENVIRONMENT VARIABLES
# ---------------------------------------------------------

load_dotenv()


# ---------------------------------------------------------
# FLASK
# ---------------------------------------------------------

app = Flask(__name__)

# Keep image uploads small enough for a lightweight local prototype.
app.config["MAX_CONTENT_LENGTH"] = 8 * 1024 * 1024

ALLOWED_IMAGE_MIME_TYPES = {
    "image/jpeg",
    "image/png",
    "image/webp",
    "image/heic",
    "image/heif",
}


# ---------------------------------------------------------
# CONFIGURATION
# ---------------------------------------------------------

SERIAL_URL = os.getenv(
    "COCOAGUARD_SERIAL_URL",
    "rfc2217://localhost:4000"
)

GEMINI_MODEL = os.getenv(
    "GEMINI_MODEL",
    "gemini-3.8-flash"
)

API_KEY = os.getenv(
    "GEMINI_API_KEY",
    ""
).strip()


# ---------------------------------------------------------
# GEMINI FALLBACK MODELS
# ---------------------------------------------------------
#
# If Gemini 3.8 Flash is temporarily overloaded,
# CocoaGuard automatically tries another Gemini model.
#

FALLBACK_MODELS = [
    GEMINI_MODEL,
    "gemini-3.7-flash",
    "gemini-3.6-flash",
]


# Remove duplicates while keeping order

FALLBACK_MODELS = list(
    dict.fromkeys(FALLBACK_MODELS)
)


# ---------------------------------------------------------
# LIVE SENSOR STORAGE
# ---------------------------------------------------------

sensor_lock = threading.Lock()

sensor_data = {

    "temperature": 27.0,

    "humidity": 75.0,

    "soil": 55.0,

    "leaf_wetness": 25.0,

    "rain": 10.0,

    "risk": 20.0,

    "status": "LOW",

    "connected": False,

    "updated_at": None,

}


# ---------------------------------------------------------
# LOAD GEMINI PROMPT
# ---------------------------------------------------------

PROMPT_PATH = Path(
    __file__
).with_name(
    "gemini_prompt.txt"
)

GEMINI_PROMPT = PROMPT_PATH.read_text(
    encoding="utf-8"
)


# =========================================================
# WOKWI SERIAL WORKER
# =========================================================

def serial_worker():

    global sensor_data

    while True:

        ser = None

        try:

            print(
                f"[CocoaGuard] Connecting to Wokwi at {SERIAL_URL}"
            )

            ser = serial.serial_for_url(

                SERIAL_URL,

                baudrate=115200,

                timeout=2
            )

            print(
                "[CocoaGuard] Wokwi serial connected."
            )

            with sensor_lock:

                sensor_data[
                    "connected"
                ] = True

            # ---------------------------------------------
            # READ SERIAL CONTINUOUSLY
            # ---------------------------------------------

            while True:

                line = ser.readline().decode(
                    "utf-8",
                    errors="ignore"
                ).strip()

                if not line:
                    continue

                # Dashboard only reads machine-readable
                # CocoaGuard lines from the ESP32.

                if not line.startswith(
                    "CGJSON:"
                ):
                    continue

                payload = line[
                    len("CGJSON:"):
                ]

                try:

                    parsed = json.loads(
                        payload
                    )

                except json.JSONDecodeError:

                    print(
                        "[CocoaGuard] Invalid JSON received."
                    )

                    continue

                parsed[
                    "connected"
                ] = True

                parsed[
                    "updated_at"
                ] = time.strftime(
                    "%H:%M:%S"
                )

                with sensor_lock:

                    sensor_data = parsed


        except Exception as exc:

            print(
                "[CocoaGuard] Wokwi connection error:"
            )

            print(
                exc
            )

            with sensor_lock:

                sensor_data[
                    "connected"
                ] = False

            time.sleep(
                2
            )


        finally:

            if ser:

                try:

                    ser.close()

                except Exception:

                    pass


# =========================================================
# HOME PAGE
# =========================================================

@app.route("/")
def index():

    return render_template(
        "index.html"
    )


# =========================================================
# SENSOR API
# =========================================================

@app.route(
    "/api/sensors"
)
def sensors():

    with sensor_lock:

        return jsonify(
            dict(
                sensor_data
            )
        )


# =========================================================
# CLEAN GEMINI JSON RESPONSE
# =========================================================

def clean_json_text(text):

    text = (
        text
        or ""
    ).strip()

    # Remove markdown JSON fences if Gemini adds them

    text = re.sub(

        r"^```(?:json)?\s*",

        "",

        text,

        flags=re.I
    )

    text = re.sub(

        r"\s*```$",

        "",

        text
    )

    return text.strip()


# =========================================================
# CHECK WHETHER ERROR IS TEMPORARY
# =========================================================

def is_temporary_gemini_error(
    error_text
):

    value = (
        error_text
        or ""
    ).lower()

    temporary_signals = [

        "503",

        "unavailable",

        "high demand",

        "overloaded",

        "429",

        "resource_exhausted",

        "temporarily",

        "timeout",
    ]

    return any(

        signal in value

        for signal in temporary_signals
    )


# =========================================================
# GEMINI ANALYSIS API
# =========================================================

@app.route(
    "/api/analyze",
    methods=["POST"]
)
def analyze():

    # -----------------------------------------------------
    # CHECK API KEY
    # -----------------------------------------------------

    if not API_KEY:

        return jsonify({

            "ok": False,

            "error":
                "Gemini API key is not configured."

        }), 400


    # -----------------------------------------------------
    # GET CURRENT LIVE SENSOR DATA
    # -----------------------------------------------------

    with sensor_lock:

        snapshot = dict(
            sensor_data
        )


    # -----------------------------------------------------
    # BUILD SENSOR CONTEXT
    # -----------------------------------------------------

    sensor_context = (

        "\n\n"
        "CURRENT LIVE COCOAGUARD SENSOR DATA:\n"

        + json.dumps(
            snapshot,
            indent=2
        )

    )


    # -----------------------------------------------------
    # BASE GEMINI CONTENT
    # -----------------------------------------------------

    contents = [

        GEMINI_PROMPT
        + sensor_context

    ]


    # -----------------------------------------------------
    # OPTIONAL COCOA PHOTO
    # -----------------------------------------------------

    image_file = request.files.get(
        "image"
    )

    if (
        image_file
        and image_file.filename
    ):

        mime_type = (
            image_file.mimetype
            or ""
        ).lower()

        if mime_type not in ALLOWED_IMAGE_MIME_TYPES:
            return jsonify({
                "ok": False,
                "error": (
                    "Unsupported image format. "
                    "Use JPEG, PNG, WebP, HEIC, or HEIF."
                ),
            }), 400

        image_bytes = (
            image_file.read()
        )

        if not image_bytes:
            return jsonify({
                "ok": False,
                "error": "The selected image file is empty.",
            }), 400

        image_part = (
            types.Part.from_bytes(

                data=image_bytes,

                mime_type=mime_type
            )
        )

        # Put image before text prompt

        contents.insert(
            0,
            image_part
        )


    # -----------------------------------------------------
    # GEMINI CLIENT
    # -----------------------------------------------------

    client = genai.Client(
        api_key=API_KEY
    )


    # =====================================================
    # AUTOMATIC MODEL FALLBACK
    # =====================================================

    last_error = None


    for index, model_name in enumerate(
        FALLBACK_MODELS
    ):

        try:

            print()
            print(
                "======================================"
            )

            print(
                f"[CocoaGuard AI] Trying model: {model_name}"
            )

            print(
                "======================================"
            )


            # ---------------------------------------------
            # GEMINI REQUEST
            # ---------------------------------------------

            response = (
                client.models.generate_content(

                    model=model_name,

                    contents=contents,

                    config=(
                        types.GenerateContentConfig(

                            temperature=0.2,

                            response_mime_type=(
                                "application/json"
                            ),
                        )
                    ),
                )
            )


            # ---------------------------------------------
            # CLEAN RESPONSE
            # ---------------------------------------------

            raw = clean_json_text(
                response.text
            )


            # ---------------------------------------------
            # PARSE RESPONSE
            # ---------------------------------------------

            try:

                report = json.loads(
                    raw
                )


            except json.JSONDecodeError:

                report = {

                    "overall_risk":
                        snapshot.get(
                            "status",
                            "UNKNOWN"
                        ),

                    "visual_observation":
                        (
                            "Gemini returned a response "
                            "that could not be structured."
                        ),

                    "sensor_interpretation":
                        raw,

                    "recommended_action":
                        (
                            "Continue monitoring the "
                            "cocoa plant and environmental "
                            "conditions."
                        ),

                    "confidence_note":
                        (
                            "Prototype advisory only."
                        ),
                }


            # ---------------------------------------------
            # SAVE MODEL USED
            # ---------------------------------------------

            report[
                "model_used"
            ] = model_name


            print(
                f"[CocoaGuard AI] SUCCESS using {model_name}"
            )


            # ---------------------------------------------
            # RETURN RESULT TO DASHBOARD
            # ---------------------------------------------

            return jsonify({

                "ok": True,

                "report": report,

                "model_used": model_name

            })


        # =================================================
        # MODEL ERROR
        # =================================================

        except Exception as exc:

            last_error = str(
                exc
            )

            print(
                f"[CocoaGuard AI] {model_name} failed."
            )

            print(
                last_error
            )


            # ---------------------------------------------
            # TEMPORARY SERVER ERROR
            # ---------------------------------------------

            if is_temporary_gemini_error(
                last_error
            ):

                # If there is another model,
                # wait briefly and automatically try it.

                if (
                    index
                    <
                    len(
                        FALLBACK_MODELS
                    ) - 1
                ):

                    print(
                        "[CocoaGuard AI] "
                        "Gemini temporarily busy."
                    )

                    print(
                        "[CocoaGuard AI] "
                        "Switching to fallback model..."
                    )

                    time.sleep(
                        1.5
                    )

                    continue


            # ---------------------------------------------
            # NON-TEMPORARY ERROR
            # ---------------------------------------------

            break


    # =====================================================
    # ALL MODELS FAILED
    # =====================================================

    print(
        "[CocoaGuard AI] "
        "All Gemini models failed."
    )

    print(
        last_error
    )


    if is_temporary_gemini_error(last_error):
        public_error = (
            "Gemini is temporarily busy. "
            "Please try the analysis again."
        )
        status_code = 503
    else:
        public_error = (
            "Gemini request failed. Check the API key, "
            "model configuration, and internet connection."
        )
        status_code = 502

    return jsonify({
        "ok": False,
        "error": public_error,
    }), status_code


# =========================================================
# FILE SIZE ERROR
# =========================================================

@app.errorhandler(413)
def upload_too_large(_error):
    return jsonify({
        "ok": False,
        "error": "Image is too large. Maximum upload size is 8 MB.",
    }), 413


# =========================================================
# START COCOAGUARD DASHBOARD
# =========================================================

if __name__ == "__main__":

    # Start Wokwi reader in background

    threading.Thread(

        target=serial_worker,

        daemon=True

    ).start()


    # Start Flask dashboard

    app.run(

        host="127.0.0.1",

        port=5000,

        debug=False

    )