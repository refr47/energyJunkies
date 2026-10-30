<template>
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
</template>

<script setup>
import { computed } from "vue";
const props = defineProps({ liveData: { type: Object, required: true } });
// ── Wetter-Forecast ──
const weatherBonus = computed(() => props.liveData?.weatherBonus ?? 0);
const weatherPvRatio = computed(() => props.liveData?.weatherPvRatio ?? 1.0);
const weatherCloudPct = computed(() =>
  props.liveData?.weatherCloudAvg
    ? Math.round(props.liveData.weatherCloudAvg * 100)
    : 50,
);
const weatherOutTemp = computed(() => props.liveData?.weatherOutTemp ?? 20.0);

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
</script>
