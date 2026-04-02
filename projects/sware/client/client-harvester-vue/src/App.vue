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
  <footer>
    <img src="/img/Energies.jpg" alt="Footer Logo" />
    <p>&copy; 2024-2026 Energie Junkies. All rights reserved.</p>
  </footer>
</template>

<script setup>
import { ref, onMounted } from "vue";
import Navbar from "./components/TopNavbar.vue";

const isConnected = ref(false);
const liveData = ref({});
const logEntries = ref([]);

const initWebSocket = () => {
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

onMounted(initWebSocket);
</script>

<style></style>
