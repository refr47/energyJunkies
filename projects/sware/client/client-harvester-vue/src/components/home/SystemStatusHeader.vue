<template>
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
</template>

<script setup>
import { computed } from "vue";
const props = defineProps({
  isConnected: Boolean,
  errorList: { type: Array, default: () => [] },
});
const hasErrors = computed(() => props.errorList.length > 0);
</script>

<style scoped>
.error-list-move,
.error-list-enter-active,
.error-list-leave-active { transition: all 0.5s ease; }
.error-list-leave-active { position: absolute; }
</style>
