<template>
  <div class="min-h-screen bg-slate-50 flex flex-col">
    <LoginForm v-if="!isLoggedIn" @login-success="handleLoginSuccess" />

    <template v-else>
      <div id="backGr" class="relative">
        <span class="headerFont">Energie Junkies</span>
      </div>

      <Navbar :weather="weather" @clear="clearLogs"
        @download="downloadCSV" />

      <main class="flex-grow w-full container mx-auto max-w-7xl ">
        <router-view :liveData="liveData" :logs="logEntries" :isConnected="isConnected" :maxLogs="maxLogs" @clear="clearLogs" />

      </main>

      <footer class="w-full flex flex-col sm:flex-row items-center border-t border-slate-200 bg-white overflow-hidden p-0 m-0">
        <img src="/img/Energies.jpg" alt="Footer Logo" class="h-12 sm:h-20 w-full sm:w-auto block m-0 p-0 object-cover" />
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
import { parseLogBlob } from './utils/logs.js';

const weather = ref({ temp: '--', label: 'Lade...', icon: 'Loading' });

let socket = null;
let reconnectTimer = null;
let disposed = false;
const isClearing = ref(false);
const logEntries = ref([]);
const maxLogs = ref(2000); // Erhöht: 50 → 2000 (ESP32 RingBuffer=60, IndexedDB persistent)

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

const handleWebSockData = async (data) => {
  if (data?.live) liveData.value = data.live;
  if (isClearing.value || !data?.log?.blob || data.log.len <= 0) return;

  // 2. Den Blob in ein Array von Objekten umwandeln
  const decodedEntries = parseLogBlob(data.log.blob, data.log.len);
  if (decodedEntries.length) await saveToDb(decodedEntries);

  // 4. Deduplizierungs-Logik anwenden (korrekt: 'last' neu bei jedem Durchlauf lesen!)
  for (const newEntry of decodedEntries) {
    const exists = logEntries.value.some(e => e.ts === newEntry.ts);
    if (exists) continue;

    // AKTUELLEN letzten Eintrag lesen (nicht vor der Schleife!)
    const last = logEntries.value[0];

    // Ähnlichkeitscheck
    const isIdentical = last &&
      newEntry.state === last.state &&
      newEntry.power === last.power &&
      newEntry.pwm === last.pwm &&
      newEntry.temp === last.temp;

    if (isIdentical) {
      last.lastSeen = newEntry.ts;
    } else {
      logEntries.value.unshift({
        ...newEntry,
        firstSeen: newEntry.ts,
        lastSeen: newEntry.ts
      });
    }
  }

  // 5. Speicherlimit einhalten (Slice)
  if (logEntries.value.length > maxLogs.value) {
    logEntries.value = logEntries.value.slice(0, maxLogs.value);
  }
};

const initWebSocket = () => {
  if (disposed) return;
  const isLocal = import.meta.env.DEV ||
    window.location.hostname === 'localhost' ||
    window.location.hostname === '127.0.0.1';

  // FIX #3: Kein Doppel-Socket wenn bereits verbunden
  if (socket && socket.readyState <= 1) {
    return;
  }

  if (isLocal) {
    isConnected.value = true;
    return;
  }

  const gateway = `ws://${window.location.hostname}/ws`;
  socket = new WebSocket(gateway);

  socket.onopen = () => {
    isConnected.value = true;
  };
  // FIX #4: onerror-Handler für stille Verbindungsabbrüche
  socket.onerror = (err) => {
    isConnected.value = false;
    console.error("WebSocket Fehler:", err);
  };
  socket.onclose = () => {
    isConnected.value = false;
    // Reconnect nur wenn Socket wirklich geschlossen
    if (socket && socket.readyState !== 3) return;
    reconnectTimer = setTimeout(initWebSocket, 3000);
  };
  socket.onmessage = async (event) => {
    try {
      const data = JSON.parse(event.data);
      isConnected.value = true;
      await handleWebSockData(data);
    } catch (err) {
      console.error("WebSocket Message Parse Error:", err);
    }
  };
};

const closeWebSocket = () => {
  clearTimeout(reconnectTimer);
  if (socket) {
    socket.onopen = null;
    socket.onclose = null;
    socket.onerror = null;
    socket.onmessage = null;
    if (socket.readyState <= 1) {
      socket.close();
    }
    socket = null;
  }
};

const startApplication = async () => {
  closeWebSocket();

  // 1. Zuerst lokale Daten laden (Sorgt für sofortige Anzeige)
  try {
    await cleanOldLogs();
    const savedLogs = await db.logs.orderBy('ts').toArray();
    if (savedLogs.length > 0) {
      // firstSeen/lastSeen anreichern (fehlen bei reinem DB-Ladestand)
      logEntries.value = savedLogs.map(entry => ({
        ...entry,
        firstSeen: entry.firstSeen ?? entry.ts,
        lastSeen: entry.lastSeen ?? entry.ts
      }));
      console.log(`${savedLogs.length} Logs aus IndexedDB geladen.`);
    }
  } catch (err) {
    console.error("Fehler beim Laden der DB:", err);
  }

  // 2. Dann WebSocket für Live-Daten öffnen
  initWebSocket();
}

// FIX #2: Timer-IDs speichern für sauberes onUnmounted-Cleanup
let weatherTimer = null;

onUnmounted(() => {
  disposed = true;
  closeWebSocket();
  if (weatherTimer) {
    clearInterval(weatherTimer);
    weatherTimer = null;
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
    // 1. ESP32 RingBuffer leeren (AJAX)
    let esp32Ok = true;
    try {
      const res = await fetch('/clearLogs', { method: 'POST' });
      const body = await res.json();
      esp32Ok = !!body.done;
    } catch (ajaxErr) {
      // ESP32 nicht erreichbar (Dev-Modus o. Ä.) — nicht kritisch
      console.warn("ESP32-RingBuffer-Clear fehlgeschlagen:", ajaxErr);
      esp32Ok = false;
    }

    // 2. In der Datenbank löschen
    await db.logs.clear();

    // 3. Im Vue-State löschen (Anzeige leeren)
    logEntries.value = [];

    // 4. Gebündeltes Feedback
    if (esp32Ok) {
      showToast("Alle Logs erfolgreich gelöscht");
    } else {
      showToast("Browser-Logs gelöscht — ESP32-Buffer konnte nicht geleert werden", "error");
    }
    console.log("Logs geleert, ESP32-Status:", esp32Ok ? "OK" : "FEHLER");
  } catch (err) {
    console.error("Fehler beim Löschen der Logs:", err);
    showToast("Fehler beim Löschen der Logs");
  } finally {
    // 5. Kurz warten, um sicherzustellen, dass keine neuen Daten reinkommen
    setTimeout(() => { isClearing.value = false; }, 1500);
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
    // FIX #2: Timer-ID speichern für Cleanup
    weatherTimer = setInterval(fetchWeather, 1000 * 60 * 15);
  }
});

</script>

<style></style>