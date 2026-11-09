<template>
  <div class="max-w-5xl mx-auto p-4 md:p-8 space-y-6">
    <header><h1 class="text-3xl font-bold">Heizplan und Boiler</h1><p class="text-slate-600">Tarifzeiten und Mindesttemperatur planen</p></header>
    <HeatingPlanSettings v-model="values" />
    <button type="button" :disabled="saving || loading" @click="save" class="bg-emerald-600 text-white rounded-xl px-5 py-3 font-semibold disabled:opacity-50">Heizplan speichern</button>
    <p role="status" class="text-sm text-slate-700">{{ message }}</p>
    <ul v-if="errors.length" role="alert" class="text-rose-700 list-disc pl-5"><li v-for="error in errors" :key="error.field">{{ error.field }}: {{ error.message }}</li></ul>
    <HeatingPlanStatus :plan="liveData?.heatingPlan" :connected="isConnected" />
  </div>
</template>

<script setup>
import { onMounted } from 'vue';
import HeatingPlanSettings from '../components/heating/HeatingPlanSettings.vue';
import HeatingPlanStatus from '../components/heating/HeatingPlanStatus.vue';
import { useSetupPatch } from '../composables/useSetupPatch';
defineProps({ liveData: Object, isConnected: Boolean });
const { values, loading, saving, message, errors, load, save } = useSetupPatch({
  heating_plan_enabled: false, heating_plan_start_minute: 1260,
  heating_plan_end_minute: 360, heating_plan_emergency_c: 2,
  heating_plan_boiler_liters: 200, heizstab_temp_min: 5
});
onMounted(load);
</script>
