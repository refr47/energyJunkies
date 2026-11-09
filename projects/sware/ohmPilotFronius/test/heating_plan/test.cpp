#include "heatingPlan.h"
#include <cassert>
#include <cstdlib>

static time_t localAt(int day, int hour, int minute) {
    struct tm value{};
    value.tm_year = 126; value.tm_mon = 10; value.tm_mday = day;
    value.tm_hour = hour; value.tm_min = minute; value.tm_isdst = -1;
    return mktime(&value);
}

int main() {
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();
    HeatingPlanConfig c;
    assert(!heatingWindow(c, localAt(9, 22, 0)));
    assert(heatingPlanNeedsPolling(c, localAt(9, 23, 0), 4, true, 5));
    assert(!heatingPlanNeedsPolling(c, localAt(9, 23, 0), 5, true, 5));
    c.enabled = true;
    time_t end = 0;
    assert(heatingWindow(c, localAt(9, 22, 0), &end));
    assert(end == localAt(10, 6, 0));
    assert(heatingWindow(c, localAt(10, 5, 59)));
    assert(!heatingWindow(c, localAt(10, 6, 0)));
    assert(!heatingWindow(c, localAt(9, 20, 59)));
    assert(heatingPlanNeedsPolling(c, localAt(9, 23, 30), 40, false));
    assert(heatingPlanNeedsPolling(c, localAt(9, 12, 0), 1, true));
    assert(!heatingPlanNeedsPolling(c, localAt(9, 12, 0), 1, false));
    auto outside = evaluateHeatingPlan(c, localAt(9, 12, 0), 20, 45, 4500, 4500, true);
    assert(!outside.requestHeat && !outside.windowActive);
    auto emergency = evaluateHeatingPlan(c, localAt(9, 12, 0), 1, 45, 4500, 0, false);
    assert(emergency.requestHeat && emergency.emergency);
    auto waiting = evaluateHeatingPlan(c, localAt(9, 21, 0), 20, 45, 4500, 0, true);
    assert(waiting.requiredWh > 5800 && waiting.requiredWh < 5900);
    assert(!waiting.requestHeat && waiting.status[0] == 'w');
    auto deadline = evaluateHeatingPlan(c, end - 30, 20, 45, 4500, 0, true);
    assert(deadline.requestHeat && deadline.windowActive);
    auto target = evaluateHeatingPlan(c, localAt(9, 22, 0), 45, 45, 4500, 0, true);
    assert(!target.requestHeat && target.requiredWh == 0);
}
