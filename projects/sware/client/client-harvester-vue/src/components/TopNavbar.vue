<template>
  <nav class="sticky top-0 z-50 overflow-hidden border-b border-white/10 shadow-2xl">
    <div class="absolute inset-0 -z-10 bg-zinc-800">
      <div class="absolute inset-0 opacity-60" style="background: 
            radial-gradient(at 0% 0%, #10b981 0px, transparent 70%), 
            radial-gradient(at 100% 0%, #3b82f6 0px, transparent 70%),
            radial-gradient(at 50% 120%, #34d399 0px, transparent 80%);">
      </div>

      <div class="absolute inset-0 opacity-[0.05]"
        style="background-image: linear-gradient(#fff 1px, transparent 1px), linear-gradient(90deg, #fff 1px, transparent 1px); background-size: 40px 40px;">
      </div>

      <div class="absolute inset-0 backdrop-blur-3xl bg-zinc-900/40"></div>
    </div>

    <div class="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
      <div class="flex justify-between h-24 items-center">

        <div class="flex items-center gap-3">
          <div
            class="w-10 h-10 bg-emerald-500 rounded-xl flex items-center justify-center shadow-lg shadow-emerald-500/30">
            <span class="text-white font-black text-2xl italic">E</span>
          </div>
          <div class="flex flex-col">
            <span class="font-bold text-white tracking-tight text-lg leading-none">Harvester</span>
            <span class="text-emerald-400 text-[10px] font-black tracking-[0.3em] uppercase">v3.0 System</span>
          </div>
        </div>

        <div class="flex items-center">

          <div
            class="hidden md:flex items-center gap-4 px-4 py-2 bg-zinc-900 border border-white/5 rounded-2xl mr-6 backdrop-blur-sm shadow-inner">
            <div class="flex items-center justify-center">
              <svg v-if="weather.status === 'sunny'"
                class="w-6 h-6 text-amber-400 drop-shadow-[0_0_8px_rgba(251,191,36,0.5)]" fill="none"
                stroke="currentColor" stroke-width="2" viewBox="0 0 24 24">
                <circle cx="12" cy="12" r="4" />
                <path
                  d="M12 2v2m0 16v2m10-10h-2M4 12H2m15.07-7.07l-1.41 1.41M6.34 17.66l-1.41 1.41M17.66 17.66l1.41 1.41M6.34 6.34l-1.41 1.41" />
              </svg>
              <svg v-else-if="weather.status === 'cloudy'"
                class="w-6 h-6 text-slate-300 drop-shadow-[0_0_8px_rgba(255,255,255,0.2)]" fill="none"
                stroke="currentColor" stroke-width="2" viewBox="0 0 24 24">
                <path
                  d="M17.5 19c2.5 0 4.5-2 4.5-4.5 0-2.4-1.9-4.3-4.3-4.5-.4-2.5-2.6-4.5-5.2-4.5-2.2 0-4.1 1.4-4.8 3.3-2.1.2-3.7 2-3.7 4.2C4 17 5.8 19 8 19h9.5z" />
              </svg>
              <svg v-else class="w-6 h-6 text-blue-400 drop-shadow-[0_0_8px_rgba(96,165,250,0.4)]" fill="none"
                stroke="currentColor" stroke-width="2" viewBox="0 0 24 24">
                <path
                  d="M16 13v8m-4-7v8m-4-5v8m13-11.5c0-2.5-2-4.5-4.5-4.5-2.6 0-4.8 2-5.2 4.5-2.1.2-3.7 2-3.7 4.2 0 2.3 1.8 4.3 4 4.3h9.5c2.5 0 4.5-2 4.5-4.5 0-2.4-1.9-4.3-4.3-4.5z" />
              </svg>
            </div>
            <div class="flex flex-col border-l border-white/10 pl-3">
              <span class="text-[9px] font-black text-emerald-400/60 uppercase tracking-widest leading-none mb-1">{{
                weather.label }}</span>
              <span class="text-lg font-bold text-white leading-none tabular-nums">{{ weather.temp }}<span
                  class="text-emerald-400 text-sm ml-0.5">°C</span></span>
            </div>
          </div>

          <div class="hidden md:flex flex-col items-end mr-8">
            <span class="text-[9px] font-black text-emerald-400 uppercase tracking-widest">System Time</span>
            <span class="font-mono text-xl font-bold text-slate-100 tabular-nums leading-none">
              {{ currentTime }}
            </span>
          </div>

          <div
            class="hidden sm:flex sm:items-center sm:gap-6 bg-zinc-900 border border-white/10 p-2 rounded-2xl shadow-inner">
            <div class="relative flex items-center gap-2 px-4 py-3 border border-white/5 rounded-xl bg-zinc-900">
              <span
                class="absolute -top-2 left-3 px-1.5 bg-slate-900 text-[8px] font-white text-emerald-400 uppercase tracking-[0.2em] border border-white/10 rounded-sm">
                Log Management
              </span>
              <button @click="$emit('download')"
                class="text-[10px] font-bold text-emerald-400 hover:bg-emerald-400/10 px-3 py-1.5 rounded-lg border border-emerald-400/30 transition-all active:scale-95">
                Exportieren
              </button>
              <button @click="$emit('clear')"
                class="text-[10px] font-bold text-rose-400 hover:bg-rose-400/10 px-3 py-1.5 rounded-lg border border-rose-400/30 transition-all active:scale-95">
                Löschen
              </button>
            </div>

            <div class="flex items-center gap-1 pr-2">
              <router-link to="/" class="nav-btn" active-class="nav-btn-active">Dashboard</router-link>
              <router-link to="/setup" class="nav-btn" active-class="nav-btn-active">Setup</router-link>
              <router-link to="/logs" class="nav-btn" active-class="nav-btn-active">Logs</router-link>
            </div>
          </div>
        </div>

        <div class="sm:hidden">
          <button @click="isMobileMenuOpen = !isMobileMenuOpen" class="text-slate-300 p-2 hover:bg-white/10 rounded-lg">
            <svg class="w-6 h-6" fill="none" stroke="currentColor" viewBox="0 0 24 24">
              <path v-if="!isMobileMenuOpen" stroke-linecap="round" stroke-linejoin="round" stroke-width="2"
                d="M4 6h16M4 12h16m-7 6h7" />
              <path v-else stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M6 18L18 6M6 6l12 12" />
            </svg>
          </button>
        </div>
      </div>
    </div>
  </nav>
</template>




<script setup>
import { ref, onMounted, onUnmounted } from 'vue';
const currentTime = ref('');
let timer = null;
const isMobileMenuOpen = ref(false);

//defineProps(['connected', 'currentTime']);
const props = defineProps({
  connected: {
    type: Boolean,
    default: false
  },
  currentTime: {
    type: String,
    required: true
  },
  // Mit Standardwert (Default), falls nichts übergeben wird
  weather: {
    type: Object,
    validator(value) {
      // Prüft, ob der Status einer der drei erlaubten Werte ist
      const validStatuses = ['sunny', 'cloudy', 'rainy'];
      const hasValidStatus = validStatuses.includes(value.status);

      // Prüft, ob die Temperatur eine Zahl ist
      const hasValidTemp = typeof value.temp === 'number';

      return hasValidStatus && hasValidTemp;
    }
  },
  maxLogs: {
    type: Number,
    default: 1000
  }
})

// Events definieren
defineEmits(['clear', 'download']);


const updateTime = () => {
  const now = new Date();
  // Format: 12:34:56
  currentTime.value = now.toLocaleTimeString('de-DE', {
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit'
  });
};

onMounted(() => {
  updateTime(); // Sofortiger Aufruf
  timer = setInterval(updateTime, 1000);
});

onUnmounted(() => {
  if (timer) clearInterval(timer);
});

</script>

<style lang="postcss" scope>
/* Hier nutzen wir Tailwind @apply, um den Template-Code sauber zu halten */
@reference "../assets/css/main.css";

.nav-btn {
  @apply text-[11px] font-bold text-slate-400 px-4 py-2 rounded-xl transition-all hover:text-white hover:bg-white/5;
}

.nav-btn-active {
  @apply bg-emerald-500/10 text-emerald-400 border;
}

.nav-btn-active {
  @apply bg-emerald-500 text-white shadow-[0_0_20px_rgba(16,185,129,0.4)];
}

.nav-link {
  @apply px-4 py-2 text-sm font-medium text-slate-500 hover:text-slate-800 rounded-lg transition-all duration-200;
}

.active-nav {
  @apply bg-slate-100 text-emerald-600 shadow-inner font-bold;
}

.mobile-nav-link {
  @apply block px-3 py-4 text-base font-medium text-slate-600 hover:bg-slate-50 rounded-md;
}

.active-mobile {
  @apply bg-emerald-50 text-emerald-600 border-l-4;
}
.nav-btn-large {
  @apply text-sm font-bold text-slate-200 px-5 py-2.5 rounded-xl transition-all hover:text-white hover:bg-white/10;
}
/* Einfache Animation für das mobile Menü */
/* Slide Transition für Mobile */
.slide-enter-active,
.slide-leave-active {
  transition: all 0.3s ease-out;
}

.slide-enter-from,
.slide-leave-to {
  transform: translateY(-20px);
  opacity: 0;
}
</style>
