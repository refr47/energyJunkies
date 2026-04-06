<template>
  <div>
  <div class="min-h-screen flex items-center justify-center bg-slate-100 p-4">
    <div class="max-w-md w-full bg-white rounded-3xl shadow-xl p-8 border border-slate-200">
      <div class="text-center mb-8">
        <h1 class="text-2xl font-black text-slate-800 uppercase tracking-tighter">System Login</h1>
        <p class="text-slate-500 text-sm">Bitte Zugangsdaten eingeben</p>
      </div>

      <form @submit.prevent="handleLogin" class="space-y-4">
        <div>
          <label class="text-[10px] font-bold text-slate-400 uppercase ml-1">Benutzername</label>
          <input v-model="user" type="text"
            class="w-full bg-slate-50 border border-slate-200 rounded-xl px-4 py-3 outline-none focus:ring-2 focus:ring-blue-500"
            required>
        </div>
        <div>
          <label class="text-[10px] font-bold text-slate-400 uppercase ml-1">Passwort</label>
          <input v-model="password" type="password"
            class="w-full bg-slate-50 border border-slate-200 rounded-xl px-4 py-3 outline-none focus:ring-2 focus:ring-blue-500"
            required>
        </div>
        <Transition name="fade">
          <div v-if="errorMessage"
            class="bg-rose-50 border border-rose-200 text-rose-600 text-xs p-3 rounded-xl flex items-center gap-2">
            <span>⚠️</span>
            <p>{{ errorMessage }}</p>
          </div>
        </Transition>
        <button type="submit"
          class="w-full bg-slate-900 text-white font-bold py-4 rounded-xl hover:bg-blue-600 transition-all active:scale-95 shadow-lg">
          ANMELDEN
        </button>
      </form>
    </div>
    </div>
  </div>
</template>

<script setup>
import { ref } from 'vue';

const user = ref('');
const password = ref('');
const emit = defineEmits(['login-success']);
const errorMessage = ref(''); // Lokaler Ref für die Anzeige im Formular

const handleLogin = async () => {
  try {
    errorMessage.value = ''; // Vorherigen Fehler löschen
    const res = await fetch('http://10.0.0.19/login', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ user: user.value, password: password.value })
    });
    const data = await res.json();

    if (data.authenticated) {
      sessionStorage.setItem('isLoggedIn', 'true');
      emit('login-success'); // Signal an die Haupt-App: Seite freischalten
    } else {
      // Fehler vom Server (401, 400 etc.)
      errorMessage.value = data.error || "Ein unbekannter Fehler ist aufgetreten.";
    }
  } catch (e) {
    errorMessage.value = "Server nicht erreichbar";
  }
};
</script>