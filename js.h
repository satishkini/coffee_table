#ifndef JS_H
#define JS_H

const char HTML_JS_ENGINE[] PROGMEM = R"rawliteral(
  <script>
    window.addEventListener("DOMContentLoaded", () => {
      fetch('/status')
        .then(response => response.json())
        .then(data => {
          const isNetworkMode = (data.ledMode === 1);
          document.getElementById("ledModeToggle").checked = isNetworkMode;
          
          if(isNetworkMode) {
            document.getElementById("ledOnBtn").classList.add("disabled");
            document.getElementById("ledOffBtn").classList.add("disabled");
          }
          
          if(data.ledState === 1) {
            document.getElementById("ledOnBtn").classList.add("active");
            document.getElementById("ledOffBtn").classList.remove("active");
          } else {
            document.getElementById("ledOffBtn").classList.add("active");
            document.getElementById("ledOnBtn").classList.remove("active");
          }
          
          if(data.dir === "forward") {
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
        }).catch(err => console.error("Handshake initialization sync failed:", err));
    });

    function updateSpeedValue(val) {
      document.getElementById("speedVal").innerText = val;
      fetch('/saveselectpercent?val=' + val);
    }
    function startTrain() { fetch('/start'); }
    function toggleLed(state) {
      if(state === 1) {
        document.getElementById("ledOnBtn").classList.add("active");
        document.getElementById("ledOffBtn").classList.remove("active");
      } else {
        document.getElementById("ledOffBtn").classList.add("active");
        document.getElementById("ledOnBtn").classList.remove("active");
      }
      fetch('/setled?state=' + state);
    }
    function updateLedAssignmentMode(isNetworkMode) {
      const modeVal = isNetworkMode ? 1 : 0;
      const onBtn = document.getElementById("ledOnBtn");
      const offBtn = document.getElementById("ledOffBtn");
      
      if(isNetworkMode) {
        onBtn.classList.add("disabled");
        offBtn.classList.add("disabled");
      } else {
        onBtn.classList.remove("disabled");
        offBtn.classList.remove("disabled");
      }
      fetch('/setledmode?network=' + modeVal);
    }
    function stopTrainSmoothly() { fetch('/smoothstop'); }
    function setDirection(dir) {
      if(dir === 'forward') {
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
      fetch(`/updatephysics?step=${step}&interval=${interval}&wait=${wait}&cooldown=${cooldown}`)
        .then(response => { if(response.ok) alert("Configurations successfully updated and saved!"); });
    }
  </script>
</body>
</html>
)rawliteral";

#endif
