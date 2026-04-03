<template>
  <div id="backGr">
    <div class="header-image-box">
     
      <span class="headerFont">Energie Junkies</span>
    </div>
  </div>

  <header>
    <Navbar :connected="isConnected" />
   
  </header>

  <main>
    <router-view :liveData="liveData" :logs="logEntries" />
  </main>
  <footer
    style="width: 100% !important; display: flex !important; align-items: center !important; border-top: 1px solid #e2e8f0; background: white; padding: 0 !important; margin: 0 !important; overflow: hidden;">

    <img src="/img/Energies.jpg" alt="Footer Logo" style="
        height: 80px !important; 
        width: 2000px !important; 
        display: block !important; 
        margin: 0 !important; 
        padding: 0 !important;
        object-fit: fill !important;
      " />

    <div style="flex-grow: 1; text-align: right; padding-right: 20px;">
      <p style="color: #94a3b8; font-size: 10px; text-transform: uppercase; margin: 0;">
        &copy; 2024-2026 Energie Junkies
      </p>
    </div>
  </footer>
</template>

<script setup>
import { ref, onMounted, onUnmounted,provide } from "vue";
import Navbar from "./components/TopNavbar.vue";

const liveData = ref({
  config: { hasBattery: false }, // Standardmäßig erst mal aus
  battery: { soc: 0, power: 0 },
  pv: { power: 0 },
  // ...
});
const isConnected = ref(false);
const logEntries = ref([]);

const initWebSocket = () => {
  if (import.meta.env.DEV) {
    console.log("🛠️ Dev-Mode: Simuliere ESP32 Daten...");
    setInterval(() => {
      simulateData();
    },3000);
    
    isConnected.value = true;
    return;
  }


  const gateway = `ws://${window.location.hostname}/ws`;
  const socket = new WebSocket(gateway);

  socket.onopen = () => {
    isConnected.value = true;
  };
  socket.onclose = () => {
    isConnected.value = false;
    setTimeout(initWebSocket, 3000);
  };
  socket.onmessage = (event) => {
    const data = JSON.parse(event.data);
    liveData.value = data.live;
    logEntries.value = data.log.entries;
  };
};
// Simulation für die Entwicklung am PC
const simulateData = () => {
  
    liveData.value.battery.soc = Math.floor(Math.random() * 100);
    liveData.value.battery.power = Math.floor(Math.random() * 500);
    liveData.value.pv.power = Math.floor(Math.random() * 800);
    liveData.value.errors = 0; // Simuliere Bit 0 Fehler
    liveData.value.aakStat = Math.random() > 0.5 ? 20 : 40; // Simuliere Export/
    liveData.value.aakPower = Math.random() > 0.5 ? 3000 : 2000; // Simuliere Export/
    liveData.value.aakEntladen = Math.random() > 0.5 ? -1 : 1; // Simuliere Entladen/
    liveData.value.L1 = Math.random() > 0.5 ? 1 : 0; // Simuliere Entladen/
    liveData.value.L2 = Math.random() > 0.5 ? 1 : 0; // Simuliere Entladen/
    liveData.value.L3 = Math.random() > 0.5 ? 10 : 24; // Simuliere Entladen/
    liveData.value.bTemp = 45;
    liveData.value.solEr = 2000;
    liveData.value.ev = 1500;
    liveData.value.netzBezug = Math.random() > 0.5 ? 500 : -500; // Simuliere 
    liveData.value.netzBezug = 1;
    // Netzbezug/Netzeinspeisung
  };

onMounted(initWebSocket);
onUnmounted(() => socket?.close());
provide('harvesterStats', liveData);
provide('connectionStatus', isConnected);

</script>

<style></style>
