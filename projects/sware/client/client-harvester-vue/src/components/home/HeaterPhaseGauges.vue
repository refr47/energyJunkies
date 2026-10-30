<template>
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
      <line v-for="y in [36, 58, 80, 102, 124, 146]" :key="y"
        x1="8" :y1="y" x2="52" :y2="y" stroke="#e2e8f0" stroke-width="1" />
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
</template>

<script setup>
import { computed } from "vue";
const props = defineProps({ liveData: { type: Object, required: true } });
// ── Heizstab-Logik ──
const relayWatt = computed(() => Number(props.liveData?.hsPhase) || 0);
const relay1 = computed(() => Number(props.liveData?.L1) === 1);
const relay2 = computed(() => Number(props.liveData?.L2) === 1);
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
</script>
