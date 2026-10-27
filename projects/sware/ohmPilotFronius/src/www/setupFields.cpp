#include "setupFields.h"
#include "ajaxConst.h"

// Getter/Setter fuer verschachtelte Members (Lambda-Field-Template)
static const char *mqttHostGet(const Setup &s) { return s.mqttSetup.mqttHost; }
static void mqttHostSet(Setup &s, const char *v) { strlcpy(s.mqttSetup.mqttHost, v, MQTT_HOST_LEN + 1); }
static const char *mqttUserGet(const Setup &s) { return s.mqttSetup.mqttUser; }
static void mqttUserSet(Setup &s, const char *v) { strlcpy(s.mqttSetup.mqttUser, v, MQTT_USER_LEN + 1); }
static const char *mqttPassGet(const Setup &s) { return s.mqttSetup.mqttPass; }
static void mqttPassSet(Setup &s, const char *v) { strlcpy(s.mqttSetup.mqttPass, v, MQTT_PASS_LEN + 1); }

static const char *influxHostGet(const Setup &s) { return s.influxSetup.influxHost; }
static void influxHostSet(Setup &s, const char *v) { strlcpy(s.influxSetup.influxHost, v, INFLUX_HOST_LEN + 1); }
static const char *influxTokenGet(const Setup &s) { return s.influxSetup.influxToken; }
static void influxTokenSet(Setup &s, const char *v) { strlcpy(s.influxSetup.influxToken, v, INFLUX_TOKEN_LEN + 1); }
static const char *influxOrgGet(const Setup &s) { return s.influxSetup.influxOrg; }
static void influxOrgSet(Setup &s, const char *v) { strlcpy(s.influxSetup.influxOrg, v, INFLUX_ORG_LEN + 1); }
static const char *influxBucketGet(const Setup &s) { return s.influxSetup.influxBucket; }
static void influxBucketSet(Setup &s, const char *v) { strlcpy(s.influxSetup.influxBucket, v, INFLUX_BUCKET_LEN + 1); }

// 1. Statische Instanzen der Felder (werden einmalig beim Start angelegt)
namespace Fields
{
    static const StringField<Setup, LEN_WLAN + 1> ssid(WLAN_ESSID, &Setup::ssid);
    static const StringField<Setup, LEN_WLAN + 1> pass(WLAN_PASSWD, &Setup::passwd);
    static const StringField<Setup, INET_ADDRSTRLEN + 1> inv(IP_INVERTER, &Setup::inverter);
    static const StringField<Setup, AMIS_KEY_LEN + 1> aKey(AMIS_READER_KEY, &Setup::amisKey);
    static const StringField<Setup, INET_ADDRSTRLEN + 1> aHost(AMIS_READER_HOST, &Setup::amisReaderHost);
    static const NumericField<int, Setup> force(FORCE_HEIZPATRONE, &Setup::forceHeating);
    static const NumericField<short, Setup> forceStop(FORCE_HEIZPATRONE_STOP, &Setup::forceHeatingStop);
    static const NumericField<unsigned int, Setup> leistung(HEIZSTABLEISTUNG, &Setup::heizstab_leistung_in_watt);
    static const NumericField<unsigned int, Setup> tMax(TEMP_AUSSCHALTEN, &Setup::tempMaxAllowedInGrad);
    static const NumericField<unsigned int, Setup> tMin(TEMP_EINSCHALT, &Setup::tempMinInGrad);
    static const NumericField<unsigned int, Setup> legDelta(LEGIONELLEN_DELTA_TIME, &Setup::legionellenDelta);
    static const NumericField<unsigned int, Setup> legMax(LEGIONELLEN_TEMP, &Setup::legionellenMaxTemp);

    static const BooleanField<Setup> akku(AKKU, &Setup::akku);
    static const NumericField<short, Setup> akkuPrio(AKKU_PRIORI, &Setup::akkuPriori);
    static const LambdaStringField<Setup, MQTT_HOST_LEN + 1> mHost(WWW_MQTT_HOST, mqttHostGet, mqttHostSet);
    static const LambdaStringField<Setup, MQTT_USER_LEN + 1> mUser(WWW_MQTT_USER, mqttUserGet, mqttUserSet);
    static const LambdaStringField<Setup, MQTT_PASS_LEN + 1> mPass(WWW_MQTT_PASWWD, mqttPassGet, mqttPassSet);

    static const LambdaStringField<Setup, INFLUX_HOST_LEN + 1> influxHost(WWW_INFLUX_HOST, influxHostGet, influxHostSet);
    static const LambdaStringField<Setup, INFLUX_TOKEN_LEN + 1> influxToken(WWW_INFLUX_TOKEN, influxTokenGet, influxTokenSet);
    static const LambdaStringField<Setup, INFLUX_ORG_LEN + 1> influxOrg(WWW_INFLUX_ORG, influxOrgGet, influxOrgSet);
    static const LambdaStringField<Setup, INFLUX_BUCKET_LEN + 1> influxBucket(WWW_INFLUX_BUCKET, influxBucketGet, influxBucketSet);
    static const NumericField<double, Setup> epsilon(PID_EPSILON, &Setup::epsilonML_PinManager);
    static const NumericField<unsigned int, Setup> biasWatt(WWW_WATT_BIAS, &Setup::wattSetupForTest);
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
    &Fields::forceStop,
    &Fields::influxHost,
    &Fields::influxToken,
    &Fields::influxOrg,
    &Fields::influxBucket,
    &Fields::biasWatt
};

const size_t setupFieldsCount = sizeof(setupFields) / sizeof(setupFields[0]);