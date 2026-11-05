#include "gridDemandAspect.h"
#include <cassert>
#include <cmath>
int main() {
    const int64_t base = 1800000000; // aligned quarter
    GridDemandConfig c; c.enabled = true; c.enforceLimit = true;
    GridDemandAspect a;
    auto feed = [&](int seconds, bool night=false, bool valid=true, double watts=8000) {
        a.update(c, base+seconds, seconds*1000U, night, valid, seconds+1, seconds*1000U, watts, 3000, 80000);
    };
    feed(0);
    assert(a.state().maxHeaterPowerW == 3500);
    assert(a.limitHeating(6000) == 3500 && a.state().limiting);
    for (int t=30;t<=900;t+=30) feed(t);
    assert(a.state().lastCompleted.complete);
    assert(std::abs(a.state().lastCompleted.importEnergyWh-2000)<0.001);
    assert(a.state().current.start == base+900);
    // A stale sample never opens more headroom, and eventually stops heating.
    a.update(c,base+902,902000,false,true,901,900000,8000,3000,80000);
    assert(a.state().maxHeaterPowerW <= 4500);
    a.update(c,base+990,990000,false,true,901,900000,8000,3000,80000);
    assert(a.limitHeating(6000)==0);
    feed(1020,true);
    assert(a.limitHeating(6000)==6000); // night is explicitly exempt
    feed(1800,true);
    assert(!a.state().lastCompleted.complete);
    feed(1830);
    assert(!a.state().current.complete);
    c.enabled=false;feed(1860);
    assert(a.limitHeating(6000)==6000 && !a.state().current.start);
    c.enabled=true;feed(1890);
    assert(!a.state().current.complete);
    c.enforceLimit=false;feed(1920);
    assert(a.limitHeating(6000)==6000);
    // Measurements straddling a boundary split the held energy correctly.
    GridDemandAspect b;
    b.update(c,base+890,1000,false,true,1,1000,3600,0,80000);
    b.update(c,base+910,21000,false,true,2,21000,3600,0,80000);
    assert(b.state().lastCompleted.importEnergyWh==10);
    assert(b.state().current.importEnergyWh==10);
    assert(!b.state().lastCompleted.complete && b.state().current.complete);
    b.update(c,base+800,22000,false,true,3,22000,3600,0,80000);
    assert(!b.state().current.complete); // backward clock correction
    c.enforceLimit=true;
    b.update(c,0,23000,false,true,4,23000,3600,0,80000);
    assert(b.limitHeating(6000)==0);
    // Import only: export never cancels purchased energy.
    GridDemandAspect d;
    d.update(c,base,0,false,true,1,0,-5000,0,80000);
    d.update(c,base+30,30000,false,true,2,30000,10000,0,80000);
    assert(d.state().current.importEnergyWh==0);
}
