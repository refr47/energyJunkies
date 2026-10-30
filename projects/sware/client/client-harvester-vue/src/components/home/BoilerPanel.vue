<template>
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

      <HeaterPhaseGauges :live-data="liveData" />

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
</template>

<script setup>
import { computed } from "vue";
import HeaterPhaseGauges from "./HeaterPhaseGauges.vue";
import { useHeaterToggle } from "../../composables/useHeaterToggle.js";
const props = defineProps({ liveData: { type: Object, required: true } });
const { enabled: forceHeizung, toggle: toggleForce } = useHeaterToggle(
  () => props.liveData.forceHeizung, "heizstab_force",
);
const { enabled: forceHeizungOff, toggle: toggleStop } = useHeaterToggle(
  () => props.liveData.forceHeizungOff, "heizstab_force_off",
);
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
</script>
