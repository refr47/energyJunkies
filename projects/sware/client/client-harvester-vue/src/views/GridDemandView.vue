<template>
  <div class="max-w-5xl mx-auto p-4 md:p-8 space-y-6">
    <header><h1 class="text-3xl font-bold">Viertelstundenbezug</h1><p class="text-slate-600">Messung und Heizleistungsbegrenzung</p></header>
    <DemandSettings v-model="values" />
    <button type="button" :disabled="saving || loading" @click="save" class="bg-indigo-600 text-white rounded-xl px-5 py-3 font-semibold disabled:opacity-50">Einstellungen speichern</button>
    <p role="status" class="text-sm text-slate-700">{{ message }}</p>
    <ul v-if="errors.length" role="alert" class="text-rose-700 list-disc pl-5"><li v-for="error in errors" :key="error.field">{{ error.field }}: {{ error.message }}</li></ul>
    <DemandLiveStatus :demand="liveData?.gridDemand" :connected="isConnected" />
    <router-link to="/netzbezug/verlauf" class="inline-block text-indigo-700 font-semibold underline">Lokalen Viertelstundenverlauf ansehen</router-link>
  </div>
</template>

<script setup>
import { onMounted } from 'vue';
import DemandSettings from '../components/demand/DemandSettings.vue';
import DemandLiveStatus from '../components/demand/DemandLiveStatus.vue';
import { useSetupPatch } from '../composables/useSetupPatch';
defineProps({ liveData: Object, isConnected: Boolean });
const { values, loading, saving, message, errors, load, save } = useSetupPatch({
  grid_demand_enabled: false, grid_demand_enforce_limit: false, grid_demand_limit_w: 10000
});
onMounted(load);
</script>
