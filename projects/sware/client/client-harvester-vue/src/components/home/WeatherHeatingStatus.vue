<template>
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

<!-- Simple Weather and Heating Status (Desktop) -->
<div class="md:col-span-12 bg-white rounded-[2.5rem] shadow-xl p-6 border border-white mt-6">
  <div class="flex flex-wrap gap-4 justify-center">
    <div class="bg-gradient-to-br from-emerald-50 to-green-50 rounded-xl p-3 border border-emerald-100">
      <div class="text-xs text-slate-500 uppercase tracking-wider">Wettervorhersage</div>
      <div :class="{ 'font-bold text-emerald-600': weatherPredictionUsed, 'text-slate-400': !weatherPredictionUsed }">
        {{ weatherPredictionUsed ? 'Aktiv' : 'Inaktiv' }}
      </div>
    </div>

    <div class="bg-gradient-to-br from-purple-50 to-violet-50 rounded-xl p-3 border border-purple-100">
      <div class="text-xs text-slate-500 uppercase tracking-wider">Frühheizung</div>
      <div :class="{ 'font-bold text-purple-600': earlyHeatingMode, 'text-slate-400': !earlyHeatingMode }">
        {{ earlyHeatingMode ? 'Aktiv' : 'Inaktiv' }}
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
</template>

<script setup>
import { computed, onMounted, onUnmounted, ref } from "vue";
const props = defineProps({ liveData: { type: Object, required: true } });
const currentTime = ref("");
let timeTimer;
const updateTime = () => {
  currentTime.value = new Date().toLocaleTimeString("de-DE", {
    hour: "2-digit", minute: "2-digit", second: "2-digit",
  });
};
onMounted(() => {
  updateTime();
  timeTimer = setInterval(updateTime, 1000);
});
onUnmounted(() => clearInterval(timeTimer));
const weatherPredictionUsed = computed(() => props.liveData.weatherPredictionUsed || 0);
const earlyHeatingMode = computed(() => props.liveData.earlyHeatingMode || 0);
</script>
