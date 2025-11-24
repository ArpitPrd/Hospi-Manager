#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define BUTTON_PIN 4
#define LED_PIN 15   // <--- LED will turn ON for 20 seconds when MED_ALARM_ON is received

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic;
bool deviceConnected = false;
bool lastButtonState = HIGH;

// Timer variables for 20-second LED control
unsigned long ledTimer = 0;
bool ledActive = false;

#define SERVICE_UUID        "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    Serial.println("Device connected");
  }

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Serial.println("Device disconnected");
    digitalWrite(LED_PIN, LOW); // Always OFF on disconnect
    BLEDevice::startAdvertising();
  }
};

class MyCharacteristicCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    
    std::string value = pCharacteristic->getValue().c_str();
    Serial.print("Received value: ");
    Serial.println(value.c_str());

    // ====== NEW LOGIC ======
    if (value == "MED_ALARM_ON") {
      Serial.println("MEDICAL ALARM: Turning LED ON for 20 seconds!");
      digitalWrite(LED_PIN, HIGH);

      ledTimer = millis();   // Start timer
      ledActive = true;

    } else if (value == "MED_ALARM_OFF") {
      Serial.println("Turning LED OFF");
      digitalWrite(LED_PIN, LOW);
      ledActive = false;
    }
  }
};

void setup() {
  Serial.begin(115200);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);

  digitalWrite(LED_PIN, LOW);   // LED MUST be OFF at start

  BLEDevice::init("ESP32_Button");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_WRITE
                    );

  pCharacteristic->addDescriptor(new BLE2902());
  pCharacteristic->setCallbacks(new MyCharacteristicCallbacks());

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  BLEDevice::startAdvertising();

  Serial.println("BLE device ready. Waiting for connection...");
}

void loop() {
  // -------- BUTTON NOTIFICATION --------
  bool buttonState = digitalRead(BUTTON_PIN);

  if (deviceConnected && buttonState == LOW && lastButtonState == HIGH) {
    Serial.println("Button pressed, notifying...");
    pCharacteristic->setValue("BUTTON_PRESSED");
    pCharacteristic->notify();
    delay(500);
  }

  lastButtonState = buttonState;

  // -------- 20-SECOND LED TIMER --------
  if (ledActive && millis() - ledTimer >= 20000) {   // 20 seconds
    Serial.println("20 seconds completed → Turning LED OFF");
    digitalWrite(LED_PIN, LOW);
    ledActive = false;
  }
}

IN this i want to add the new feature that when the fall detection happen from the MPU6050 it will send the fall detection notification to the application

Here is the app code:
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Nurse Assistant System</title>
  <style>
    body {
      font-family: 'Segoe UI', sans-serif;
      background: linear-gradient(135deg, #203040, #2c3e50);
      color: white;
      text-align: center;
      min-height: 100vh;
      display: flex;
      flex-direction: column;
      justify-content: center;
      align-items: center;
      padding: 20px 0;
    }

    h1 {
      font-size: 2.2em;
      margin-bottom: 20px;
    }

    button, .btn {
      background: #00bcd4;
      color: white;
      border: none;
      padding: 15px 30px;
      border-radius: 10px;
      cursor: pointer;
      font-size: 18px;
      margin: 10px;
      transition: 0.2s;
      text-decoration: none;
      display: inline-block;
    }

    button:hover, .btn:hover {
      background: #0097a7;
    }

    /* Shared styling for status boxes */
    .status-box {
      margin-top: 20px;
      font-size: 20px;
      font-weight: bold;
      padding: 15px;
      border-radius: 10px;
      transition: 0.3s;
      width: 80%;
      max-width: 400px;
    }

    .alert {
      background-color: #e53935;
      color: white;
      box-shadow: 0 0 20px rgba(229, 57, 53, 0.6);
    }

    .serviced {
      background-color: #43a047;
      color: white;
      box-shadow: 0 0 20px rgba(67, 160, 71, 0.6);
    }

    .waiting {
      background-color: #546e7a;
      color: white;
    }

    .med-alert-active {
      background-color: #fdd835; /* Yellow */
      color: #212121; /* Dark text for readability */
      box-shadow: 0 0 20px rgba(253, 216, 53, 0.7);
    }

    /* *** NEW *** Modal Styles */
    #scheduleModal {
      display: none; /* Hidden by default */
      position: fixed;
      z-index: 100;
      left: 0;
      top: 0;
      width: 100%;
      height: 100%;
      overflow: auto;
      background-color: rgba(0, 0, 0, 0.6);
      justify-content: center;
      align-items: center;
    }

    .modal-content {
      background-color: #3e5770;
      color: white;
      margin: auto;
      padding: 25px;
      border: 1px solid #888;
      width: 90%;
      max-width: 500px;
      border-radius: 15px;
      box-shadow: 0 5px 25px rgba(0,0,0,0.4);
    }

    .modal-content h2 {
      margin-top: 0;
    }

    .modal-content input[type="text"],
    .modal-content input[type="time"] {
      padding: 10px;
      font-size: 16px;
      margin: 5px;
      border-radius: 5px;
      border: none;
      width: 180px;
    }

    .modal-content .btn-add {
      background: #43a047;
      padding: 10px 20px;
      font-size: 16px;
    }
    .modal-content .btn-add:hover { background: #388e3c; }

    .modal-content .btn-download {
      background: #f57c00;
    }
    .modal-content .btn-download:hover { background: #e65100; }

    .modal-content .btn-close {
      background: #e53935;
    }
    .modal-content .btn-close:hover { background: #c62828; }

    #scheduleList {
      list-style-type: none;
      padding: 0;
      max-height: 200px;
      overflow-y: auto;
      background: rgba(0,0,0,0.2);
      border-radius: 5px;
      margin-top: 15px;
    }
    #scheduleList li {
      padding: 10px;
      border-bottom: 1px solid #546e7a;
      display: flex;
      justify-content: space-between;
      align-items: center;
      font-size: 18px;
    }
    #scheduleList li:last-child { border-bottom: none; }

    #scheduleList .delete-btn {
      background: #e53935;
      color: white;
      border: none;
      border-radius: 50%;
      cursor: pointer;
      font-weight: bold;
      width: 28px;
      height: 28px;
      font-size: 16px;
    }

  </style>
</head>
<body>

  <h1>🏥 Nurse Assistant System</h1>

  <button id="connectBtn">🔗 Connect to ESP32</button>
  <button id="serviceBtn" disabled>✅ Patient Serviced</button>
  <button id="editScheduleBtn" class="btn">📅 Edit Schedule</button>

  <div id="med-alert" class="status-box waiting">
    💊 No medicine reminders.
  </div>

  <p id="status" class="status-box waiting">🕓 Waiting for patient call...</p>

  <audio id="alertSound" src="https://actions.google.com/sounds/v1/alarms/beep_short.ogg" preload="auto"></audio>

  <div id="scheduleModal">
    <div class="modal-content">
      <h2>Edit Medicine Schedule</h2>

      <ul id="scheduleList">
        </ul>

      <div>
        <input type="text" id="medNameInput" placeholder="Medicine Name">
        <input type="time" id="medTimeInput">
        <button id="addMedBtn" class="btn btn-add">Add</button>
      </div>

      <hr>

      <button id="downloadBtn" class="btn btn-download">💾 Download as CSV</button>
      <button id="closeModalBtn" class="btn btn-close">Close</button>
    </div>
  </div>

  <script>
    const SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
    const CHARACTERISTIC_UUID = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";
    const SCHEDULE_KEY = "nurseAppSchedule"; // localStorage key

    let bleCharacteristic = null;
    let medicineSchedule = []; // To store the schedule
    let medCheckInterval = null;
    let lastAlertedMedTime = null;

    // --- Sound ---
    function playAlertSound() {
      const sound = document.getElementById("alertSound");
      sound.currentTime = 0;
      sound.play().catch(err => console.warn("Audio blocked:", err));
    }

    // --- BLE Functions ---
    async function connectBLE() {
      try {
        const statusEl = document.getElementById("status");
        statusEl.className = "status-box waiting";
        statusEl.innerText = "🔍 Scanning for ESP32...";

        const device = await navigator.bluetooth.requestDevice({
          filters: [{ namePrefix: "ESP32" }],
          optionalServices: [SERVICE_UUID],
        });

        statusEl.innerText = "🔗 Connecting...";
        const server = await device.gatt.connect();
        const service = await server.getPrimaryService(SERVICE_UUID);
        bleCharacteristic = await service.getCharacteristic(CHARACTERISTIC_UUID);

        await bleCharacteristic.startNotifications();
        bleCharacteristic.addEventListener("characteristicvaluechanged", handleNotification);

        statusEl.innerText = "✅ Connected! Waiting for patient call...";
        document.getElementById("connectBtn").disabled = true;

        // Start the medicine checker
        startMedicationChecker();

      } catch (error) {
        console.error(error);
        document.getElementById("status").innerText = "❌ Connection failed: " + error;
      }
    }

    function handleNotification(event) {
      const value = new TextDecoder().decode(event.target.value);
      console.log("Received:", value);

      if (value.includes("BUTTON_PRESSED")) {
        const statusEl = document.getElementById("status");
        statusEl.className = "status-box alert";
        statusEl.innerText = "🚨 Patient Calling! Immediate attention required.";
        playAlertSound();
        document.getElementById("serviceBtn").disabled = false;
      }

      if (value.includes("FALL_DETECTED")) {
        const fallEl = document.getElementById("fall-alert");
        fallEl.className = "status-box alert";
        fallEl.innerText = "🚨 FALL DETECTED! Immediate attention required.";
        playAlertSound();

        // Enable serviced button
        document.getElementById("serviceBtn").disabled = false;
    }

    }

    async function sendBLECommand(command) {
      if (!bleCharacteristic) {
        console.warn("BLE not connected. Cannot send command.");
        return;
      }
      try {
        const encoder = new TextEncoder();
        await bleCharacteristic.writeValue(encoder.encode(command));
        console.log("Sent command:", command);
      } catch (err) {
        console.error("Failed to send command", err);
      }
    }

    // --- Serviced Button ---
    async function markServiced() {
      const statusEl = document.getElementById("status");
      statusEl.className = "status-box serviced";
      statusEl.innerText = "✅ Patient Serviced. System ready.";
      document.getElementById("serviceBtn").disabled = true;

      const medEl = document.getElementById("med-alert");
      medEl.className = "status-box waiting";
      medEl.innerText = "💊 No medicine reminders.";

      await sendBLECommand("MED_ALARM_OFF");

      // ---- RESET FALL ALERT ----
        const fallEl = document.getElementById("fall-alert");
        fallEl.className = "status-box waiting";
        fallEl.innerText = "🧍‍♂️ No fall detected.";
        sendBLECommand("FALL_RESET");
    }

    // --- *** MODIFIED / NEW *** Medicine Schedule Logic ---

    // 1. Load schedule from localStorage (if it exists) or from CSV (as fallback)
    async function startMedicationChecker() {
      const medEl = document.getElementById("med-alert");
      const savedSchedule = localStorage.getItem(SCHEDULE_KEY);

      if (savedSchedule) {
        // Found schedule in localStorage
        medicineSchedule = JSON.parse(savedSchedule);
        console.log("Loaded schedule from localStorage:", medicineSchedule);
        medEl.innerText = "💊 Schedule loaded. Monitoring...";
      } else {
        // Not found, try to fetch the default CSV
        try {
          const response = await fetch('schedule.csv');
          if (!response.ok) throw new Error('schedule.csv not found');

          const data = await response.text();
          medicineSchedule = data.split('\n')
            .filter(line => line.trim() !== '')
            .map(line => {
              const [name, time] = line.split(',');
              return { name: (name || "").trim(), time: (time || "").trim() };
            })
            .filter(med => med.name && med.time); // Ensure no empty entries

          console.log("Loaded default schedule from schedule.csv:", medicineSchedule);
          saveSchedule(); // Save the default schedule to localStorage
          medEl.innerText = "💊 Default schedule loaded. Monitoring...";
        } catch (err) {
          console.error("Could not load schedule.csv", err);
          medEl.innerText = "❌ Could not load schedule.csv";
          return;
        }
      }

      // 2. Start checking the time
      if (medCheckInterval) clearInterval(medCheckInterval);
      medCheckInterval = setInterval(checkSchedule, 10000); // Check every 10 seconds
    }

    // 2. Main time-checking logic (unchanged)
    function checkSchedule() {
      const now = new Date();
      const currentTime = now.toTimeString().substring(0, 5); // "HH:MM"

      for (const med of medicineSchedule) {
        if (med.time === currentTime) {
          if (lastAlertedMedTime !== currentTime) {
            console.log("MEDICATION ALERT:", med.name);
            triggerMedicationAlert(med.name);
            lastAlertedMedTime = currentTime;
          }
          break;
        }
      }
    }

    // 3. Trigger alert (unchanged)
    function triggerMedicationAlert(medName) {
      const medEl = document.getElementById("med-alert");
      medEl.className = "status-box med-alert-active";
      medEl.innerText = `💊 Time for: ${medName}`;
      playAlertSound();
      document.getElementById("serviceBtn").disabled = false;
      sendBLECommand("MED_ALARM_ON");
    }

    // 4. *** NEW *** Save the current schedule to localStorage
    function saveSchedule() {
      localStorage.setItem(SCHEDULE_KEY, JSON.stringify(medicineSchedule));
      console.log("Schedule saved to localStorage");
    }

    // 5. *** NEW *** Render the schedule list in the modal
    function renderScheduleList() {
      const listEl = document.getElementById("scheduleList");
      listEl.innerHTML = ""; // Clear the list

      if(medicineSchedule.length === 0) {
        listEl.innerHTML = "<li>No medicines in schedule.</li>";
        return;
      }

      medicineSchedule.forEach((med, index) => {
        const li = document.createElement("li");
        li.innerHTML = `
          <span>${med.name} at ${med.time}</span>
          <button class="delete-btn" data-index="${index}">&times;</button>
        `;
        listEl.appendChild(li);
      });

      // Add event listeners to the new delete buttons
      listEl.querySelectorAll('.delete-btn').forEach(btn => {
        btn.addEventListener('click', deleteMedicine);
      });
    }

    // 6. *** NEW *** Add a new medicine
    function addMedicine() {
      const nameInput = document.getElementById("medNameInput");
      const timeInput = document.getElementById("medTimeInput");

      const name = nameInput.value.trim();
      const time = timeInput.value;

      if (name && time) {
        medicineSchedule.push({ name, time });
        medicineSchedule.sort((a, b) => a.time.localeCompare(b.time)); // Keep sorted by time
        saveSchedule(); // Save to localStorage
        renderScheduleList(); // Redraw the list in the modal

        // Clear inputs
        nameInput.value = "";
        timeInput.value = "";
      } else {
        alert("Please enter both a name and a time.");
      }
    }

    // 7. *** NEW *** Delete a medicine
    function deleteMedicine(event) {
      const index = event.target.dataset.index;
      if (confirm(`Are you sure you want to delete ${medicineSchedule[index].name}?`)) {
        medicineSchedule.splice(index, 1); // Remove from array
        saveSchedule(); // Save to localStorage
        renderScheduleList(); // Redraw list
      }
    }

    // 8. *** NEW *** Download schedule as a new CSV file
    function downloadScheduleCSV() {
      let csvContent = "data:text/csv;charset=utf-8,";
      csvContent += medicineSchedule
        .map(med => `${med.name},${med.time}`)
        .join("\n");

      const encodedUri = encodeURI(csvContent);
      const link = document.createElement("a");
      link.setAttribute("href", encodedUri);
      link.setAttribute("download", "my_schedule.csv");
      document.body.appendChild(link);
      link.click();
      document.body.removeChild(link);
    }

    // 9. *** NEW *** Show/Hide Modal
    const modal = document.getElementById("scheduleModal");
    function openScheduleModal() {
      renderScheduleList();
      modal.style.display = "flex";
    }
    function closeScheduleModal() {
      modal.style.display = "none";
    }

    // --- Event Listeners ---
    document.getElementById("connectBtn").addEventListener("click", connectBLE);
    document.getElementById("serviceBtn").addEventListener("click", markServiced);

    // *** NEW *** Modal event listeners
    document.getElementById("editScheduleBtn").addEventListener("click", openScheduleModal);
    document.getElementById("closeModalBtn").addEventListener("click", closeScheduleModal);
    document.getElementById("addMedBtn").addEventListener("click", addMedicine);
    document.getElementById("downloadBtn").addEventListener("click", downloadScheduleCSV);

  </script>


<div id="fall-alert" class="status-box waiting">
  🧍‍♂️ No fall detected.
</div>

</body>
</html>