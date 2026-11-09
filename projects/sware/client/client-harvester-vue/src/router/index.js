import { createRouter, createWebHistory } from "vue-router";
import HomeView from "../views/HomeView.vue";
import LogView from "../views/LogView.vue";
import SetupView from "../views/SetupView.vue";

const routes = [
  { path: "/", component: HomeView },
  { path: "/setup", component: SetupView },
  { path: "/out", component: () => import("../views/LiveView.vue") },
  { path: "/about", component: () => import("../views/AboutView.vue") },
  { path: "/logs", component: LogView },
  { path: "/heizplan", component: () => import("../views/HeatingPlanView.vue") },
  { path: "/netzbezug", component: () => import("../views/GridDemandView.vue") },
  { path: "/netzbezug/verlauf", component: () => import("../views/GridDemandHistoryView.vue") },
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
