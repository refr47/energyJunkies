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

    <!-- Mobile Overlay Menu -->
    <transition name="mobile-menu">
      <div v-if="isMobileMenuOpen" class="sm:hidden">
        <!-- Backdrop -->
        <div class="fixed inset-0 bg-black/60 z-40" @click="isMobileMenuOpen = false"></div>

        <!-- Menu Panel -->
        <div class="fixed top-0 right-0 h-full w-3/4 max-w-xs bg-zinc-900 z-50 flex flex-col border-l border-white/10 shadow-2xl">
          <!-- Menu Header -->
          <div class="flex items-center gap-3 px-5 py-4 border-b border-white/10">
            <div class="w-8 h-8 bg-emerald-500 rounded-lg flex items-center justify-center">
              <span class="text-white font-black italic">E</span>
            </div>
            <span class="font-bold text-white text-sm">Harvester v3.0</span>
          </div>

          <!-- Scrollable Content -->
          <div class="flex-1 overflow-y-auto py-2">
            <!-- Wetter & Uhrzeit -->
            <div class="mx-4 mb-3 p-3 bg-zinc-800/80 rounded-xl border border-white/5">
              <div class="flex items-center gap-3">
                <div>
                  <svg v-if="weather.status === 'sunny'" class="w-6 h-6 text-amber-400" fill="none" stroke="currentColor" stroke-width="2" viewBox="0 0 24 24">
                    <circle cx="12" cy="12" r="4" />
                    <path d="M12 2v2m0 16v2m10-10h-2M4 12H2m15.07-7.07l-1.41 1.41M6.34 17.66l-1.41 1.41M17.66 17.66l1.41 1.41M6.34 6.34l-1.41 1.41" />
                  </svg>
                  <svg v-else-if="weather.status === 'cloudy'" class="w-6 h-6 text-slate-300" fill="none" stroke="currentColor" stroke-width="2" viewBox="0 0 24 24">
                    <path d="M17.5 19c2.5 0 4.5-2 4.5-4.5 0-2.4-1.9-4.3-4.3-4.5-.4-2.5-2.6-4.5-5.2-4.5-2.2 0-4.1 1.4-4.8 3.3-2.1.2-3.7 2-3.7 4.2C4 17 5.8 19 8 19h9.5z" />
                  </svg>
                  <svg v-else class="w-6 h-6 text-blue-400" fill="none" stroke="currentColor" stroke-width="2" viewBox="0 0 24 24">
                    <path d="M16 13v8m-4-7v8m-4-5v8m13-11.5c0-2.5-2-4.5-4.5-4.5-2.6 0-4.8 2-5.2 4.5-2.1.2-3.7 2-3.7 4.2 0 2.3 1.8 4.3 4 4.3h9.5c2.5 0 4.5-2 4.5-4.5 0-2.4-1.9-4.3-4.3-4.5z" />
                  </svg>
                </div>
                <div class="flex flex-col">
                  <span class="text-[9px] font-bold text-emerald-400/60 uppercase tracking-widest">{{ weather.label }}</span>
                  <span class="text-base font-bold text-white">{{ weather.temp }}°C</span>
                </div>
                <div class="ml-auto text-right">
                  <span class="text-[9px] font-bold text-emerald-400/60 uppercase tracking-widest">Uhrzeit</span>
                  <span class="block text-base font-mono font-bold text-white">{{ currentTime }}</span>
                </div>
              </div>
            </div>

            <!-- Navigation Links -->
            <nav class="px-2 space-y-1">
              <router-link to="/" @click="isMobileMenuOpen = false" class="flex items-center gap-3 px-4 py-3 rounded-xl text-white font-bold text-sm hover:bg-white/10 transition-colors" active-class="bg-emerald-500/20 text-emerald-400">
                <svg class="w-5 h-5" fill="none" stroke="currentColor" stroke-width="2" viewBox="0 0 24 24"><path d="M3 12l2-2m0 0l7-7 7 7M5 10v10a1 1 0 001 1h3m10-11l2 2m-2-2v10a1 1 0 01-1 1h-3m-6 0a1 1 0 001-1v-4a1 1 0 011-1h2a1 1 0 011 1v4a1 1 0 001 1m-6 0h6" /></svg>
                Dashboard
              </router-link>
              <router-link to="/setup" @click="isMobileMenuOpen = false" class="flex items-center gap-3 px-4 py-3 rounded-xl text-white font-bold text-sm hover:bg-white/10 transition-colors" active-class="bg-emerald-500/20 text-emerald-400">
                <svg class="w-5 h-5" fill="none" stroke="currentColor" stroke-width="2" viewBox="0 0 24 24"><path d="M10.325 4.317c.426-1.756 2.924-1.756 3.35 0a1.724 1.724 0 002.573 1.066c1.543-.94 3.31.826 2.37 2.37a1.724 1.724 0 001.065 2.572c1.756.426 1.756 2.924 0 3.35a1.724 1.724 0 00-1.066 2.573c.94 1.543-.826 3.31-2.37 2.37a1.724 1.724 0 00-2.572 1.065c-.426 1.756-2.924 1.756-3.35 0a1.724 1.724 0 00-2.573-1.066c-1.543.94-3.31-.826-2.37-2.37a1.724 1.724 0 00-1.065-2.572c-1.756-.426-1.756-2.924 0-3.35a1.724 1.724 0 001.066-2.573c-.94-1.543-.826-3.31 2.37-2.37a1.724 1.724 0 002.572-1.065z" /></svg>
                Setup
              </router-link>
              <router-link to="/logs" @click="isMobileMenuOpen = false" class="flex items-center gap-3 px-4 py-3 rounded-xl text-white font-bold text-sm hover:bg-white/10 transition-colors" active-class="bg-emerald-500/20 text-emerald-400">
                <svg class="w-5 h-5" fill="none" stroke="currentColor" stroke-width="2" viewBox="0 0 24 24"><path d="M9 5H7a2 2 0 00-2 2v12a2 2 0 002 2h10a2 2 0 002-2V7a2 2 0 00-2-2h-2M9 5a2 2 0 002 2h2a2 2 0 002-2M9 5a2 2 0 012-2h2a2 2 0 012 2" /></svg>
                Logs
              </router-link>
            </nav>

            <!-- Actions -->
            <div class="mx-4 mt-4 pt-4 border-t border-white/10 space-y-2">
              <button @click="$emit('download'); isMobileMenuOpen = false" class="w-full flex items-center gap-3 px-4 py-3 rounded-xl text-emerald-400 font-bold text-sm border border-emerald-400/30 hover:bg-emerald-400/10 transition-all active:scale-[0.98]">
                <svg class="w-5 h-5" fill="none" stroke="currentColor" stroke-width="2" viewBox="0 0 24 24"><path d="M12 10v6m0 0l-3-3m3 3l3-3m2 8H7a2 2 0 01-2-2V5a2 2 0 012-2h5.586a1 1 0 01.707.293l5.414 5.414a1 1 0 01.293.707V19a2 2 0 01-2 2z" /></svg>
                Daten exportieren
              </button>
              <button @click="$emit('clear'); isMobileMenuOpen = false" class="w-full flex items-center gap-3 px-4 py-3 rounded-xl text-rose-400 font-bold text-sm border border-rose-400/30 hover:bg-rose-400/10 transition-all active:scale-[0.98]">
                <svg class="w-5 h-5" fill="none" stroke="currentColor" stroke-width="2" viewBox="0 0 24 24"><path d="M19 7l-.867 12.142A2 2 0 0116.138 21H7.862a2 2 0 01-1.995-1.858L5 7m5 4v6m4-6v6m1-10V4a1 1 0 00-1-1h-4a1 1 0 00-1 1v3M4 7h16" /></svg>
                Logs löschen
              </button>
            </div>
          </div>
        </div>
      </div>
    </transition>
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

  // Menü schließen wenn Display auf Desktop wechselt
  window.addEventListener('resize', () => {
    if (window.innerWidth >= 640) {
      isMobileMenuOpen.value = false;
    }
  });
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
/* Animation für das mobile Menü */
/* Slide Transition für Mobile */
.mobile-menu-enter-active,
.mobile-menu-leave-active {
  transition: all 0.3s ease-out;
}

.mobile-menu-enter-from,
.mobile-menu-leave-to {
  opacity: 0;
}

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
