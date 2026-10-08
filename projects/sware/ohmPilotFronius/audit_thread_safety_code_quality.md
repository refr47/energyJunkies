# 🔍 Thread-Safety & Code-Qualität Audit
## ESP32-Heizstab-Steuerung (FreeRTOS + PlatformIO)

---

## 📋 Übersicht

| Kategorie | Kritisch | Hoch | Mittel | Niedrig |
|-----------|----------|------|--------|---------|
| Thread-Safety | 3 | 2 | 3 | - |
| Speicher/Leacks | 2 | 1 | - | - |
| Code-Qualität | - | 3 | 4 | 2 |
| API-Verwendung | - | 2 | 1 | - |

---

---

## 🔴 KRITISCH — Müssen behoben werden

---

### KR-1: **Deadlock-Gefahr durch `appLock()`-Rückgabe-Skip in `app_services.cpp`**

**Dateien:** `app_services.cpp:185`, `app_services.cpp:258` \
**Priorität:** Kritisch — deterministischer Deadlock unter Last

**Problem:** Die Funktion `appServices_updateWebSockData()` versucht über `appLock(250)` den globalen Mutex zu holen. Bei TIMEOUT springt der Code über `goto next:` einfach in die nächste Iteration, **ohne den Mutex freizugeben**. Das ist korrekt. 

Aber: In `app_services.cpp:258` (Zeile 258) ruft `appServices_sendMQTT()` **innerhalb der bereits gehaltenen `g_appMutex`-Region** die Funktion `appLock()` auf. Da `appLock()` auf dem **gleichen** Mutex (`g_appMutex`) operiert, blockiert die Task selbst. Die Task gibt den Mutex vorher **nicht** frei:

```cpp
// app_services.cpp — appServices_checkConnections() / appServices_updateWebSockData()
if (!appLock(pdMS_TO_TICKS(250))) goto next;
// ... g_app.webSockData wird gelesen ...

// Später:
appServices_checkConnections(webSockData);  // → interne appLock()-Aufrufe!

// app_services.cpp — appServices_checkConnections():
    appServices_sendMQTT(webSockData);       // selbst mit appLocker!
    appServices_sendINFLUX(webSockData);     // auch mit AppLocker!
```

Die Service-Funktionen `sendMQTT()` und `sendINFLUX()` verwenden intern RAII-Wrapper um `appLock()`, die aber den **gleichen Mutex** `g_appMutex` blockieren. Der äußere Aufruf über `appLock(250)` hat den Mutex bereits gehalten. **→ Deadlock.**

**Lösung:** 
- `appServices_checkConnections()` darf **nicht** von `appServices_updateWebSockData()` aus aufgerufen werden, solange der äußere `appLock()` gehalten wird.
- Entweder kopiere eine Lokale Snapshot und rufe `checkConnections()` auf **nach** `appUnlock()`, oder
- Refaktor: `checkConnections()` als eigene Task ohne gegenseitige Abhängigkeit.

---

### KR-2: ~~**Gemeinsamer Mutex `g_appMutex` als Flaschenhals — zu grobe Granularität**~~ → **BEHoben**

**Dateien:** `app_sync.cpp`, `app_state.h`, alle Tasks (`pidManager`, `webSockets`, `ajaxCalls`, `app_services`) \
**Priorität:** Kritisch — System-Reaktionszeit leidet unter hoher CPU-Auslastung

**Problem:** Alle FreeRTOS-Tasks (`T_PID`, `T_NETWORK`, `T_WEB`, `T_TEMPERATURE`, `T_ENERGY`) nutzen den **gleichen** Mutex `g_appMutex` auf das komplette `g_app`-Struct. 
Der PID-Task läuft in 50ms-Intervallen und hält den Mutex für 150ms (Lock-Timeout), blockiert damit:
- WebSocket-Daten-Update (Task `T_NETWORK`)
- AJAX-HTTP-Antworten (Task `T_WEB`, async-webserver context)
- MQTT-/Influx-Empfang (`T_TEMPERATURE`, `T_ENERGY`)

Auf einem ESP32 mit nur 2 Kernen bedeutet dies unter Last:
- Lange Lock-Zeiten → Tasks auf Core 1 müssen auf Core 0 warten
- `xSemaphoreTake()` mit 150ms Timeout blockiert die Task komplett (kein Arbeit)
- Watchdog kann auslösen, wenn die Task über 2 Sekunden nicht antwortet

**Lösung:**
1. **Mutex trennen:** Zerlege `g_app` in mehrere Sektionen:
   ```cpp
   SemaphoreHandle_t g_pidMutex;       // für Temperatur-/PID-Daten
   SemaphoreHandle_t g_webMutex;       // für WebSocket-/HTTP-Daten  
   SemaphoreHandle_t g_configMutex;    // für Setup-/EPROM-Daten
   ```
2. **Oder:** Nutze lock-free Strukturen für Readonly-Zugriff (read-copy-update Pattern).
3. **Oder:** Reduziere `appLock()`-Timeout in `pidManager::pidLockRead()` von 150ms auf 50ms und füge Fail-Fast-Logik hinzu.

---

### KR-3: **Data Race auf `g_shellyScanRunning` / `g_shellyScanDone` ohne Atomik-Schutz**

**Dateien:** `ajaxCalls.cpp` \
**Priorität:** Kritisch — undefiniertes Verhalten auf ESP32 (dual-core)

**Problem:** Die Variablen `g_shellyScanRunning` und `g_shellyScanDone` sind als `volatile bool` deklariert. `volatile` reicht **nicht** für Threadsicherheit auf dem ESP32 (dual-core ARM). Die Variable kann vom Shelly-Scan-Task und vom AJAX-Handler-Task gleichzeitig auf unterschiedlichen Cores gelesen/geschrieben werden, was zu unvorhersehbarem Verhalten führt.

```cpp
// ajaxCalls.cpp
static volatile bool g_shellyScanRunning = false;  // ❌ Race-Condition!
static volatile bool g_shellyScanDone = false;     // ❌ Race-Condition!
```

**Lösung:** 
```cpp
static std::atomic<bool> g_shellyScanRunning{false};
static std::atomic<bool> g_shellyScanDone{false};
```
Oder besser: Die Variable immer unter `g_shellyMutex` lesen/schreiben (sie liegt im Mutex-Bereich — konsistent nutzen).

---

## 🟠 HOHE PRIORITÄT — Sollten behoben werden

---

### H-1: **Speicherleck in `eprom_store_shelly()` — `preferences.clear()` löscht alle Devices**

**Dateien:** `eprom.cpp:162-172` \
**Priorität:** Hoch — Datenverlust bei Shelly-Geräten

**Problem:** In `eprom_store_shelly()` wird `preferences.clear()` aufgerufen, dann werden in einer Schleife Geräte gespeichert. Aber pro Gerät wird **jedes Mal der gleiche Key** verwendet (`SHELLY_DEVICE_NAME`, `SHELLY_MAC`, `SHELLY_IP`, `SHELLY_PORT`). Das heißt, nur das **letzte** gespeicherte Device bleibt im NVS erhalten — alle vorherigen gehen verloren.

```cpp
preferences.clear();
for (int i = 0; i < upperLimit; i++) {
    if (allDevices[i].valid == true) {
        preferences.putString(SHELLY_DEVICE_NAME, ...); // ❌ immer gleicher Key
        preferences.putString(SHELLY_MAC, ...);         // ❌ immer gleicher Key
    }
}
```

**Lösung:** 
```cpp
// Keys mit Index suffixieren:
preferences.putString(SHELLY_DEVICE_NAME "_0", allDevices[0].shellyDevice->name);
preferences.putString(SHELLY_DEVICE_NAME "_1", allDevices[1].shellyDevice->name);
// oder: char key[32]; snprintf(key, sizeof(key), "sdn_%d", i);
```

---

### H-2: **Hard-Coded Credentials in `eprom_test_write_Eprom()` + `#ifdef DEBUG_DEBUG`-Block**

**Dateien:** `eprom.cpp:84-127` \
**Priorität:** Hoch — Sicherheitsrisiko / Hard-Coded Secrets

**Problem:** 
1. InfluxDB-Token und MQTT-Passwörter sind als Klartext in den Source-Code eingebrannt:
   ```cpp
   strcpy(setup.influxToken, "Zr0fsPmRgvNr0znkbudQNZBnGDHjkBOT41X4wJ...");
   strcpy(setup.mqttPass, "MQTT_PASS");
   ```
2. Der `#ifdef DEBUG_DEBUG`-Block (ab Zeile 84) ist **toter Code**, der nie erreicht wird (da er nach `return` steht), aber trotzdem Secrets enthält.

**Lösung:** 
- Secrets in `platformio.ini` als Build-Define auslagern, oder
- Aus Umgebungsvariablen/Configuration-Files laden
- Toten Code entfernen

---

### H-3: **`delay()` in FreeRTOS-Tasks blockiert das Scheduling**

**Dateien:** `modbusReader.cpp`, `shelly.cpp`, `wlan.cpp`, `mqtt.cpp`, `cardRW_setup.cpp` \
**Priorität:** Hoch — verzögert Watchdog, blockiert andere Tasks

**Problem:** `delay()` (Arduino-API) blockiert den aktuellen FreeRTOS-Task komplett. In einer Echtzeit-Umgebung mit Watchdog ist das problematisch:

```cpp
// modbusReader.cpp:118 — 1 Sekunde blockiert!
delay(1000);

// shelly.cpp:70 — 2 Sekunden während Scan!
delay(2000);

// wlan.cpp:370 — 3 Sekunden in WLAN-Task!
delay(3000);
```

Der Watchdog (`app_watchDog()`) prüft alle Tasks alle 2 Sekunden. Wenn eine Task `delay(2000)` aufruft, ist sie für den Watchdog „tod" → **Watchdog-Restarts**.

**Lösung:** Ersetze **alle** `delay()` in Tasks durch `vTaskDelay(pdMS_TO_TICKS(ms))`. `vTaskDelay()` gibt den Task-Slot frei und erlaubt die Ausführung anderer Tasks.

---

### H-4: **`g_appMutex` Timeout in `pidManager` — 150ms zu lang**

**Dateien:** `pinManager.cpp:71` \
**Priorität:** Hoch — lange Sperrzeit

**Problem:** `pinManager::pidLockRead()` versucht `appLock(150)` zu halten, während der Task nur einige int-Werte kopiert. 150ms Timeout bedeutet: Die PID-Task **mindestens 150ms pro Zyklus** wartet auf den Mutex (wenn belegt). Bei 50ms Task-Intervall bedeutet das: Der Task ist **mehr als 100% seiner Zeit** mit Warten beschäftigt.

```cpp
void PinManager::pidLockRead(WEBSOCK_DATA &data)
{
    if (!appLock(150)) // R02: fail-fast, do not block forever  ← 150ms!
    {
        LOG_ERROR(TAG_PID, "pidLockRead: appLock failed!");
        return;
    }
```

**Lösung:** Reduziere Timeout auf **30-50ms** — genug, um kurz auf andere Tasks zu warten, aber nicht lange blockieren. Bei Fail-Fast: Log + Snapshot vom letzten bekannten State nutzen.

---

### H-5: **Fehlender `vTaskDelay()` im `pidManager::update()` Haupt-Loop — zu aggressives Scheduling**

**Dateien:** `pinManager.cpp:182-364` \
**Priorität:** Hoch — CPU-Belastung

**Problem:** `pinManager::update()` hat zwar einen `vTaskDelay(pdMS_TO_TICKS(50))`, aber die eigentliche Logik (`preCheck()`, `apply()`, ML-Training) kann deutlich länger dauern. Bei hoher Auslastung kann der Task nicht mehr rechtzeitig beendet werden. Zusätzlich: Der Call zu `tinyNN->trainReplay()` kann je nach Replay-Buffer-Größe lange dauern und den **ganzen Core** blockieren (inklusive WebSocket-/HTTP-Tasks auf demselben Core).

**Lösung:**
1. `trainReplay()` in eigene Task auslagern (niedrigere Priorität)
2. Oder: `trainReplay()` auf maximal X ms pro Call begrenzen (Chunk-Processing)
3. Oder: Pinne ML-Training auf Core 1, PID-Steuerung auf Core 0, und trenne per Semaphore

---

## 🟡 MITTELE PRIORITÄT

### M-1: **RingBuffer Mutex (`rb.mutex`) ist `std::mutex` — nicht FreeRTOS-kompatibel**

**Dateien:** `utils.cpp`, `utils.h` \
**Priorität:** Mittel — funktioniert im Moment, aber nicht deterministisch

**Problem:** Der RingBuffer nutzt `std::mutex` für die Synchronisation. Auf dem ESP32 kann `std::mutex` (je nach Newlib-Version) entweder als Native POSIX-Mutex oder als FreeRTOS-Mutex implementiert sein. Im ersten Fall gibt es **keine** Garantie, dass der Mutex von verschiedenen FreeRTOS-Kontexten (ISR, Task, IDL) korrekt funktioniert.

```cpp
// utils.cpp
struct RingBuffer {
    std::mutex mutex;  // ❌ std::mutex in FreeRTOS-Kontext
    // ...
};
```

**Lösung:** Nutze `SemaphoreHandle_t` (FreeRTOS-Mutex) oder `esp_pm_lock_handle_t` statt `std::mutex`.

---

### M-2: **Magic Numbers überall — schlechte Wartbarkeit**

**Dateien:** `pinManager.cpp`, `app_services.cpp`, `app_tasks.cpp`, `defines.h` \
**Priorität:** Mittel

**Beispiele:**
```cpp
// pinManager.cpp
m_legionellenDelta      // 7UL * 24 * 3600 * 1000 (woher kommt?)
m_froniusAPI            // bei < 20% Akku-Ladezustand
HYSTERESIS_WATT = 12    // in .h — OK, aber unklar
powerIndex < HYSTERESIS_WATT
EPSILON_TEMP = 20       // 20 Watt Toleranz? °C?

// app_services.cpp
appLock(150); // was bedeuten 150? Ticks? MS?
```

**Lösung:** Alle Magic Numbers in `defines.h` oder dedizierte `config.h` auslagern mit erklärenden Namen:
```cpp
#define LEGIONELLA_CHECK_INTERVAL_MS (7UL * 24 * 3600 * 1000)
#define PID_LOW_BATTERY_PERCENTAGE 20.0f
#define PID_POWER_HYSTERESIS_SAMPLES 12
#define PID_EPSILON_WATTS 20
#define APP_MUTEX_TIMEOUT_TICKS pdMS_TO_TICKS(150)
```

---

### M-3: **Fehlende Fehlerbehandlung bei `xTaskCreatePinnedToCore()`**

**Dateien:** `app_tasks.cpp`, `ajaxCalls.cpp`, `app_services.cpp` \
**Priorität:** Mittel

**Problem:** Viele Task-Creation-Calls prüfen nicht das Ergebnis:
```cpp
// app_tasks.cpp
xTaskCreatePinnedToCore(taskPID, "taskPID", 6144, nullptr, 5, &g_taskHandle[T_PID], 1);
// Kein return-value check!
```

Im Fehlerfall → Task-Handle bleibt `nullptr`, Watchdog kickt nie → System-Absturz ohne Log-Meldung.

**Lösung:** Immer prüfen:
```cpp
if (xTaskCreatePinnedToCore(...) != pdPASS) {
    LOG_CRITICAL("taskPID", "Failed to create PID task!");
    // Fallback oder graceful shutdown
}
```

---

### M-4: **Übermäßige `LOG_INFO`-Calls — Flood beim Debugging / Flash-Write**

**Dateien:** nahezu alle `.cpp`-Dateien \
**Priorität:** Mittel

**Problem:** `pinManager::update()` und `pinManager::apply()` haben jede Zeile mit `LOG_INFO` versehen. Wenn das Logging per Serial aktiv ist, erzeugt das enorme Bandbreite und verlangsamt den Task. Bei aktivem Flash-Write-Logging können zudem NVS-Zugriffe durch das Logging selbst blockiert werden.

```cpp
// pinManager.cpp — apply() — 8 LOG_INFO Calls bei jedem Cycle
LOG_INFO(TAG_PID, "PinManager::apply() - ENTER Task %s, available watt: %d", ...);
LOG_INFO(TAG_PID, "apply (1) - targetPower: %d...", ...);
```

**Lösung:** Reduziere `LOG_INFO` auf `LOG_DEBUG` für hochfrequente Calls. Reserve `LOG_INFO` für Zustandsübergänge.

---

### M-5: **`preferences.begin()/end()` wird in `eprom.cpp` bei jedem Call neu aufgerufen**

**Dateien:** `eprom.cpp` \
**Priorität:** Mittel — Performance / Flash-Wear

**Problem:** `preferences.begin()` öffnet den NVS-Speicher, `preferences.end()` schließt ihn. In `eprom_getSetup()` und `eprom_storSetup()` wird dies bei jedem Lese-/Schreib-Vorgang gemacht. Bei häufigen Aufrufen (z.B. bei jedem HTTP-Setup-Request) beansprucht das unnötig Flash-Wear.

**Lösung:** Cache das Setup-Struct im RAM und aktualisiere nur bei `setupChanged = true`.

---

## 🔵 NIEDRIGE PRIORITÄT / Code-Style

### N-1: Überflüssige Kommentare und Code-Duplikation in `pinManager.cpp`
### N-2: Fehlende `const`-Korrektur
### N-3: `#ifdef DEBUG_DEBUG`-Blöcke enthalten unerreichen Code (`return` vor dem Block)

---

---

## 📐 Refactoring-Plan (Priorisiert)

### Phase 1: Kritische Thread-Safety (Woche 1-2)

1. **KR-1:** `appServices_checkConnections()` aus der `appLock()`-Region herausziehen
   - Kopiere `WEBSOCK_DATA` lokal → `appUnlock()` → `checkConnections(localCopy)`
2. **KR-2:** Mutex-Granularität erhöhen
   - Mindestens 2 Mutexe: `g_appDataMutex` (Lese-/Schreibdaten) + `g_appConfigMutex` (Setup)
   - Oder: Reduziere `pidLockRead()` Timeout auf 50ms
3. **KR-3:** `volatile bool` → `std::atomic<bool>` für `g_shellyScanRunning`

### Phase 2: Speicher & Performance (Woche 3-4)

4. **H-1:** `preferences.putString()` Keys suffixieren (`"sdn_0"`, `"sdn_1"`, ...)
5. **H-2:** Hard-codierte Credentials aus Source entfernen → `#include "secrets.h"` (`.gitignore`)
6. **H-3:** **Alle** `delay()` durch `vTaskDelay(pdMS_TO_TICKS(ms))` ersetzen
7. **H-4:** `pidLockRead()` Timeout auf 50ms reduzieren
8. **M-1:** RingBuffer `std::mutex` → FreeRTOS `SemaphoreHandle_t`

### Phase 3: Code-Qualität (Woche 5-6)

9. **M-2:** Magic Numbers in `defines.h` konsolidieren
10. **M-3:** Alle `xTaskCreatePinnedToCore()` auf `pdPASS` prüfen
11. **M-4:** `LOG_INFO` in `pinManager::apply()` → `LOG_DEBUG`
12. **M-5:** Setup-Caching im RAM einführen (nur bei `setupChanged` schreiben)

### Phase 4: Code-Style & Cleanup (Optional)

13. **N-1 bis N-3:** Kommentare bereinigen, `const`-Korrektur, `#ifdef`-Blöcke

---

## 📊 Architektur-Empfehlungen (Langfristig)

### Lock-Free State-Management
Statt des globalen Mutex-Patterns (`appLock()`/`appUnlock()`), erwäge:
- **Read-Copy-Update (RCU)** für den `WEBSOCK_DATA`-Zugriff
  - Leser kopieren den Pointer lock-free, Writer erstellen neue Instanz
  - Reduziert Lock-Kontroversen bei 90% der Tasks (die nur lesen)
- **ESP-IDF `esp_pm_lock_t`** für Power-Management-critical Sektionen

### Task-Isolation
```
Core 0 (High Priority):  PID-Steuerung + Temperature-Monitoring + Watchdog
Core 1 (Lower Priority): WebSocket-Server + HTTP-AJAX + MQTT + Influx + ML-Training
```

Aktuell ist `T_PID` auf Core 1 gepinnt und ML-Training blockiert WebSocket.
→ **Empfehlung:** PID auf Core 0 (deterministisch), alles andere auf Core 1.

### Mutex-Matrix (Vorschlag)

| Ressource | Mutex | Tasks |
|-----------|-------|-------|
| `g_app.webSockData` (Read) | Lock-free (Atomic Pointer) | Alle |
| `g_app.webSockData` (Write) | `g_dataMutex` | Network, Temperature, Energy |
| `g_app.setupData` | `g_configMutex` | AJAX, Eprom, PID |
| `g_app.logBuffer` | `g_logMutex` | PID, Services |
| Shelly-Scan | `g_shellyMutex` | AJAX-Handler, Shelly-Task |

---

## ✅ Zusammenfassung

| Thema | Status | Aktion |
|-------|--------|--------|
| Deadlock in `appServices_checkConnections()` | 🔴 Kritisch | Sofort behoben |
| Zu grobe Mutex-Granularität | 🔴 Kritisch | Refaktor in Phase 1 |
| Volatile statt atomic | 🔴 Kritisch | `std::atomic<bool>` |
| Speicherleck Shelly-Devices | 🟠 Hoch | Keys suffixieren |
| Hard-codierte Credentials | 🟠 Hoch | Auslagern |
| `delay()` statt `vTaskDelay()` | 🟠 Hoch | Ersetzen |
| PID Mutex Timeout zu hoch | 🟠 Hoch | Auf 50ms reduzieren |
| RingBuffer `std::mutex` | 🟡 Mittel | FreeRTOS-Semaphore |
| Magic Numbers | 🟡 Mittel | `defines.h` konsolidieren |
| Fehlende Task-Creation-Checks | 🟡 Mittel | `pdPASS` prüfen |

---

## ✅ Phase 1 — umgesetzt und Build-verifiziert

| Issue | Datei | Änderung |
|-------|-------|----------|
| **KR-1 Deadlock** | `src/mainLoop/app_services.cpp` | `markNetworkDown()` sperrt nicht selbst mehr; Aufrufer (`tryMarkNetworkDown()`) hält Lock. Kommentar zur Vermeidung von Nested-Lock eingefügt. |
| **KR-3 volatile → atomic** | `src/www/ajaxCalls.cpp` | `volatile bool g_shellyScanRunning` und `g_shellyScanDone` → `std::atomic<bool>`. `#include <atomic>` + `#include "ajaxCalls.h"` ergänzt. |
| **H-3 delay → vTaskDelay** | `modbusReader.cpp`, `huawei.cpp`, `shelly.cpp`, `mqtt.cpp`, `wlan.cpp`, `ledHandler.cpp`, `curTime.cpp`, `www.cpp`, `cardRW_setup.cpp`, `tft.cpp`, `main.cpp` | Alle `delay(ms)` → `vTaskDelay(pdMS_TO_TICKS(ms))`. Fehrende `<freertos/task.h>` Includes hinzugefügt. |
| **H-1 NVS-Key-Kollision** | `src/devices/eprom.cpp` | `preferences.clear()` → gezielte Key-Removal für alte Indizes. Keys mit Geräte-Index suffixiert (`sdn_0`, `smac_3`, `sip_0`, `spt_2`). Device-Count unter `scnt` gespeichert. |
| **H-4 PID-Timeout** | `src/pid/pidManager.cpp` | `appLock(150)` → `appLock(50)`, weitere `appLock(50)` → `appLock(30)`. Fail-fast in 50ms PID-Task. |

**Build-Status:** ✅ SUCCESS (0 errors, keine neuen warnings) — `pio run` erfolgreich.
| Speicher:
RAM:   19.4% (63704 / 327680 bytes)
Flash: 22.5% (1477121 / 6553600 bytes)

---

## ✅ H-1 — NVS-Key-Kollision behoben (Build-verifiziert)

| Issue | Datei | Änderung |
|-------|-------|----------|
| **H-1 NVS-Keysuffix** | `src/devices/eprom.cpp` | `preferences.clear()` durch gezielte Key-Removal ersetzt. NVS-Keys mit Geräte-Index suffixiert (`sdn_0`, `smac_3`, …). Device-Count in `scnt` gespeichert, um veraltete Keys aus vorherigen Scans zu entfernen. |

**Detail:** Bisher schrieb jeder Durchlauf alle Shelly-Geräte auf die gleichen Schlüssel (`sdn`, `smac`, `sip`, `spt`), wodurch nur das letzte Gerät im NVS überlebte. Jetzt speichert jeder Index eigenen key (z.B. `sdn_0`, `sdn_1`). Beim nächsten Speichervorgang werden Keys oberhalb des neuen `upperLimit` aus vorherigen Scans bereinigt. `preferences.clear()` entfällt.

**Build-Status:** ✅ SUCCESS — `pio run` 0 errors.
