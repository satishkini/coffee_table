#ifndef CT_INDEX_H
#define CT_INDEX_H

const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width,initial-scale=1.0">
  <title>Z-Scale Throttle</title>
  <style>
    body { font-family: Arial, sans-serif; text-align: center; background-color: #111; color: #eee; margin: 0; padding: 20px; }
    h2 { color: #ffcc00; margin-bottom: 10px; }
    .container { max-width: 400px; margin: 0 auto; background: #222; padding: 20px; border-radius: 15px; box-shadow: 0 4px #000; margin-bottom: 15px; }
    .label { font-size: 18px; margin: 15px 0; color: #aaa; }
    .value { font-size: 32px; font-weight: bold; color: #ffcc00; }
    .slider { -webkit-appearance: none; width: 100%; height: 25px; background: #444; outline: none; border-radius: 12px; margin: 20px 0; }
    .slider::-webkit-slider-thumb { -webkit-appearance: none; appearance: none; width: 35px; height: 35px; background: #ffcc00; border-radius: 50%; cursor: pointer; }
    .btn { background-color: #333; border: 2px solid #555; color: white; padding: 15px; font-size: 18px; font-weight: bold; margin: 10px 5px; cursor: pointer; border-radius: 10px; width: 45%; text-decoration: none; display: inline-block; }
    .btn.active { background-color: #ffcc00; color: #000; border-color: #ffcc00; }
    .action-btn { background-color: #00adb5; border: none; width: 45%; margin: 5px; }
    .action-btn:active { background-color: #008c9e; }
    .led-btn { background-color: #4a4e69; border: none; width: 45%; margin: 5px; }
    .led-btn.active { background-color: #9a8c98; color: #fff; border: 1px solid #ffcc00; }
    .stop-btn { background-color: #cc0000; border-color: #aa0000; width: 95%; font-size: 22px; padding: 20px; margin-top: 15px; }
    .default-btn { background-color: #2c2c2c; border: 1px solid #444; color: #00adb5; font-size: 12px; font-weight: bold; padding: 6px 14px; border-radius: 6px; cursor: pointer; transition: 0.2s; margin-top: -5px; margin-bottom: 15px; display: inline-block; }
    .default-btn:active { background-color: #00adb5; color: #fff; border-color: #00adb5; }
    .settings-toggle { background-color: #444; border: 1px solid #666; color: #ffcc00; padding: 12px; font-size: 16px; font-weight: bold; width: 100%; max-width: 440px; border-radius: 10px; cursor: pointer; margin: 15px auto 5px auto; display: block; }
    .config-panel { max-width: 400px; margin: 0 auto; background: #2c2c2c; padding: 0 15px; border-radius: 12px; border: 1px solid #444; text-align: left; max-height: 0; overflow: hidden; transition: max-height 0.3s ease-out, padding 0.3s ease-out; }
    .config-panel.expanded { max-height: 600px; padding: 15px; }
    .config-title { font-size: 16px; color: #ffcc00; font-weight: bold; margin-bottom: 15px; text-align: center; }
    .input-group { margin-bottom: 12px; display: flex; justify-content: space-between; align-items: center; }
    .input-group label { font-size: 14px; color: #bbb; }
    .input-group input { width: 90px; padding: 6px; background: #111; border: 1px solid #555; color: #fff; border-radius: 6px; text-align: center; font-size: 14px; }
    .update-btn { background-color: #00adb5; border: none; color: white; padding: 10px; font-size: 14px; font-weight: bold; width: 100%; border-radius: 8px; cursor: pointer; margin-top: 5px; }
    .toggle-switch { position: relative; display: inline-block; width: 50px; height: 26px; }
    .toggle-switch input { opacity: 0; width: 0; height: 0; }
    .slider-round { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #444; transition: .3s; border-radius: 34px; border: 1px solid #555; }
    .slider-round:before { position: absolute; content: ""; height: 18px; width: 18px; left: 3px; bottom: 3px; background-color: white; transition: .3s; border-radius: 50%; }
    input:checked + .slider-round { background-color: #ffcc00; border-color: #ffcc00; }
    input:checked + .slider-round:before { transform: translateX(24px); background-color: #111; }
    .telemetry-box { display: flex; justify-content: space-between; background: #1a1a1a; border: 1px solid #333; padding: 10px 15px; border-radius: 10px; margin-bottom: 15px; font-size: 13px; color: #aaa; }
    .telemetry-item span { color: #ffcc00; font-weight: bold; }
  </style>
</head>
<body>
  <h2>Coffee table Controller</h2>
  <div class="container">
    <div class="telemetry-box">
      <div class="telemetry-item">State: <span id="telemetryState">---</span></div>
      <div class="telemetry-item">Dir: <span id="telemetryDir">---</span></div>
      <div class="telemetry-item">Live: <span id="telemetrySpeed">0</span>%</div>
    </div>
    <div class="label">Target Speed: <span id="speedVal" class="value">0</span>%</div>
    <input type="range" min="0" max="100" value="0" class="slider" id="throttle" 
           oninput="updateSpeedValue(this.value)"
           onmousedown="startInteraction()" 
           onmouseup="endInteraction()"
           ontouchstart="startInteraction()" 
           ontouchend="endInteraction()">
    <div>
      <button class="default-btn" onclick="saveAsDefaultSpeed()">SET AS DEFAULT</button>
    </div>
    <div>
      <button class="btn action-btn" onclick="startTrain()">START</button>
      <button class="btn action-btn" onclick="stopTrainSmoothly()">STOP</button>
    </div>
    <div id="manualLedContainer">
      <button id="ledOnBtn" class="btn led-btn" onclick="toggleLed(1)">LIGHT ON</button>
      <button id="ledOffBtn" class="btn led-btn active" onclick="toggleLed(0)">LIGHT OFF</button>
    </div>
    <div>
      <button id="fwdBtn" class="btn active" onclick="setDirection('forward')">FORWARD</button>
      <button id="revBtn" class="btn" onclick="setDirection('reverse')">REVERSE</button>
    </div>
    <button class="btn stop-btn" onclick="emergencyStop()">EMERGENCY STOP</button>
  </div>
  
  <!-- RESTORED: Settings panel toggle button wrapper position fixed cleanly -->
  <button class="settings-toggle" onclick="toggleSettingsMenu()">SETTINGS</button>
  
  <div class="config-panel" id="settingsMenu">
    <div class="config-title">Momentum and Automation Fine-Tuning</div>
    <div class="input-group">
      <label for="stepInput">Ramp Step Size (1-50):</label>
      <input type="number" id="stepInput" min="1" max="50" value="2">
    </div>
    <div class="input-group">
      <label for="intervalInput">Ramp Interval (10-500 ms):</label>
      <input type="number" id="intervalInput" min="10" max="500" value="15">
    </div>
    <div class="input-group">
      <label for="waitInput">Station Wait (1000-30000 ms):</label>
      <input type="number" id="waitInput" min="1000" max="30000" value="4000">
    </div>
    <div class="input-group">
      <label for="cooldownInput">IR Cooldown (1000-30000 ms):</label>
      <input type="number" id="cooldownInput" min="1000" max="30000" value="5000">
    </div>
    <div class="input-group">
      <label for="clampInput">Min Speed Clamp (0-100):</label>
      <input type="number" id="clampInput" min="0" max="100" value="14">
    </div>
    <div class="input-group">
      <label for="debugToggle">Enable Debug Telemetry:</label>
      <label class="toggle-switch">
        <input type="checkbox" id="debugToggle" onclick="toggleDebugMode(this.checked)">
        <span class="slider-round"></span>
      </label>
    </div>
    <button class="update-btn" onclick="updatePhysicsSettings()">UPDATE CONFIGURATIONS</button>
  </div>

  <script>
    let isUserInteracting = false;
    let interactionTimeout = null;
    let isInitialLoad = true; // NEW INTERLOCK: Track the very first network data sync pass

    function startInteraction() {
      isUserInteracting = true;
      if (interactionTimeout) clearTimeout(interactionTimeout);
    }

    function endInteraction() {
      interactionTimeout = setTimeout(() => {
        isUserInteracting = false;
      }, 1500);
    }

    function fetchStatusUpdate() {
      fetch('/status')
        .then(response => response.json())
        .then(data => {
          document.getElementById("telemetryState").innerText = data.state;
          document.getElementById("telemetryDir").innerText = data.dir.toUpperCase();
          document.getElementById("telemetrySpeed").innerText = data.current;

          if (data.state === "EMERGENCY STOP") {
            document.getElementById("telemetryState").style.color = "#cc0000";
          } else if (data.state === "STOPPED") {
            document.getElementById("telemetryState").style.color = "#888888";
          } else {
            document.getElementById("telemetryState").style.color = "#ffcc00";
          }

          // NEW INITIALIZATION GATE: Runs exactly once when you refresh or open the page!
          if (isInitialLoad) {
            isInitialLoad = false;
            // Force the physical thumb handle slider position to match the hardware's active value
            document.getElementById("throttle").value = data.target; 
            // Force the big target speed text reading to update on screen layout
            document.getElementById("speedVal").innerText = data.target; 
          }

          if (isUserInteracting) return; 

          if (data.ledState === 1) {
            document.getElementById("ledOnBtn").classList.add("active");
            document.getElementById("ledOffBtn").classList.remove("active");
          } else {
            document.getElementById("ledOffBtn").classList.add("active");
            document.getElementById("ledOnBtn").classList.remove("active");
          }
          
          if (data.dir === "forward") {
            document.getElementById("fwdBtn").classList.add("active");
            document.getElementById("revBtn").classList.remove("active");
          } else {
            document.getElementById("revBtn").classList.add("active");
            document.getElementById("fwdBtn").classList.remove("active");
          }

          document.getElementById("stepInput").value = data.step;
          document.getElementById("intervalInput").value = data.interval;
          document.getElementById("waitInput").value = data.wait;
          document.getElementById("cooldownInput").value = data.cooldown;
          document.getElementById("clampInput").value = data.clamp;
          document.getElementById("debugToggle").checked = (data.debug === 1);
        }).catch(err => console.error("Telemetry update loop dropped:", err));
    }

    window.addEventListener("DOMContentLoaded", () => {
      fetchStatusUpdate();
      setInterval(fetchStatusUpdate, 1000); 
    });

    function updateSpeedValue(val) {
      document.getElementById("speedVal").innerText = val;
      fetch('/saveselectpercent?val=' + val);
    }
    
    function saveAsDefaultSpeed() {
      fetch('/savedefaultspeed')
        .then(response => { if(response.ok) alert("Current target speed saved as system boot default!"); })
        .catch(err => console.error("Failed to commit default speed:", err));
    }

    function startTrain() { fetch('/start'); }
    
    function toggleLed(state) {
      if (state === 1) {
        document.getElementById("ledOnBtn").classList.add("active");
        document.getElementById("ledOffBtn").classList.remove("active");
      } else {
        document.getElementById("ledOffBtn").classList.add("active");
        document.getElementById("ledOnBtn").classList.remove("active");
      }
      fetch('/setled?state=' + state);
    }
    
    function toggleDebugMode(checked) {
      const state = checked ? 1 : 0;
      fetch('/setdebug?state=' + state).catch(err => console.error("Debug toggle failed:", err));
    }
    
    function stopTrainSmoothly() { fetch('/smoothstop'); }
    
    function setDirection(dir) {
      if (dir === 'forward') {
        document.getElementById("fwdBtn").classList.add("active");
        document.getElementById("revBtn").classList.remove("active");
      } else {
        document.getElementById("revBtn").classList.add("active");
        document.getElementById("fwdBtn").classList.remove("active");
      }
      fetch('/setdir?dir=' + dir);
    }
    
    function emergencyStop() {
      document.getElementById("throttle").value = 0;
      document.getElementById("speedVal").innerText = 0;
      fetch('/stop');
    }
    
    function toggleSettingsMenu() {
      document.getElementById("settingsMenu").classList.toggle("expanded");
    }
    
    function updatePhysicsSettings() {
      const step = document.getElementById("stepInput").value;
      const interval = document.getElementById("intervalInput").value;
      const wait = document.getElementById("waitInput").value;
      const cooldown = document.getElementById("cooldownInput").value;
      const clamp = document.getElementById("clampInput").value;
      fetch(`/updatephysics?step=${step}&interval=${interval}&wait=${wait}&cooldown=${cooldown}&clamp=${clamp}`)
        .then(response => { if(response.ok) alert("Configurations successfully updated and saved!"); });
    }
  </script>
</body>
</html>
)rawliteral";

#endif
