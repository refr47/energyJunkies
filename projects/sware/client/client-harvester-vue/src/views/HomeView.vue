<template>
  <div
    v-if="liveData"
    class="min-h-screen bg-[#f1f5f9] p-4 md:p-8 font-sans text-slate-900"
  >
    <header
      class="flex flex-col md:flex-row justify-between items-start md:items-center gap-4 mb-8 bg-white/50 backdrop-blur-md p-4 rounded-2xl border border-white/80 shadow-sm transition-all duration-500"
    >
      <div class="flex items-center gap-4">
        <div class="relative">
          <div
            v-if="hasErrors"
            class="absolute inset-0 bg-rose-500 blur-lg opacity-40 animate-pulse"
          ></div>

          <div
            :class="
              hasErrors
                ? 'from-rose-500 to-rose-700 shadow-rose-200'
                : 'from-blue-500 to-blue-700 shadow-blue-200'
            "
            class="relative bg-gradient-to-br p-2.5 rounded-xl text-white shadow-lg transition-all duration-500"
          >
            <svg
              xmlns="http://www.w3.org/2000/svg"
              class="h-6 w-6"
              fill="none"
              viewBox="0 0 24 24"
              stroke="currentColor"
            >
              <path
                v-if="!hasErrors"
                stroke-linecap="round"
                stroke-linejoin="round"
                stroke-width="2"
                d="M13 10V3L4 14h7v7l9-11h-7z"
              />
              <path
                v-else
                stroke-linecap="round"
                stroke-linejoin="round"
                stroke-width="2"
                d="M12 9v2m0 4h.01m-6.938 4h13.856c1.54 0 2.502-1.667 1.732-3L13.732 4c-.77-1.333-2.694-1.333-3.464 0L3.34 16c-.77 1.333.192 3 1.732 3z"
              />
            </svg>
          </div>
        </div>

        <div>
          <h1
            class="text-xl font-black tracking-tight text-slate-800 uppercase"
          >
            Power Hub
          </h1>
          <div class="flex items-center gap-2">
            <span
              class="flex h-2 w-2 rounded-full transition-colors"
              :class="[
                isConnected
                  ? hasErrors
                    ? 'bg-rose-500'
                    : 'bg-emerald-500'
                  : 'bg-slate-300 animate-pulse',
              ]"
            >
            </span>
            <p
              class="text-slate-400 text-[10px] font-bold tracking-[0.15em] uppercase"
            >
              {{
                isConnected
                  ? hasErrors
                    ? "System gestört"
                    : "System Operational"
                  : "Verbindung verloren"
              }}
            </p>
          </div>
        </div>
      </div>

      <div class="flex flex-wrap gap-2 justify-end w-full md:w-auto">
        <transition-group name="error-list">
          <div
            v-for="err in errorList"
            :key="err"
            class="bg-rose-600 text-white px-3 py-1.5 rounded-lg text-[9px] font-black uppercase tracking-tight flex items-center gap-2 shadow-md border border-rose-500"
          >
            <span class="w-1.5 h-1.5 bg-white rounded-full animate-ping"></span>
            {{ err }}
          </div>
        </transition-group>

        <div
          v-if="!hasErrors"
          :class="
            isConnected
              ? 'bg-emerald-50 text-emerald-600 border-emerald-100'
              : 'bg-slate-100 text-slate-400 border-slate-200'
          "
          class="px-3 py-1.5 rounded-lg text-[9px] font-black border flex items-center gap-2"
        >
          <span class="w-1.5 h-1.5 rounded-full bg-current"></span>
          {{ isConnected ? "ONLINE" : "OFFLINE" }}
        </div>
      </div>
    </header>

    <!-- Wetter & Uhrzeit für Mobile (in Navbar nur ab md: sichtbar) -->
    <div
      class="sm:hidden mb-6 p-4 bg-white rounded-2xl border border-slate-200 shadow-sm"
    >
      <div class="flex justify-between items-center">
        <div class="flex items-center gap-3">
          <div class="text-2xl">
            <span v-if="liveData.wetter === 'sonnig'">☀️</span>
            <span v-else-if="liveData.wetter === 'bewölkt'">⛅</span>
            <span v-else>🌧️</span>
          </div>
          <div>
            <div class="text-xs font-bold text-slate-400 uppercase">Wetter</div>
            <div class="text-lg font-bold text-slate-800">
              {{ liveData.temp || "--" }}°C
            </div>
          </div>
        </div>
        <div class="text-right">
          <div class="text-xs font-bold text-slate-400 uppercase">Aktuell</div>
          <div class="text-lg font-mono font-bold text-slate-700">
            {{ currentTime }}
          </div>
        </div>
      </div>
    </div>

    <!-- Weather Prediction and Early Heating Status (Desktop) -->
    <div class="md:col-span-12 bg-white rounded-[2.5rem] shadow-xl p-8 border border-white mt-6">
      <div class="flex justify-between items-center mb-6">
        <span class="text-[10px] font-black text-slate-400 uppercase tracking-widest">Heizungsstatus</span>
      </div>

      <div class="grid grid-cols-1 md:grid-cols-2 gap-6">
        <div class="bg-gradient-to-br from-emerald-50 to-green-50 rounded-2xl p-5 border border-emerald-100 shadow-sm">
          <div class="flex items-center gap-2 mb-3">
            <span class="text-lg">🌤️</span>
            <span class="text-[9px] font-black uppercase tracking-widest text-emerald-700">Wetter-Vorhersage</span>
          </div>
          <div class="flex items-center justify-between">
            <span class="font-bold text-emerald-600">Verwendung</span>
            <span :class="weatherPredictionUsed ? 'bg-emerald-500 text-white' : 'bg-slate-300 text-slate-700'"
              class="px-3 py-1 rounded-full text-xs font-bold uppercase">
              {{ weatherPredictionUsed ? 'Aktiv' : 'Inaktiv' }}
            </span>
          </div>
        </div>

        <div class="bg-gradient-to-br from-purple-50 to-violet-50 rounded-2xl p-5 border border-purple-100 shadow-sm">
          <div class="flex items-center gap-2 mb-3">
            <span class="text-lg">🔥</span>
            <span class="text-[9px] font-black uppercase tracking-widest text-purple-700">Frühheizung</span>
          </div>
          <div class="flex items-center justify-between">
            <span class="font-bold text-purple-600">Status</span>
            <span :class="earlyHeatingMode ? 'bg-emerald-500 text-white' : 'bg-slate-300 text-slate-700'"
              class="px-3 py-1 rounded-full text-xs font-bold uppercase">
              {{ earlyHeatingMode ? 'Aktiv' : 'Inaktiv' }}
            </span>
          </div>
        </div>
      </div>
    </div>

    <div
      v-if="liveData"
      class="sm:hidden mb-6 p-4 bg-white rounded-2xl border border-slate-200 shadow-sm"
    >
      <h3 class="text-sm font-bold text-slate-700 mb-3">Heizungsstatus</h3>
      <div class="flex justify-between gap-4">
        <div class="flex flex-col">
          <span class="text-xs text-slate-500 uppercase tracking-wider">Wettervorhersage</span>
          <span :class="{ 'text-emerald-600 font-bold': liveData.weatherPredictionUsed, 'text-slate-400': !liveData.weatherPredictionUsed }">
            {{ liveData.weatherPredictionUsed ? 'Aktiv' : 'Inaktiv' }}
          </span>
        </div>
        <div class="flex flex-col">
          <span class="text-xs text-slate-500 uppercase tracking-wider">Frühheizung</span>
          <span :class="{ 'text-emerald-600 font-bold': liveData.earlyHeatingMode, 'text-slate-400': !liveData.earlyHeatingMode }">
            {{ liveData.earlyHeatingMode ? 'Aktiv' : 'Inaktiv' }}
          </span>
        </div>
      </div>
    </div>

    <div v-if="errorList.length > 0" class="lg:hidden flex flex-col gap-2 mb-6">
      <div
        v-for="err in errorList"
        :key="err"
        class="bg-rose-500 text-white p-3 rounded-xl text-xs font-bold flex items-center gap-3"
      >
        <span>⚠️</span> {{ err }}
      </div>
    </div>

    <div class="grid grid-cols-1 md:grid-cols-12 gap-6">
      <!-- Live Energie-Bilanz (conditional nach dataSource) -->
      <div
        class="md:col-span-12 lg:col-span-8 bg-white rounded-[2.5rem] p-8 border border-white shadow-xl shadow-slate-200/60"
      >
        <div class="flex justify-between items-center mb-6">
          <span
            class="text-[10px] font-black text-slate-400 uppercase tracking-widest"
            >Live Energie-Bilanz</span
          >
          <DataSourceBadge :source="dataSource" />
        </div>

        <EnergyBalanceFronius v-if="isRichSource" :liveData="liveData" />
        <EnergyBalanceAmis v-else-if="isAmis" :liveData="liveData" />
        <EnergyBalanceFallback v-else />
      </div>

      <div v-if="isRichSource" class="md:col-span-12 lg:col-span-4">
        <EnergyBalanceBattery :liveData="liveData" />
      </div>

      <div
        class="md:col-span-12 bg-white rounded-[2.5rem] shadow-xl p-8 border border-white"
      >
        <div class="flex justify-between items-center mb-8">
          <span
            class="text-[10px] font-black text-slate-400 uppercase tracking-widest"
            >Energie-Vorhersage &amp; Zusammensetzung</span
          >
        </div>

        <div class="grid grid-cols-1 md:grid-cols-3 gap-6">
          <!-- Wetter-Forecast Karte (immer) -->
          <div
            class="bg-gradient-to-br from-amber-50 to-orange-50 rounded-2xl p-5 border border-amber-100 shadow-sm"
          >
            <div class="flex items-center gap-2 mb-3">
              <span class="text-lg">☀️</span>
              <span
                class="text-[9px] font-black uppercase tracking-widest text-amber-700"
                >Wetter-Einfluss</span
              >
            </div>
            <div class="space-y-2">
              <div class="flex justify-between text-xs">
                <span class="text-slate-500">Preheat-Bonus</span>
                <span
                  class="font-bold"
                  :class="
                    weatherBonus === 8
                      ? 'text-orange-600 bg-orange-50 px-1.5 py-0.5 rounded'
                      : 'text-amber-600'
                  "
                >
                  +{{ weatherBonus }}°C
                  <span v-if="weatherBonus === 8" class="text-xs"
                    >(fallback aktiv)</span
                  >
                </span>
              </div>
              <div class="flex justify-between text-xs">
                <span class="text-slate-500">PV-Verhältnis</span>
                <span class="font-bold text-amber-600">{{
                  weatherPvRatio
                }}</span>
              </div>
              <div class="flex justify-between text-xs">
                <span class="text-slate-500">Wolken-Bedeckung</span>
                <span class="font-bold text-amber-600"
                  >{{ weatherCloudPct }}%</span
                >
              </div>
              <div class="flex justify-between text-xs">
                <span class="text-slate-500">Außentemperatur</span>
                <span class="font-bold text-amber-600"
                  >{{ weatherOutTemp }}°C</span
                >
              </div>
            </div>
          </div>

          <!-- TinyNN Karte (nur wenn TinyNN-Daten im JSON) -->
          <div
            class="bg-gradient-to-br from-violet-50 to-purple-50 rounded-2xl p-5 border border-violet-100 shadow-sm"
            v-if="hasTinyNN"
          >
            <div class="flex items-center gap-2 mb-3">
              <span class="text-lg">🧠</span>
              <span
                class="text-[9px] font-black uppercase tracking-widest text-violet-700"
                >TinyNN-Prädiktion</span
              >
            </div>
            <div class="space-y-3">
              <div>
                <div class="flex justify-between text-xs mb-1">
                  <span class="text-slate-500">Preheat-Wahrscheinl.</span>
                  <span class="font-bold text-violet-600"
                    >{{ tinyNNPreheatPct }}%</span
                  >
                </div>
                <div class="h-3 bg-white/60 rounded-full p-0.5">
                  <div
                    class="h-full bg-gradient-to-r from-violet-400 to-purple-500 rounded-full transition-all duration-500"
                    :style="{ width: tinyNNPreheatPct + '%' }"
                  ></div>
                </div>
              </div>
              <div>
                <div class="flex justify-between text-xs mb-1">
                  <span class="text-slate-500">Energiebudget</span>
                  <span class="font-bold text-violet-600"
                    >{{ tinyNNBufferPct }}%</span
                  >
                </div>
                <div class="h-3 bg-white/60 rounded-full p-0.5">
                  <div
                    class="h-full bg-gradient-to-r from-teal-400 to-emerald-500 rounded-full transition-all duration-500"
                    :style="{ width: tinyNNBufferPct + '%' }"
                  ></div>
                </div>
              </div>
            </div>
          </div>

          <!-- Stacked-Bar: Heizer-Zusammensetzung (immer) -->
          <div
            class="bg-gradient-to-br from-blue-50 to-indigo-50 rounded-2xl p-5 border border-indigo-100 shadow-sm"
          >
            <div class="flex items-center gap-2 mb-3">
              <span class="text-lg">⚡</span>
              <span
                class="text-[9px] font-black uppercase tracking-widest text-indigo-700"
                >Heizstab-Zuschaltung</span
              >
            </div>
            <div class="space-y-2">
              <div>
                <div class="flex justify-between text-xs mb-1">
                  <span class="text-slate-500">Basis (Regelung)</span>
                  <span class="font-bold text-blue-600"
                    >{{ basePowerVal }}W</span
                  >
                </div>
              </div>
              <div>
                <div class="flex justify-between text-xs mb-1">
                  <span class="text-slate-500">Wetter-Bonus</span>
                  <span class="font-bold text-amber-600"
                    >+{{ weatherPowerBonus }}W</span
                  >
                </div>
              </div>
              <div v-if="hasTinyNN">
                <div class="flex justify-between text-xs mb-1">
                  <span class="text-slate-500">NN-Adjustment</span>
                  <span class="font-bold text-violet-600"
                    >{{ nnPowerAdjust }}W</span
                  >
                </div>
              </div>
              <div class="mt-3 border-t border-indigo-200 pt-2">
                <div class="flex justify-between text-xs">
                  <span class="font-black text-indigo-700 uppercase"
                    >Gesamt</span
                  >
                  <span class="font-black text-indigo-600"
                    >{{ totalPowerVal }}W</span
                  >
                </div>
                <div
                  class="h-3 bg-white/60 rounded-full mt-1 p-0.5 overflow-hidden"
                >
                  <div
                    class="h-full rounded-full flex transition-all duration-500"
                    :style="{ width: totalPowerPct + '%' }"
                  >
                    <div
                      class="bg-gradient-to-r from-blue-400 to-blue-500"
                      :style="{ width: basePowerPct + '%' }"
                    ></div>
                    <div
                      class="bg-gradient-to-r from-amber-400 to-orange-400"
                      :style="{ width: weatherBonusPct + '%' }"
                    ></div>
                    <div
                      v-if="hasTinyNN"
                      class="bg-gradient-to-r from-violet-400 to-purple-400"
                      :style="{ width: nnAdjustPct + '%' }"
                    ></div>
                  </div>
                </div>
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>

    <div
      class="md:col-span-12 bg-white rounded-[2.5rem] shadow-xl p-8 border border-white"
    >
      <div class="flex flex-col lg:flex-row items-center gap-12">
        <div class="relative flex-shrink-0 group">
          <div
            class="absolute inset-0 bg-blue-400 blur-3xl opacity-5 rounded-full group-hover:opacity-10 transition-opacity"
          ></div>

          <svg class="w-48 h-48 transform -rotate-90 relative z-10">
            <circle
              cx="96"
              cy="96"
              r="84"
              stroke="currentColor"
              stroke-width="10"
              fill="transparent"
              class="text-slate-100"
            />
            <circle
              cx="96"
              cy="96"
              r="84"
              stroke="currentColor"
              stroke-width="14"
              fill="transparent"
              :stroke-dasharray="527"
              :stroke-dashoffset="527 - (527 * liveData.bTemp) / 80"
              class="text-blue-500 transition-all duration-1000 ease-out"
              stroke-linecap="round"
            />
          </svg>

          <div
            class="absolute inset-0 flex flex-col items-center justify-center relative z-20"
          >
            <span class="text-5xl font-black text-slate-800 tracking-tighter"
              >{{ liveData.bTemp }}<small class="text-xl ml-0.5">°</small></span
            >
            <span
              class="text-[10px] font-black text-slate-400 uppercase tracking-widest mt-1"
              >Wassertemperatur</span
            >
          </div>
        </div>

        <div class="flex-1 w-full">
          <div class="flex justify-between items-center mb-8">
            <div>
              <h3
                class="font-black text-slate-800 uppercase tracking-widest text-sm"
              >
                Boiler Management
              </h3>
              <p
                class="text-[10px] text-slate-400 font-bold uppercase mt-1 tracking-wider"
              >
                Phasen-Status & Modulation
              </p>
            </div>
            <div
              :class="chargeMode.class"
              class="px-5 py-1.5 rounded-full text-[10px] font-black uppercase shadow-inner tracking-widest"
            >
              {{ chargeMode.label }}
            </div>
          </div>

          <!-- Heizstab-Phasen + PWM als 3er-Grid -->
          <div class="grid grid-cols-3 gap-4 items-end">
            <!-- ── Heizstab L1 & L2 (SVG Röhren) ── -->
            <div
              v-for="i in [1, 2]"
              :key="'relay-' + i"
              class="flex flex-col items-center"
            >
              <svg
                :id="'stab' + i"
                xmlns="http://www.w3.org/2000/svg"
                viewBox="0 0 60 180"
                class="w-[52px] h-40 drop-shadow-sm"
              >
                <!-- Röhre Außen -->
                <rect
                  x="10"
                  y="10"
                  width="40"
                  height="160"
                  rx="20"
                  ry="20"
                  fill="#f8fafc"
                  stroke="#cbd5e1"
                  stroke-width="2.5"
                />
                <!-- Füll-Wasser -->
                <clipPath :id="'clip' + i">
                  <rect x="12" y="12" width="36" height="156" rx="18" ry="18" />
                </clipPath>
                <g :clip-path="'url(#clip' + i + ')'">
                  <rect
                    :key="i + 'fill'"
                    x="12"
                    y="12"
                    :height="relayFill(i)"
                    :y="168 - relayFill(i)"
                    fill="#fbbf24"
                    class="transition-all duration-700"
                  />
                </g>
                <!-- Glühstab (Mitten-Heizelement) -->
                <rect
                  x="24"
                  :y="60"
                  :height="relayActive(i) ? 90 : 0"
                  rx="4"
                  :fill="relayActive(i) ? '#ef4444' : '#e2e8f0'"
                  class="transition-all duration-700"
                />
                <!-- Aktiv-Punkt -->
                <circle cx="30" cy="6">
                  <animate
                    attributeName="opacity"
                    :values="relayActive(i) ? '1;0.5;1' : '0.2'"
                    :dur="relayActive(i) ? '1.5s' : '0s'"
                    repeatCount="indefinite"
                  />
                </circle>
              </svg>
              <span
                class="text-[9px] font-black uppercase tracking-widest mt-2 text-slate-400"
                >L{{ i }}</span
              >
              <span class="text-xs font-black text-slate-700"
                >{{ relayActive(i) ? relayWatt : 0 }}W</span
              >
            </div>

            <!-- ── PWM L3 als Batterie ── -->
            <div class="flex flex-col items-center">
              <svg
                id="pwmBat"
                xmlns="http://www.w3.org/2000/svg"
                viewBox="0 0 60 180"
                class="w-[52px] h-40 drop-shadow-sm"
              >
                <!-- Pluspol -->
                <rect
                  x="22"
                  y="2"
                  width="16"
                  height="10"
                  rx="3"
                  fill="#94a3b8"
                />
                <!-- Batterie Rahmen -->
                <rect
                  x="6"
                  y="14"
                  width="48"
                  height="156"
                  rx="10"
                  ry="10"
                  fill="#f8fafc"
                  stroke="#cbd5e1"
                  stroke-width="2.5"
                />
                <!-- Füll-Ebene -->
                <clipPath id="batClip">
                  <rect x="8" y="16" width="44" height="152" rx="8" ry="8" />
                </clipPath>
                <g clip-path="url(#batClip)">
                  <rect
                    id="batFill"
                    x="8"
                    :y="168 - batFillPx"
                    width="44"
                    :height="batFillPx"
                    :fill="batColor"
                    class="transition-all duration-700"
                  />
                </g>
                <!-- Segmente (Striche) -->
                <line
                  x1="8"
                  y1="36"
                  x2="52"
                  y2="36"
                  stroke="#e2e8f0"
                  stroke-width="1"
                />
                <line
                  x1="8"
                  y1="58"
                  x2="52"
                  y2="58"
                  stroke="#e2e8f0"
                  stroke-width="1"
                />
                <line
                  x1="8"
                  y1="80"
                  x2="52"
                  y2="80"
                  stroke="#e2e8f0"
                  stroke-width="1"
                />
                <line
                  x1="8"
                  y1="102"
                  x2="52"
                  y2="102"
                  stroke="#e2e8f0"
                  stroke-width="1"
                />
                <line
                  x1="8"
                  y1="124"
                  x2="52"
                  y2="124"
                  stroke="#e2e8f0"
                  stroke-width="1"
                />
                <line
                  x1="8"
                  y1="146"
                  x2="52"
                  y2="146"
                  stroke="#e2e8f0"
                  stroke-width="1"
                />
              </svg>
              <span
                class="text-[9px] font-black uppercase tracking-widest mt-2 text-slate-400"
                >L3</span
              >
              <span class="text-xs font-black text-slate-700"
                >{{ pwmPct }}%</span
              >
            </div>
          </div>

          <div class="flex gap-3 mt-6">
            <button
              @click="toggleForce()"
              :class="
                forceHeizung
                  ? 'bg-rose-500 hover:bg-rose-600 text-white ring-4 ring-rose-200'
                  : 'bg-slate-100 hover:bg-slate-200 text-slate-500 ring-2 ring-slate-200'
              "
              class="w-full py-2.5 rounded-xl text-xs font-black uppercase tracking-widest shadow-sm transition-all duration-300 cursor-pointer"
            >
              {{ forceHeizung ? "⚡ FORCE AN" : "○ FORCE AUS" }}
            </button>
            <button
              @click="toggleStop()"
              :class="
                forceHeizungOff
                  ? 'bg-amber-600 hover:bg-amber-700 text-white ring-4 ring-amber-200'
                  : 'bg-slate-100 hover:bg-slate-200 text-slate-500 ring-2 ring-slate-200'
              "
              class="w-full py-2.5 rounded-xl text-xs font-black uppercase tracking-widest shadow-sm transition-all duration-300 cursor-pointer"
            >
              {{ forceHeizungOff ? "🛑 STOP AN" : "○ STOP AUS" }}
            </button>
          </div>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { computed, onMounted, ref, watch } from "vue";
import DataSourceBadge from "../components/DataSourceBadge.vue";
import EnergyBalanceFronius from "../components/EnergyBalanceFronius.vue";
import EnergyBalanceAmis from "../components/EnergyBalanceAmis.vue";
import EnergyBalanceFallback from "../components/EnergyBalanceFallback.vue";
import EnergyBalanceBattery from "../components/EnergyBalanceBattery.vue";

const props = defineProps(["liveData", "logs", "isConnected"]);

// Force-Heizung: lokaler Ref, sync mit liveData vom Server
const forceHeizung = ref(false);
watch(
  () => props.liveData?.forceHeizung,
  (val) => {
    forceHeizung.value = !!val;
  },
  { immediate: true },
);

const toggleForce = async () => {
  forceHeizung.value = !forceHeizung.value;
  try {
    await fetch("/storeSetup", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ heizstab_force: forceHeizung.value ? 1 : 0 }),
    });
  } catch (e) {
    console.error("Force-Toggle fehlgeschlagen:", e);
    forceHeizung.value = !forceHeizung.value; // revert
  }
};

// Force-Stop (Heater Kill): lokaler Ref, sync mit liveData vom Server
const forceHeizungOff = ref(false);
watch(
  () => props.liveData?.forceHeizungOff,
  (val) => {
    forceHeizungOff.value = !!val;
  },
  { immediate: true },
);

const toggleStop = async () => {
  forceHeizungOff.value = !forceHeizungOff.value;
  try {
    await fetch("/storeSetup", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        heizstab_force_off: forceHeizungOff.value ? 1 : 0,
      }),
    });
  } catch (e) {
    console.error("Stop-Toggle fehlgeschlagen:", e);
    forceHeizungOff.value = !forceHeizungOff.value; // revert
  }
};
// Mobile: Live-Uhrzeitnst currentTime = ref('');let timeTimer = null;nst updateTime = () => {  currentTime.value = new Date().toLocaleTimeString('de-DE', { hour: '2-digit', minute: '2-digit', second: '2-digit' });};onMounted(() => {  updateTime();  timeTimer = setInterval(updateTime, 1000);});

// 1. Definiere die Fehlertexte
const errorDefinitions = {
  1: "MicroCard",
  2: "Flash Speicher",
  4: "Modbus/Wechselrichter",
  8: "Temperatur Sensorik",
  16: "Boiler Heizung nicht aktiv",
  32: "Amis Reader",
  64: "MQTT Schnittstelle",
  128: "InfluxDB Schnittstelle",
  256: "Watt Bias AKTIV !! ",
  512: "Manuelle Heizung",
};



// 2. Computed Properties mit "Optional Chaining" (?.) absichern
const isExporting = computed(() => (props.liveData?.netzBezug || 0) <= 0);
// --- Datenquelle aus WebSocket ---
const dataSource = computed(() => props.liveData?.dataSource ?? null);
const isRichSource = computed(() =>
  ["fronius", "modbus", "huawei"].includes(dataSource.value),
);
const isAmis = computed(() => dataSource.value === "amis");

// --- Battery (nur bei fronius/modbus relevant) ---
const hasBattery = computed(() => !!props.liveData?.aakHasBattery); // !! erzwingt Boolean

// 3. Fehlerliste berechnen
const errorList = computed(() => {
  // Sicherstellen, dass liveData und liveData.errors existieren
  const errorCode = props.liveData?.errors;

  if (!errorCode || isNaN(parseInt(errorCode))) return [];

  const bitvector = parseInt(errorCode);

  return Object.keys(errorDefinitions)
    .filter((bitKey) => {
      const bitValue = parseInt(bitKey);
      return (bitvector & bitValue) === bitValue;
    })
    .map((bitKey) => errorDefinitions[bitKey]);
});

const hasErrors = computed(() => errorList.value.length > 0);

// Lademodus Logik
const chargeMode = computed(() => {
  // STOP hat höchste Priorität, überschreibt alles
  if (props.liveData?.forceHeizungOff)
    return {
      label: "STOP KILL",
      class: "bg-amber-600 text-white animate-pulse",
    };
  const mode = props.liveData?.forceHeizung || 0;
  if (mode === 2)
    return {
      label: "FORCE LOAD",
      class: "bg-rose-500 text-white animate-pulse",
    };
  if (mode === 1)
    return { label: "SOLAR ONLY", class: "bg-emerald-500 text-white" };
  return { label: "STANDBY", class: "bg-slate-200 text-slate-500" };
});

// ── Wetter-Forecast ──
const weatherBonus = computed(() => props.liveData?.weatherBonus ?? 0);
const weatherPvRatio = computed(() => props.liveData?.weatherPvRatio ?? 1.0);
const weatherCloudPct = computed(() =>
  props.liveData?.weatherCloudAvg
    ? Math.round(props.liveData.weatherCloudAvg * 100)
    : 50,
);
const weatherOutTemp = computed(() => props.liveData?.weatherOutTemp ?? 20.0);

// ── Heizstab-Logik ──
const relayWatt = computed(() => Number(props.liveData?.hsPhase) || 0);
const relay1 = computed(() => Boolean(props.liveData?.HEIZPATRONE_L1));
const relay2 = computed(() => Boolean(props.liveData?.HEIZPATRONE_L2));
const relayActive = (i) => (i === 1 ? relay1.value : relay2.value);
const relayFill = (i) => (relayActive(i) ? 140 : 12);

// ── PWM-Batterie ──
const pwmPct = computed(() =>
  props.liveData?.L3 != null ? Math.round((props.liveData.L3 / 255) * 100) : 0,
);
const batFillPx = computed(() => 152 * (pwmPct.value / 100));
const batColor = computed(() => {
  if (pwmPct.value <= 20) return "#ef4444"; // rot
  if (pwmPct.value <= 50) return "#f97316"; // orange
  if (pwmPct.value <= 80) return "#eab308"; // gelb
  return "#22c55e"; // grün
});

// ── TinyNN (optional – nur wenn Server-TinyNN aktiv) ──
const hasTinyNN = computed(() => "tinyNN_preheat" in (props.liveData || {}));
const tinyNNPreheatPct = computed(() =>
  hasTinyNN.value ? Math.round(props.liveData.tinyNN_preheat * 100) : 0,
);
const tinyNNBufferPct = computed(() =>
  hasTinyNN.value ? Math.round(props.liveData.tinyNN_buffer * 100) : 0,
);

// ── Stacked-Bar: Heizer-Leistung-Zusammensetzung (aus PWM-Ausgang abgeleitet) ──
const maxHeaterWatt = computed(() =>
  Math.round((props.liveData?.hsPhase || 0) * 3),
); // 3 Phasen × 576W
const totalPwmVal = computed(() =>
  props.liveData?.L3 != null
    ? Math.round((props.liveData.L3 / 255) * maxHeaterWatt.value)
    : 0,
);
const basePowerVal = computed(() =>
  Math.max(
    0,
    totalPwmVal.value - weatherPowerBonus.value - nnPowerAdjust.value,
  ),
);
const weatherPowerBonus = computed(() =>
  Math.max(0, Math.round(weatherBonus.value * 8)),
); // °C → ~W-Äquivalent
const nnPowerAdjust = computed(() =>
  hasTinyNN.value ? Math.round((props.liveData.tinyNN_preheat - 0.5) * 80) : 0,
);
const totalPowerVal = computed(
  () => basePowerVal.value + weatherPowerBonus.value + nnPowerAdjust.value,
);

// Segment-Prozente der Stacked-Bar
const totalPowerPct = computed(() => (totalPowerVal.value > 0 ? 100 : 0));
const basePowerPct = computed(() =>
  totalPowerVal.value > 0
    ? Math.max(5, (basePowerVal.value / totalPowerVal.value) * 100)
    : 0,
);
const weatherBonusPct = computed(() =>
  totalPowerVal.value > 0
    ? Math.max(0, (weatherPowerBonus.value / totalPowerVal.value) * 100)
    : 0,
);
const nnAdjustPct = computed(() =>
  totalPowerVal.value > 0
    ? Math.max(0, (nnPowerAdjust.value / totalPowerVal.value) * 100)
    : 0,
);

// Weather prediction and early heating mode status
const weatherPredictionUsed = computed(() => {
  return props.liveData?.weatherPredictionUsed || 0;
});

const earlyHeatingMode = computed(() => {
  return props.liveData?.earlyHeatingMode || 0;
});

onMounted(() => {
  console.log("Props Check:");
  console.log("liveData vorhanden:", !!props.liveData);
  console.log("logs vorhanden:", !!props.logs);
  console.log("isConnected Status:", props.isConnected);

  if (props.liveData === undefined) {
    console.error("KRITISCH: liveData wurde nicht an HomeView übergeben!");
  }
});
</script>

<style scoped>
.animation-delay-100 {
  animation-delay: 0.1s;
}

.animation-delay-200 {
  animation-delay: 0.2s;
}

.list-enter-active,
.list-leave-active {
  transition: all 0.4s ease;
}

.list-enter-from,
.list-leave-to {
  opacity: 0;
  transform: translateX(30px);
}

.animate-spin-slow {
  animation: spin 8s linear infinite;
}

@keyframes spin {
  from {
    transform: rotate(0deg);
  }

  to {
    transform: rotate(360deg);
  }
}

/* Animation für die Fehler-Badges */
.error-fade-enter-active,
.error-fade-leave-active {
  transition: all 0.4s cubic-bezier(0.4, 0, 0.2, 1);
}

.error-fade-enter-from,
.error-fade-leave-to {
  opacity: 0;
  transform: translateX(20px) scale(0.95);
}

/* Optional: Stärkere Betonung der Gauge bei Fehlern */
.error-pulse {
  animation: error-glow 2s infinite;
}

@keyframes error-glow {
  0% {
    box-shadow: 0 0 0 0 rgba(225, 29, 72, 0.4);
  }

  70% {
    box-shadow: 0 0 0 15px rgba(225, 29, 72, 0);
  }

  100% {
    box-shadow: 0 0 0 0 rgba(225, 29, 72, 0);
  }
}

.error-list-move,
	/* Animation bei Positionsänderung */
	.error-list-enter-active,
	.error-list-leave-active {
  transition: all 0.5s ease;
}

/* Verhindert, dass die Liste während der Animation andere Elemente überlagert */
.error-list-leave-active {
  position: absolute;
}
</style>
