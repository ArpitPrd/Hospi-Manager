import asyncio
import streamlit as st
from bleak import BleakClient, BleakScanner
from datetime import datetime

# BLE UUIDs (must match ESP32)
SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
CHARACTERISTIC_UUID = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

st.set_page_config(page_title="ESP32 BLE Button Monitor", layout="centered")

st.title("🔵 ESP32 BLE Button Monitor")
status_placeholder = st.empty()
log_placeholder = st.empty()

# Global variable to store current button state
button_state = st.session_state.get("button_state", "OFF")

async def connect_and_listen():
    """Scan for ESP32 device and listen for button notifications."""
    devices = await BleakScanner.discover(timeout=5)
    esp32_device = None
    for d in devices:
        if "ESP32_Button" in d.name:
            esp32_device = d
            break

    if not esp32_device:
        status_placeholder.error("❌ ESP32_Button not found. Make sure it’s powered and advertising.")
        return

    status_placeholder.info(f"🔗 Connecting to {esp32_device.name}...")
    async with BleakClient(esp32_device) as client:
        if not client.is_connected:
            status_placeholder.error("⚠️ Connection failed.")
            return

        status_placeholder.success("✅ Connected to ESP32_Button")

        def notification_handler(sender, data):
            """Handle BLE notifications."""
            message = data.decode("utf-8").strip()
            global button_state
            if "BUTTON_PRESSED" in message:
                button_state = "ON" if button_state == "OFF" else "OFF"
                st.session_state["button_state"] = button_state

        await client.start_notify(CHARACTERISTIC_UUID, notification_handler)

        # Keep listening until Streamlit stops
        while True:
            status_placeholder.markdown(f"### 🟢 Button State: **{st.session_state['button_state']}**")
            log_placeholder.text(f"Last update: {datetime.now().strftime('%H:%M:%S')}")
            await asyncio.sleep(0.5)

async def main():
    await connect_and_listen()

if st.button("🔍 Start Listening"):
    st.info("Starting BLE listener... Please wait.")
    asyncio.run(main())
