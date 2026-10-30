#pragma once
#include <string>
using String = std::string;
struct Setup {};
struct ENERGY_STATE { double p_grid=0, p_load=0, p_pv=0, soc=0, p_akku=0; };
struct WEBSOCK_DATA { ENERGY_STATE energyState; };
enum class EnergySourceType { SOURCE_LOXONE };
class EnergySource {
public:
    virtual ~EnergySource() = default;
    virtual bool init(Setup &, WEBSOCK_DATA &) = 0;
    virtual bool read(WEBSOCK_DATA &) = 0;
    virtual const char* name() const = 0;
    virtual EnergySourceType type() const = 0;
    virtual bool hasBattery() const = 0;
    static inline bool publishOK = true;
    static inline int published = 0;
protected:
    static bool publishSample(WEBSOCK_DATA &data, ENERGY_STATE sample) {
        if (!publishOK) return false;
        data.energyState = sample;
        ++published;
        return true;
    }
};
