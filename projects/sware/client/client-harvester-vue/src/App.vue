<template>
  <div class="min-h-screen bg-slate-50 flex flex-col">
    <LoginForm v-if="!isLoggedIn" @login-success="handleLoginSuccess" />

    <template v-else>
      <div id="backGr" class="relative">
        <span class="headerFont">Energie Junkies</span>
      </div>

      <Navbar :connected="isConnected" :weather="weather" :currentTime="currentTime" @clear="clearLogs"
        @download="downloadCSV" />

      <main class="flex-grow w-full container mx-auto max-w-7xl ">
        <router-view :liveData="liveData" :logs="logEntries" :isConnected="isConnected" :maxLogs="maxLogs" @update-max="v => maxLogs = v" />

        <!--    <router-view :liveData="liveData" :logs="logEntries" />
        <router-view :logs="logEntries" :isConnected="isConnected" :maxLogs="maxLogs" @update-max="v => maxLogs = v" /> -->
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
  <transition name="fade">
    <div v-if="toast.show" :class="toast.type === 'success' ? 'bg-emerald-500' : 'bg-rose-500'"
      class="fixed bottom-5 right-5 text-white px-6 py-3 rounded-xl shadow-2xl z-[100] font-bold">
      {{ toast.message }}
    </div>
  </transition>
</template>



<script setup>
import { ref, onMounted, onUnmounted, provide } from "vue";
import Navbar from "./components/TopNavbar.vue";
import LoginForm from './components/LoginForm.vue';
import { db } from './db';

const weather = ref({ temp: '--', label: 'Lade...', icon: 'Loading' });


let socket = null;
const isClearing = ref(false);
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
  sessionStorage.setItem('isLoggedIn', 'true');
  startApplication(); // Jetzt erst Dienste starten
  // Erst nach dem Login fangen wir an, die Daten vom ESP32 zu laden
};

const logout = () => {
  sessionStorage.removeItem('isLoggedIn');
  isLoggedIn.value = false;
};




const saveToDb = async (newEntries) => {
  try {
    // 'put' fügt ein oder aktualisiert, falls ts schon existiert (Deduplizierung!)
    await db.logs.bulkPut(newEntries);
  } catch (error) {
    console.error("Fehler beim Speichern in IndexedDB:", error);
  }
};

const cleanOldLogs = async () => {
  const thirtyDaysAgo = Date.now() / 1000 - (30 * 24 * 60 * 60);
  await db.logs.where('ts').below(thirtyDaysAgo).delete();
};


const parseLogBlob = async (base64String, count) => {
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
  if (entries.length > 0) {
    // 1. In die Datenbank schreiben (Deduplizierung passiert automatisch via 'ts')
    await saveToDb(entries);
  }

  return entries;
};

const handleWebSockData = async (data) => {
  if (isClearing.value) return;
  console.log("handleWebSockData aufgerufen mit Daten:", data);

  // 1. Prüfen, ob der neue 'blob' vorhanden ist
  if (data.log && data.log.blob && data.log.len > 0) {

    // 2. Den Blob in ein Array von Objekten umwandeln
    const decodedEntries = await parseLogBlob(data.log.blob, data.log.len);
    console.table(decodedEntries);
    // 3. Bestehende Deduplizierungs-Logik anwenden
    decodedEntries.forEach((newEntry) => {
      const exists = logEntries.value.some(e => e.ts === newEntry.ts);
      if (exists) return;
      const last = logEntries.value[0]; // Der aktuell neueste im Speicher

      // Ähnlichkeitscheck (angepasst an die neuen Feldnamen im Struct)
      const isIdentical = last &&
        newEntry.state === last.state &&
        newEntry.power === last.power &&
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
  socket = new WebSocket(gateway);

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




const startApplication = async () => {
  console.log("Starte App-Dienste...");
  if (socket) {
    console.log("WebSocket bereits offen, schließe alten Socket...");
    socket.onclose = null;
    socket.close();
    socket = null; // Speicher freigeben
  }

  // 1. Zuerst lokale Daten laden (Sorgt für sofortige Anzeige)
  try {
    await cleanOldLogs();
    const savedLogs = await db.logs.orderBy('ts').toArray();
    if (savedLogs.length > 0) {

      logEntries.value = savedLogs;
      console.log(`${savedLogs.length} Logs aus IndexedDB geladen.`);
    }
  } catch (err) {
    console.error("Fehler beim Laden der DB:", err);
  }

  // 2. Dann WebSocket für Live-Daten öffnen
  initWebSocket();
}



// Referenz auf den Socket, damit wir ihn überall im Script erreichen



// --- HIER DER WICHTIGE TEIL ---
onUnmounted(() => {
  if (socket) {
    console.log("Schließe WebSocket vor dem Unmount...");
    socket.onclose = null;
    socket.close();
    socket = null; // Speicher freigeben
  }
});



const toast = ref({ show: false, message: '', type: 'success' });

const showToast = (msg, type = 'success') => {
  toast.value = { show: true, message: msg, type };
  setTimeout(() => toast.value.show = false, 3000); // Nach 3 Sek. ausblenden
};

const downloadCSV = async () => {
  try {
    // 1. Alle Daten aus der IndexedDB holen
    const allLogs = await db.logs.orderBy('ts').toArray();

    if (allLogs.length === 0) {
      alert("Keine Daten zum Exportieren vorhanden.");
      return;
    }

    // 2. Header-Zeile definieren
    const headers = ["Datum/Zeit", "Unix-Timestamp", "L1", "L2", "Leistung (W)", "PWM (%)", "Temperatur (C)"];

    // 3. Daten in Zeilen umwandeln
    const csvRows = allLogs.map(log => {
      // Wir nutzen das gleiche Format wie in der Tabelle für das Datum
      const dateStr = new Date(log.ts * 1000).toLocaleString('de-DE');

      return [
        `"${dateStr}"`,       // In Anführungszeichen, falls Kommas im Datum sind
        log.ts,
        (log.state & 1) ? 1 : 0,
        (log.state & 2) ? 1 : 0,
        log.power,
        log.pwm,
        log.temp
      ].join(';'); // Semikolon ist der Standard für deutsches Excel
    });

    // 4. Alles zusammenfügen (Header + Daten)
    const csvString = [headers.join(';'), ...csvRows].join('\n');

    // 5. Blob erstellen (mit UTF-8 Byte Order Mark für Excel-Kompatibilität)
    const BOM = "\uFEFF";
    const blob = new Blob([BOM + csvString], { type: "text/csv;charset=utf-8;" });

    // 6. Download auslösen
    const url = window.URL.createObjectURL(blob);
    const link = document.createElement("a");
    link.href = url;
    link.download = `harvester_export_${new Date().toISOString().slice(0, 10)}.csv`;

    document.body.appendChild(link);
    link.click();

    // 7. Cleanup
    document.body.removeChild(link);
    window.URL.revokeObjectURL(url);

    console.log("CSV Export erfolgreich.");
  } catch (err) {
    console.error("CSV Export Fehler:", err);
  }
};

const clearLogs = async () => {
  if (!confirm("Alle Logs löschen?")) return;

  isClearing.value = true; // Blockiert eingehende WebSocket-Daten

  try {
    // 1. In der Datenbank löschen
    await db.logs.clear();

    // 2. Im Vue-State löschen (Anzeige leeren)
    logEntries.value = [];
    showToast("Datenbank erfolgreich geleert");
    console.log("Datenbank erfolgreich geleert.");
  } catch (err) {
    console.error("Fehler beim Löschen der Logs:", err);
    showToast("Fehler beim Löschen der Logs");
  }
  finally {
    // 3. Kurz warten, um sicherzustellen, dass keine neuen Daten reinkommen, während wir löschen
    isClearing.value = false; // Gibt die Verarbeitung wieder frei
  }

};



const fetchWeather = async () => {
  try {
    // Beispiel-Koordinaten (Wien). Setze hier deine exakten Daten ein!
    const lat = 48.24;
    const lon = 13.3308;
    const response = await fetch(`https://api.open-meteo.com/v1/forecast?latitude=${lat}&longitude=${lon}&current=temperature_2m,weather_code`);
    const data = await response.json();

    const code = data.current.weather_code;
    weather.value = {
      temp: Math.round(data.current.temperature_2m),
      // Mapping der Open-Meteo WMO Codes
      status: code === 0 ? 'sunny' : (code < 4 ? 'cloudy' : 'rainy'),
      label: code === 0 ? 'Klar' : (code < 4 ? 'Wolkig' : 'Regen')
    };
  } catch (e) {
    console.error("Wetter konnte nicht geladen werden", e);
  }
};
provide('harvesterStats', liveData);
provide('connectionStatus', isConnected);
onMounted(async () => {
  if (sessionStorage.getItem('isLoggedIn') === 'true') {
    isLoggedIn.value = true;
    startApplication();
    fetchWeather();
    setInterval(fetchWeather, 1000 * 60 * 15); // Alle 15 Min aktualisiere
  }
  //initWebSocket()
});

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
</script>

<style></style>