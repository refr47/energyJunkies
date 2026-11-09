<template>
  <div v-if="plan && plan.status !== 'disabled'" class="mb-4 rounded-2xl bg-white border border-slate-200 p-4 flex flex-wrap items-center justify-between gap-2 text-sm">
    <p><strong>Heizplan:</strong> {{ status }}</p>
    <div class="flex gap-4"><router-link to="/heizplan" class="text-emerald-700 underline">Plan</router-link><router-link to="/netzbezug" class="text-indigo-700 underline">Bezugslimit</router-link></div>
  </div>
</template>

<script setup>
import { computed } from 'vue';
const props = defineProps({ plan: Object });
const labels = {
  outsideWindow: 'außerhalb des Tarifintervalls', atTarget: 'Zieltemperatur erreicht',
  waitingLoad: 'wartet wegen hoher Hauslast', heating: 'Heizen freigegeben',
  deadline: 'spätester Heizbeginn erreicht', emergency: 'Notfallheizung angefordert',
  blockedDemand: 'Heizen durch Bezugslimit blockiert', timeInvalid: 'Uhrzeit ungültig',
  sensorInvalid: 'Temperatursensor ungültig'
};
const status = computed(() => labels[props.plan?.status] || 'Status unbekannt');
</script>
