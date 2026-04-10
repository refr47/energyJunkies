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
        <router-view :logs="logEntries" :isConnected="isConnected"  :maxLogs="maxLogs" @update-max="v => maxLogs = v" />
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


const parseLogBlob = (base64String, count) => {
  const binaryString = window.atob(base64String);
  const len = binaryString.length;
  const bytes = new Uint8Array(len);
  for (let i = 0; i < len; i++) {
    bytes[i] = binaryString.charCodeAt(i);
  }

  const view = new DataView(bytes.buffer);
  const entries = [];

  // WICHTIG: Die structSize muss EXAKT deinem C-Struct entsprechen!
  // uint32(4) + uint8(1) + int16(2) + uint16(2) + int16(2) = 11 Bytes
  /*
  struct __attribute__((packed)) LogEntry
{ 
    uint32_t ts;
    uint8_t state;
    int16_t power;
    uint8_t pwm;    
    int temp;
};
*/
  // const safeCount = Math.min(count, 60);
  /*
VariableC-TypBytesSummetsuint32_t44stateuint8_t15powerint16_t27pwmuint8_t18tempint16_t210

  */

  const structSize = 10;

  for (let i = 0; i < count; i++) {
    let offset = i * structSize;
    entries.push({
      ts: view.getUint32(offset + 0, true), // Byte 0, 1, 2, 3
      state: view.getUint8(offset + 4),        // Byte 4
      power: view.getInt16(offset + 5, true),  // Byte 5, 6
      pwm: view.getUint8(offset + 7),        // Byte 7
      temp: view.getInt16(offset + 8, true)   // Byte 8, 9
    });
  }
  return entries;
};

const handleWebSockData = (data) => {
  console.log("handleWebSockData aufgerufen mit Daten:", data);

  // 1. Prüfen, ob der neue 'blob' vorhanden ist
  if (data.log && data.log.blob && data.log.len > 0) {

    // 2. Den Blob in ein Array von Objekten umwandeln
    const decodedEntries = parseLogBlob(data.log.blob, data.log.len);

    // 3. Bestehende Deduplizierungs-Logik anwenden
    decodedEntries.forEach((newEntry) => {
      const last = logEntries.value[0]; // Der aktuell neueste im Speicher

      // Ähnlichkeitscheck (angepasst an die neuen Feldnamen im Struct)
      const isIdentical = last &&
        newEntry.state === last.state &&
        newEntry.pwr === last.pwr &&
        newEntry.pwm === last.pwm &&
        newEntry.temp === last.temp;

      if (isIdentical) {
        // Nur Zeitstempel aktualisieren
        last.lastSeen = newEntry.ts;
      } else {
        // Neu oder geändert -> vorne einfügen
        logEntries.value.unshift({
          ...newEntry,
          firstSeen: newEntry.ts,
          lastSeen: newEntry.ts
        });
      }
    });

    // 4. Speicherlimit einhalten (Slice)
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
