/** @type {import('tailwindcss').Config} */
export default {
    content: [
        "./index.html",
        "./src/**/*.{vue,js,ts,jsx,tsx}",
    ],
    css: {
        transformer: 'lightningcss',
        minify: 'lightningcss',
    },
    build: {
        cssMinify: 'lightningcss'
    },
    theme: {
        extend: {
            // Hier kannst du eigene Farben für deinen E-Harvester definieren
            colors: {
                'energy-green': '#10b981',
                'energy-red': '#ef4444',
                'boiler-blue': '#3b82f6',
            },
            animation: {
                'pulse-slow': 'pulse 3s cubic-bezier(0.4, 0, 0.6, 1) infinite',
            }
        },
    },
    plugins: [],
}