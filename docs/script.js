const MQTT_URL = "wss://broker.hivemq.com:8884/mqtt";
const TOPIC_PREFIX = "home/6C:C8:40:4E:96:10";
const ADMIN_PASSWORD = "1234";
const FULL_LIMIT = 90;

let currentWater = 0;
let pumpState = "OFF";
let tankState = "UNKNOWN";
let lastHeartbeat = 0;
let isAdmin = false;
let sirenAudio = null;
let sirenInterval = null;

const client = mqtt.connect(MQTT_URL, {
  clientId: "github_dashboard_" + Math.random().toString(16).slice(2),
  clean: true,
  reconnectPeriod: 2500,
  connectTimeout: 10000,
});

const chart = new Chart(document.getElementById("waterChart").getContext("2d"), {
  type: "line",
  data: {
    labels: [],
    datasets: [{
      label: "Water Level %",
      data: [],
      borderColor: "#0ea5e9",
      backgroundColor: "rgba(14,165,233,.13)",
      tension: .35,
      fill: true,
    }],
  },
  options: {
    responsive: true,
    maintainAspectRatio: false,
    scales: {
      y: {
        min: 0,
        max: 100,
      },
    },
  },
});

client.on("connect", () => {
  setMessage("Connected to public MQTT broker. Waiting for ESP32 data...");
  client.subscribe(`${TOPIC_PREFIX}/#`);
});

client.on("reconnect", () => {
  setSystem("CONNECTING");
  setMessage("Reconnecting to MQTT broker...");
});

client.on("error", () => {
  setSystem("OFFLINE");
  setMessage("Could not connect to public MQTT broker.");
});

client.on("message", (topic, payload) => {
  const value = payload.toString();
  document.getElementById("deviceId").textContent = TOPIC_PREFIX.replace("home/", "");

  if (!topic.includes("/actions/")) {
    lastHeartbeat = Date.now();
  }

  if (topic.endsWith("/device/status")) {
    setSystem(value.toUpperCase());
    return;
  }

  if (topic.endsWith("/water/level")) {
    updateWater(Number.parseInt(value, 10) || 0);
    return;
  }

  if (topic.endsWith("/tank/status")) {
    tankState = value.toUpperCase();
    document.getElementById("tankStatus").textContent = tankState;
    if (tankState === "FULL") {
      updateWater(FULL_LIMIT);
      setPump("OFF");
      startSiren();
    }
    return;
  }

  if (topic.endsWith("/pump/status")) {
    setPump(value.toUpperCase());
    return;
  }

  if (topic.endsWith("/light/status")) {
    setOnOff("lightStatus", value);
    return;
  }

  if (topic.endsWith("/fan/status")) {
    setOnOff("fanStatus", value);
    return;
  }

  if (topic.endsWith("/ack")) {
    setMessage(value);
  }
});

function updateWater(level) {
  currentWater = Math.max(0, Math.min(100, level));
  document.getElementById("waterLevel").textContent = `${currentWater}%`;
  document.getElementById("tankText").textContent = `${currentWater}%`;
  document.getElementById("waterFill").style.height = `${currentWater}%`;

  chart.data.labels.push(new Date().toLocaleTimeString());
  chart.data.datasets[0].data.push(currentWater);
  if (chart.data.labels.length > 20) {
    chart.data.labels.shift();
    chart.data.datasets[0].data.shift();
  }
  chart.update();

  if (currentWater >= FULL_LIMIT) {
    tankState = "FULL";
    document.getElementById("tankStatus").textContent = "FULL";
    setPump("OFF");
    startSiren();
  }
}

function setSystem(value) {
  const el = document.getElementById("systemStatus");
  el.textContent = value;
  el.className = value === "ONLINE" ? "on" : "off";
  if (value === "ONLINE") {
    setMessage("ESP32 is online. Live data is visible to everyone.");
  }
}

function setPump(value) {
  pumpState = value;
  setOnOff("pumpStatus", value);
}

function setOnOff(id, value) {
  const normalized = value.toUpperCase();
  const el = document.getElementById(id);
  el.textContent = normalized;
  el.className = normalized === "ON" ? "on" : "off";
}

function control(device, state) {
  if (!isAdmin) {
    setMessage("Viewer mode only. Admin login is required for controls.");
    return;
  }

  if (device === "pump" && state === "ON" && (tankState === "FULL" || currentWater >= FULL_LIMIT)) {
    setMessage("Tank is full. Manual pump ON is blocked.");
    client.publish(`${TOPIC_PREFIX}/actions/pump`, "OFF");
    return;
  }

  client.publish(`${TOPIC_PREFIX}/actions/${device}`, state);
  setMessage(`Admin command sent: ${device.toUpperCase()} ${state}`);
}

function emptyTank() {
  if (!isAdmin) {
    setMessage("Admin login is required to empty the tank.");
    return;
  }
  stopSiren();
  client.publish(`${TOPIC_PREFIX}/actions/tank`, "EMPTY");
  updateWater(0);
  tankState = "EMPTY";
  document.getElementById("tankStatus").textContent = "EMPTY";
  setMessage("Admin command sent: EMPTY TANK");
}

function loginAdmin() {
  const entered = document.getElementById("adminPassword").value;
  if (entered !== ADMIN_PASSWORD) {
    setMessage("Wrong admin password.");
    return;
  }

  isAdmin = true;
  document.body.classList.add("admin");
  document.getElementById("adminPassword").hidden = true;
  document.getElementById("adminLoginBtn").hidden = true;
  document.getElementById("adminLogoutBtn").hidden = false;
  setMessage("Admin mode enabled. Controls are unlocked on this browser.");
}

function logoutAdmin() {
  isAdmin = false;
  document.body.classList.remove("admin");
  document.getElementById("adminPassword").hidden = false;
  document.getElementById("adminLoginBtn").hidden = false;
  document.getElementById("adminLogoutBtn").hidden = true;
  document.getElementById("adminPassword").value = "";
  setMessage("Viewer mode enabled.");
}

function startSiren() {
  document.getElementById("sirenAlert").classList.add("active");
  if (!sirenAudio) {
    sirenAudio = new (window.AudioContext || window.webkitAudioContext)();
  }
  if (sirenInterval) {
    return;
  }

  sirenInterval = setInterval(() => {
    const oscillator = sirenAudio.createOscillator();
    const gain = sirenAudio.createGain();
    oscillator.type = "sawtooth";
    oscillator.frequency.setValueAtTime(740, sirenAudio.currentTime);
    oscillator.frequency.exponentialRampToValueAtTime(1180, sirenAudio.currentTime + .22);
    gain.gain.setValueAtTime(.001, sirenAudio.currentTime);
    gain.gain.exponentialRampToValueAtTime(.12, sirenAudio.currentTime + .03);
    gain.gain.exponentialRampToValueAtTime(.001, sirenAudio.currentTime + .36);
    oscillator.connect(gain);
    gain.connect(sirenAudio.destination);
    oscillator.start();
    oscillator.stop(sirenAudio.currentTime + .38);
  }, 430);
}

function stopSiren() {
  document.getElementById("sirenAlert").classList.remove("active");
  if (sirenInterval) {
    clearInterval(sirenInterval);
    sirenInterval = null;
  }
}

function setMessage(message) {
  document.getElementById("message").textContent = message;
}

setInterval(() => {
  if (lastHeartbeat && Date.now() - lastHeartbeat > 15000) {
    setSystem("OFFLINE");
    setPump("OFF");
    setMessage("ESP32 is offline. Waiting for reconnect...");
  }
}, 5000);
