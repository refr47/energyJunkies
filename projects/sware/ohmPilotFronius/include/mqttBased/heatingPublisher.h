#pragma once

#if defined(FEATURE_MQTT) && defined(LOXONE_API)
// Call change handlers while holding the application data lock.
void heatingPublisher_powerChanged(int watts);
void heatingPublisher_temperatureChanged(int degrees);
bool heatingPublisher_isTopic(const char *topic);

// Called by the MQTT-owning clock task after a successful connection.
bool heatingPublisher_resendSnapshot();
#endif
