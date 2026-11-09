<template>
  <section class="bg-white p-6 rounded-3xl shadow-sm space-y-5" aria-labelledby="plan-settings">
    <h2 id="plan-settings" class="font-bold text-lg">Heizplan einstellen</h2>
    <label class="flex items-center gap-2 font-medium"><input type="checkbox" v-model="model.heating_plan_enabled" /> Tarifintervall aktivieren</label>
    <p class="text-sm text-slate-600">Ohne Heizplan wird unter der Zieltemperatur wie bisher sofort geheizt. Manuelles Heizen und Legionellenprogramm bleiben eigene Betriebsarten.</p>
    <fieldset :disabled="!model.heating_plan_enabled" class="space-y-4 disabled:opacity-50">
      <div class="grid sm:grid-cols-2 gap-4">
        <label class="block text-sm">Beginn <input type="time" v-model="startTime" class="block w-full mt-1 rounded-xl border border-slate-200 p-3" /></label>
        <label class="block text-sm">Ende <input type="time" v-model="endTime" class="block w-full mt-1 rounded-xl border border-slate-200 p-3" /></label>
      </div>
      <p class="text-sm text-slate-600">Ein Zeitfenster über Mitternacht ist möglich. Während des Fensters wird die nächtliche Energieabfragepause ausgesetzt.</p>
      <p class="text-sm text-slate-600">Das Verschieben bei hoher Hauslast nutzt die Viertelstundenüberwachung. Ohne deren Messwerte beginnt die geplante Heizung bei Bedarf direkt im Fenster.</p>
      <div class="grid sm:grid-cols-3 gap-4">
        <ConfigInput label="Zieltemperatur" v-model="model.heizstab_temp_min" type="number" min="1" max="90" unit="°C" />
        <ConfigInput label="Notfallgrenze" v-model="model.heating_plan_emergency_c" type="number" min="0" :max="model.heizstab_temp_min - 1" unit="°C" />
        <ConfigInput label="Boilerinhalt" v-model="model.heating_plan_boiler_liters" type="number" min="20" max="5000" unit="l" />
      </div>
      <p class="text-sm text-slate-600">Unter der Notfallgrenze wird auch außerhalb des Fensters Heizleistung angefordert. Ein aktives Viertelstundenlimit gilt immer; fehlen dafür Messwerte, bleibt die Heizung aus.</p>
    </fieldset>
  </section>
</template>

<script setup>
import { computed } from 'vue';
import ConfigInput from '../ConfigInput.vue';
const model = defineModel({ type: Object, required: true });
const timeText = minute => `${String(Math.floor(minute / 60)).padStart(2, '0')}:${String(minute % 60).padStart(2, '0')}`;
const parseTime = text => { const [hour, minute] = text.split(':').map(Number); return hour * 60 + minute; };
const startTime = computed({
  get: () => timeText(model.value.heating_plan_start_minute),
  set: text => { model.value.heating_plan_start_minute = parseTime(text); }
});
const endTime = computed({
  get: () => timeText(model.value.heating_plan_end_minute),
  set: text => { model.value.heating_plan_end_minute = parseTime(text); }
});
</script>
