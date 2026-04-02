import { createApp } from "vue";
import App from "./App.vue";
import router from "./router";

import "./assets/css/main.css";
// import "./assets/css/menu.css";
// import "./assets/css/output.css";

createApp(App).use(router).mount("#app");
