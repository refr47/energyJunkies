import { createRouter, createWebHistory } from "vue-router";
import HomeView from "../views/HomeView.vue";

const routes = [
  { path: "/", component: HomeView },
  { path: "/setup", component: () => import("../views/SetupView.vue") },
  { path: "/out", component: () => import("../views/LiveView.vue") },
  { path: "/about", component: () => import("../views/AboutView.vue") },
  { path: "/logs", component: () => import("../views/LogView.vue") },
];

const router = createRouter({
  history: createWebHistory(),
  routes,
});

/* const router = createRouter({
  history: createWebHistory(process.env.BASE_URL),
  routes,
}); */

export default router;
