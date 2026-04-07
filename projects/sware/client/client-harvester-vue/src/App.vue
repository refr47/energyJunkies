<template>
  <div class="min-h-screen bg-slate-50 flex flex-col">
    <LoginForm v-if="!isLoggedIn" @login-success="isLoggedIn = true" />

    <template v-else>
      <div id="backGr" class="relative">
        <span class="headerFont">Energie Junkies</span>
      </div>

      <header>
        <Navbar :connected="isConnected" />
      </header>

      <main class="flex-grow w-full container mx-auto max-w-7xl">
        <router-view :liveData="liveData" :logs="logEntries" />
        <router-view :logs="logEntries" :maxLogs="maxLogs" @update-max="v => maxLogs = v" />
      </main>

      <footer class="w-full flex items-center border-t border-slate-200 bg-white overflow-hidden p-0 m-0">
        <img src="/img/Energies.jpg" alt="Footer Logo" class="h-20 block m-0 p-0 object-fill"
          style="width: 2000px !important;" />
        <div class="flex-grow text-right pr-5">
          <p class="text-slate-400 text-[10px] uppercase m-0">
            &copy; 2024-2026 Energie Junkies
          </p>
        </div>
      </footer>
    </template>
  </div>
</template>



<script setup>
import { ref, onMounted, onUnmounted, provide } from "vue";
import Navbar from "./components/TopNavbar.vue";
import LoginForm from './components/LoginForm.vue';


const logEntries = ref([]);
const maxLogs = ref(50); // Einstellbar

const isLoggedIn = ref(false);
const liveData = ref({

  battery: { soc: 0, power: 0 },
  pv: { power: 0 },
});
const isConnected = ref(false);
const handleLoginSuccess = () => {
  isLoggedIn.value = true;
  // Erst nach dem Login fangen wir an, die Daten vom ESP32 zu laden
};

const logout = () => {
  sessionStorage.removeItem('isLoggedIn');
  isLoggedIn.value = false;
};


const handleLogin = async () => {
  const payload = {
    user: username.value,
    password: password.value
  };

  try {
    const response = await fetch('/login', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(payload)
    });

    if (response.status === 200) {
      const data = await response.json();
      if (data.authenticated) {
        sessionStorage.setItem('isLoggedIn', 'true');
        emit('login-success');
      }
    } else {
      showError("Login fehlgeschlagen. Passwort prüfen.");
    }
  } catch (err) {
    showError("ESP32 nicht erreichbar.");
  }
};




const handleWebSockData = (data) => {
  // Prüfen, ob das 'log' Objekt und das 'entries' Array existieren
  console.log("handleWebSockData aufgerufen mit Daten:", data);
  if (data.log && Array.isArray(data.log.entries) && data.log.entries.length > 0) {

    data.log.entries.forEach((newEntry) => {
      const last = logEntries.value[0]; // Der aktuell neueste im Speicher

      // Ähnlichkeitscheck (Deduplizierung)
      // Wir prüfen: L1, L2, PWM und Temperatur. Der Timestamp wird ignoriert.
      const isIdentical = last &&
        newEntry.l1 === last.l1 &&
        newEntry.l2 === last.l2 &&
        newEntry.pwm === last.pwm &&
        newEntry.temp === last.temp &&
        newEntry.tag === last.tag; // Auch gleicher Dienst?
      //console.log("newEntry und dann:", newEntry);
      if (isIdentical) {
        // Falls identisch: Wir aktualisieren nur, wann wir diesen Zustand zuletzt gesehen haben
        last.lastSeen = newEntry.timestamp;
        
      } else {
       
        // Falls neu oder geändert: Vorne ins Array einfügen
        logEntries.value.unshift({
          ...newEntry,
          firstSeen: newEntry.ts,
          lastSeen: newEntry.ts
        });
       

      }
    });

    // Ringpuffer-Sicherheit: Speicherlimit einhalten
    if (logEntries.value.length > maxLogs.value) {
      logEntries.value = logEntries.value.slice(0, maxLogs.value);
    }
  }
};

const initWebSocket = () => {
  let simTimer = null; // WICHTIG: Hier oben parken
  const isLocal = import.meta.env.DEV ||
    window.location.hostname === 'localhost' ||
    window.location.hostname === '127.0.0.1';
  console.log("WebSocket Init: Ist lokale Entwicklung?", isLocal);



  if (isLocal && !simTimer) {
    console.log("🛠️ Dev-Mode: Simuliere ESP32 Daten...");
    simTimer = setInterval(simulateData, 3000);

    isConnected.value = true;
    return;
  }
  console.log("🔌 Verbinde mit ESP32 WebSocket...");

  const gateway = `ws://${window.location.hostname}/ws`;
  const socket = new WebSocket(gateway);

  socket.onopen = () => {
    isConnected.value = true;
  };
  socket.onclose = () => {
    isConnected.value = false;
    setTimeout(initWebSocket, 3000);
  };
  socket.onmessage = (event) => {
    const data = JSON.parse(event.data);
    isConnected.value = true;
    liveData.value = data.live;
    //logEntries.value = data.log.entries;
    handleWebSockData(data);



  };
};
// Simulation für die Entwicklung am PC
const simulateData = () => {  
  console.log("🔄 Simuliere ESP32 Daten...beginnend mit Live-Daten");
  liveData.value.netzBezug = Math.random() > 0.5 ? 500 : -500; // Simuliere 
  liveData.value.ev = 1500;
  liveData.value.solEr = 2000;
  liveData.value.bTemp = 45;
  liveData.value.L1 = Math.random() > 0.5 ? 1 : 0; // Simuliere Entladen/
  liveData.value.L2 = Math.random() > 0.5 ? 1 : 0; // Simuliere Entladen/
  liveData.value.L3 = Math.random() > 0.5 ? 10 : 24; // Simuliere Entladen/
  liveData.value.forceHeizung = Math.random() > 0.5 ? 1 : 0;

  liveData.value.errors = 0; // Simuliere Bit 0 Fehler
  liveData.value.aakHasBattery = false;
  liveData.value.aakStat = Math.random() > 0.5 ? 20 : 40; // Simuliere Export/
  liveData.value.aakPower = Math.random() > 0.5 ? 3000 : 2000; // Simuisliere Export/
  liveData.value.aakEntladen = Math.random() > 0.5 ? -1 : 1; // Simuliere Entladen/
  console.log("isLoggedIn:", isLoggedIn.value);
  if (isLoggedIn.value) {
    console.log("Simuliere Log-Daten... mockPayload wird erstellt und verarbeitet");
    // 2. MOCK-LOGS (Tabelle)
    // WICHTIG: Das Objekt muss EXAKT so aufgebaut sein, wie handleWebSockData es erwartet
    const mockPayload = {
      log: {
        entries: [
          { ts: Date.now(), l1: 1, l2: 0, pwm: 0, temp: 45, tag: "ENERGY" },
          { ts: Date.now(), l1: 0, l2: 1, pwm: 100, temp: 46, tag: "ENERGY" },
          { ts: Date.now(), l1: 1, l2: 0, pwm: 0, temp: 85, tag: "ALARM" }
        ]
      }
    };

    // 3. Aufruf der Verarbeitungsfunktion
    // Hier wird sichergestellt, dass mockPayload NICHT undefined ist
    if (mockPayload && mockPayload.log) {
      handleWebSockData(mockPayload);
    }
  }

}
onUnmounted(() => socket?.close());
provide('harvesterStats', liveData);
provide('connectionStatus', isConnected);
onMounted(() => {
  // Check, ob wir noch eine aktive Sitzung im Browser-Tab haben
  if (sessionStorage.getItem('isLoggedIn') === 'true') {
    isLoggedIn.value = true;
  }
  initWebSocket()
});

</script>

<style></style>
