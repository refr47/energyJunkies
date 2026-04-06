import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'

// https://vite.dev/config/
export default defineConfig({
  plugins: [vue()],
  build: {
    rollupOptions: {
      output: {
        // Wir erzwingen extrem kurze Namen für den ESP32
        entryFileNames: `assets/[name].js`,
        chunkFileNames: `assets/[name].js`,
        assetFileNames: `assets/[name].[ext]`,

        // Optional: Falls 'runtime-core.esm-bundler' immer noch kommt, 
        // benennen wir diesen spezifischen Chunk manuell um:
        manualChunks(id) {
          if (id.includes('node_modules')) {
            return 'vendor'; // Macht aus dem langen Namen einfach 'vendor.js'
          }
        }
      }
    }
  }
})