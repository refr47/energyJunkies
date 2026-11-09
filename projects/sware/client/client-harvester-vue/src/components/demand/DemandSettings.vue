<template>
  <section class="bg-white p-6 rounded-3xl shadow-sm space-y-4" aria-labelledby="demand-settings">
    <h2 id="demand-settings" class="text-lg font-bold">Viertelstundenbezug einstellen</h2>
    <label class="flex gap-2 items-center"><input type="checkbox" v-model="model.grid_demand_enabled" /> Viertelstundenwerte überwachen</label>
    <fieldset :disabled="!model.grid_demand_enabled" class="space-y-4 disabled:opacity-50">
      <label class="flex gap-2 items-center"><input type="checkbox" v-model="model.grid_demand_enforce_limit" /> Heizleistung begrenzen</label>
      <div class="max-w-xs"><ConfigInput label="Bezugslimit" v-model="model.grid_demand_limit_w" type="number" min="100" max="1000000" step="1" unit="W" /></div>
    </fieldset>
    <p class="text-sm text-slate-600">Das Limit gilt auch für manuelles Heizen, Hygiene- und Notfallheizung. Ohne gültigen Netzmesswert stoppt die begrenzte Heizung. Während eines aktiven Tarifintervalls oder bei Notfallheizung mit Limit haben Messungen Vorrang vor der nächtlichen Abfragepause.</p>
  </section>
</template>

<script setup>
import ConfigInput from '../ConfigInput.vue';
const model = defineModel({ type: Object, required: true });
</script>
