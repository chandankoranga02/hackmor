const express = require("express");
const cors = require("cors");

const app = express();
const PORT = 5000;

app.use(cors());
app.use(express.json());

/* =========================
   In-Memory Storage
========================= */

let sensorData = {
  temperature: 0,
  humidity: 0,
  moisture: 0,
  ph: 5.5,
  weather: 50,
  lastUpdated: null
};

let pumpState = "OFF";      // ON / OFF
let pumpMode = "AUTO";      // AUTO / MANUAL
let safetyActive = false;

/* =========================
   Utility Logger
========================= */

function log(title, data) {
  console.log(`\n=== ${title} ===`);
  console.log(data);
  console.log("=====================\n");
}

/* =========================
   Health Check
========================= */

app.get("/", (req, res) => {
  res.json({ status: "Backend running 🚀" });
});

/* =========================
   SENSOR ROUTES
========================= */

app.get("/api/sensors", (req, res) => {
  res.json({
    ...sensorData,
    pumpState,
    pumpMode,
    safetyActive
  });
});

app.post("/api/esp32", (req, res) => {

  const { temperature, humidity, moisture } = req.body;

  if (
    typeof temperature !== "number" ||
    typeof humidity !== "number" ||
    typeof moisture !== "number"
  ) {
    return res.status(400).json({ error: "Invalid sensor data format" });
  }

  sensorData.temperature = temperature;
  sensorData.humidity = humidity;
  sensorData.moisture = moisture;
  sensorData.lastUpdated = new Date();

  // AUTO SAFETY EXAMPLE (Optional)
  if (moisture > 95) {
    safetyActive = true;
    pumpState = "OFF";
  }

  log("ESP32 Data Updated", sensorData);

  res.json({ message: "ESP32 data updated" });
});

/* =========================
   MANUAL DATA UPDATE
========================= */

app.post("/api/manual", (req, res) => {

  const { ph, weather } = req.body;

  if (typeof ph === "number") sensorData.ph = ph;
  if (typeof weather === "number") sensorData.weather = weather;

  log("Manual Update", sensorData);

  res.json({ message: "Manual data updated" });
});

/* =========================
   PUMP CONTROL
========================= */

app.post("/api/pump", (req, res) => {

  const { state, mode } = req.body;

  if (mode === "AUTO" || mode === "MANUAL") {
    pumpMode = mode;
  }

  if (state === "ON" || state === "OFF") {
    if (!safetyActive) {
      pumpState = state;
    }
  }

  log("Pump Updated", { pumpState, pumpMode });

  res.json({
    success: true,
    state: pumpState,
    mode: pumpMode,
    safetyActive
  });
});

app.get("/api/pump", (req, res) => {

  if (safetyActive) {
    return res.json({
      state: "OFF",
      mode: pumpMode,
      safetyActive: true
    });
  }

  res.json({
    state: pumpState,
    mode: pumpMode,
    safetyActive
  });
});

/* =========================
   SAFETY CONTROL
========================= */

app.post("/api/safety", (req, res) => {

  const { active } = req.body;

  if (typeof active !== "boolean") {
    return res.status(400).json({ error: "Invalid safety value" });
  }

  safetyActive = active;

  if (safetyActive) {
    pumpState = "OFF";
  }

  log("Safety Status", safetyActive);

  res.json({
    success: true,
    safetyActive,
    pumpState
  });
});

/* =========================
   SERVER START
========================= */

app.listen(PORT, "0.0.0.0", () => {
  console.log(`🚀 Server running on port ${PORT}`);
});
