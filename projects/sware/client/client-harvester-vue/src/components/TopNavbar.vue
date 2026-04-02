<template>
  <nav class="sticky top-0 z-50 bg-white/80 backdrop-blur-md border-b border-slate-200"
    >
    <div class="absolute inset-0 -z-10" :style="{ backgroundImage: `url(${navBG})` }"></div>
    <div class="absolute inset-0 -z-10 bg-white/20"
      :style="{ backdropFilter: 'saturate(0.6) blur(8px) brightness(1.1)' }"></div>
    
    <div class="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
      <div class="flex justify-between h-16">

        <div class="flex items-center">
          <div class="flex-shrink-0 flex items-center gap-2">
            <div
              class="w-8 h-8 bg-emerald-500 rounded-lg flex items-center justify-center shadow-lg shadow-emerald-500/20">
              <span class="text-white font-black text-xl italic">E</span>
            </div>
            <span class="font-bold text-slate-800 tracking-tight hidden sm:block">
              Harvester <span class="text-emerald-500 text-xs">v3.0</span>
            </span>
          </div>
        </div>

        <div class="hidden sm:flex sm:items-center sm:gap-4">
          <router-link to="/" class="nav-link" active-class="active-nav">
            Dashboard
          </router-link>
          <router-link to="/setup" class="nav-link" active-class="active-nav">
            System-Setup
          </router-link>
        </div>

        <div class="flex items-center sm:hidden">
          <button @click="isMobileMenuOpen = !isMobileMenuOpen" class="text-slate-500 p-2">
            <svg class="w-6 h-6" fill="none" stroke="currentColor" viewBox="0 0 24 24">
              <path v-if="!isMobileMenuOpen" stroke-linecap="round" stroke-linejoin="round" stroke-width="2"
                d="M4 6h16M4 12h16m-7 6h7" />
              <path v-else stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M6 18L18 6M6 6l12 12" />
            </svg>
          </button>
        </div>
      </div>
    </div>

    <transition name="slide">
      <div v-if="isMobileMenuOpen" class="sm:hidden bg-white border-b border-slate-200">
        <div class="px-2 pt-2 pb-3 space-y-1">
          <router-link to="/" @click="isMobileMenuOpen = false" class="mobile-nav-link" active-class="active-mobile">
            Dashboard
          </router-link>
          <router-link to="/setup" @click="isMobileMenuOpen = false" class="mobile-nav-link"
            active-class="active-mobile">
            System-Setup
          </router-link>
        </div>
      </div>
    </transition>
  </nav>
</template>

<script setup>
import { ref } from 'vue';
import navBG from '../assets/headerBG.jpg';

const isMobileMenuOpen = ref(false);
</script>

<style scoped>
/* Hier nutzen wir Tailwind @apply, um den Template-Code sauber zu halten */
@reference "../assets/css/main.css";

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
  @apply bg-emerald-50 text-emerald-600 border-l-4 border-emerald-500;
}

/* Einfache Animation für das mobile Menü */
.slide-enter-active,
.slide-leave-active {
  transition: all 0.3s ease-out;
}

.slide-enter-from,
.slide-leave-to {
  opacity: 0;
  transform: translateY(-10px);
}
</style>
