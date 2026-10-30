#include "devices/loxoneAsProvider.h"
#include <HTTPClient.h>
#include <cassert>
#include <iostream>

void prepare()
{
    HTTPClient::replies.clear();
    HTTPClient::urls.clear();
    HTTPClient::next = 0;
    HTTPClient::closed = 0;
    for (double value : {-1200., -3000., 4500., -300., 75.})
        HTTPClient::replies.push_back({200, "{\"LL\":{\"Code\":200,\"value\":" + std::to_string(value) + "}}"});
}

int main()
{
    LoxoneAsProvider provider;
    Setup setup;
    WEBSOCK_DATA data;
    prepare();
    assert(provider.init(setup, data));
    assert(provider.hasBattery());
    assert(data.energyState.p_grid == -1200);
    assert(data.energyState.p_load == -3000);
    assert(data.energyState.p_pv == 4500);
    assert(data.energyState.p_akku == -300);
    assert(data.energyState.soc == 75);
    const char* names[] = {"P_Grid", "P_Load", "P_PV", "P_Akku", "SOC"};
    for (unsigned i=0; i<5; ++i)
        assert(HTTPClient::urls[i] == String("http://test-host/jdev/sps/io/") + names[i] + "/state");
    assert(HTTPClient::closed == 5);
    const int published = EnergySource::published;
    for (unsigned failed=0; failed<5; ++failed) {
        prepare();
        HTTPClient::replies[failed].code = 500;
        assert(!provider.read(data));
        assert(HTTPClient::closed == static_cast<int>(failed + 1));
        assert(EnergySource::published == published);
        assert(data.energyState.p_grid == -1200);
    }
    prepare();
    HTTPClient::replies[4].body = R"({"LL":{"Code":200,"value":101}})";
    assert(!provider.read(data));
    assert(EnergySource::published == published);
    prepare();
    HTTPClient::replies[2].body = "invalid";
    assert(!provider.init(setup, data));
    assert(!provider.hasBattery());
    assert(EnergySource::published == published);
    prepare();
    EnergySource::publishOK = false;
    assert(!provider.read(data));
    assert(EnergySource::published == published);
    EnergySource::publishOK = true;
    prepare();
    assert(provider.init(setup, data));
    assert(provider.hasBattery());
    std::cout << "Loxone provider tests passed\n";
}
