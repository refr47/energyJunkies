#include "setupFields.h"
#include "ajaxConst.h"

// 1. Statische Instanzen der Felder (werden einmalig beim Start angelegt)
namespace Fields
{
    static const StringField<Setup, LEN_WLAN + 1> ssid(WLAN_ESSID, &Setup::ssid);
    static const StringField<Setup, LEN_WLAN + 1> pass(WLAN_PASSWD, &Setup::passwd);
    static const StringField<Setup, INET_ADDRSTRLEN + 1> inv(IP_INVERTER, &Setup::inverter);
    static const StringField<Setup, AMIS_KEY_LEN + 1> aKey(AMIS_READER_KEY, &Setup::amisKey);
    static const StringField<Setup, INET_ADDRSTRLEN + 1> aHost(AMIS_READER_HOST, &Setup::amisReaderHost);
    static const NumericField<int, Setup> force(FORCE_HEIZPATRONE, &Setup::forceHeating);
    static const NumericField<unsigned int, Setup> leistung(HEIZSTABLEISTUNG, &Setup::heizstab_leistung_in_watt);
    static const NumericField<unsigned int, Setup> tMax(TEMP_AUSSCHALTEN, &Setup::tempMaxAllowedInGrad);
    static const NumericField<unsigned int, Setup> tMin(TEMP_EINSCHALT, &Setup::tempMinInGrad);
    static const NumericField<unsigned int, Setup> legDelta(LEGIONELLEN_DELTA_TIME, &Setup::legionellenDelta);
    static const NumericField<unsigned int, Setup> legMax(LEGIONELLEN_TEMP, &Setup::legionellenMaxTemp);

    static const NumericField<short, Setup> akku(AKKU, &Setup::akku);
    static const NumericField<short, Setup> akkuPrio(AKKU_PRIORI, &Setup::akkuPriori);
    static const StringField<Setup, MQTT_HOST_LEN + 1> mHost(WWW_MQTT_HOST, &Setup::mqttHost);
    static const StringField<Setup, MQTT_USER_LEN + 1> mUser(WWW_MQTT_USER, &Setup::mqttUser);
    static const StringField<Setup, MQTT_PASS_LEN + 1> mPass(WWW_MQTT_PASWWD, &Setup::mqttPass);

    static const StringField<Setup, INFLUX_HOST_LEN + 1> influxHost(WWW_INFLUX_HOST, &Setup::mqttHost);
    static const StringField<Setup, INFLUX_TOKEN_LEN + 1> influxToken(WWW_INFLUX_TOKEN, &Setup::influxToken);
    static const StringField<Setup, INFLUX_ORG_LEN + 1> influxOrg(WWW_INFLUX_ORG, &Setup::influxOrg);
    static const StringField<Setup, INFLUX_BUCKET_LEN + 1> influxBucket(WWW_INFLUX_BUCKET, &Setup::influxBucket);
    static const NumericField<double, Setup> epsilon(PID_EPSILON, &Setup::epsilonML_PinManager);
    static const NumericField<int, Setup> biasWatt(WWW_WATT_BIAS, &Setup::wattSetupForTest);
}


// 2. Das zentrale Array(ebenfalls statisch)
const FieldBase<Setup> *setupFields[] = {
    &Fields::ssid,
    &Fields::pass,
    &Fields::leistung,
    &Fields::tMax,
    &Fields::tMin,
    &Fields::inv,
    &Fields::akku,
    &Fields::akkuPrio,
    &Fields::legDelta,
    &Fields::legMax,
    &Fields::aKey,
    &Fields::aHost,
    &Fields::mHost,
    &Fields::mUser,
    &Fields::mPass,
    &Fields::epsilon,
    &Fields::force,
    &Fields::influxHost,
    &Fields::influxToken,
    &Fields::influxOrg,
    &Fields::influxBucket,
    &Fields::biasWatt
};

const size_t setupFieldsCount = sizeof(setupFields) / sizeof(setupFields[0]);