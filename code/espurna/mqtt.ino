/*

ESPurna
MQTT MODULE

Copyright (C) 2016 by Xose Pérez <xose dot perez at gmail dot com>

*/

#include <ESP8266WiFi.h>
#include <AsyncMqttClient.h>
#include <vector>

AsyncMqttClient mqtt;

String mqttTopic;
std::vector<void (*)(unsigned int, const char *, const char *)> _mqtt_callbacks;
#if MQTT_SKIP_RETAINED
    unsigned long mqttConnectedAt = 0;
#endif

// -----------------------------------------------------------------------------
// MQTT
// -----------------------------------------------------------------------------

bool mqttConnected() {
    return mqtt.connected();
}

void mqttDisconnect() {
    mqtt.disconnect();
}

void buildTopics() {
    // Replace identifier
    mqttTopic = getSetting("mqttTopic", MQTT_TOPIC);
    mqttTopic.replace("{identifier}", getSetting("hostname"));
}

unsigned int mqttTopicRootLength() {
    return mqttTopic.length();
}

void mqttSendRaw(const char * topic, const char * message) {
    if (mqtt.connected()) {
        DEBUG_MSG("[MQTT] Sending %s %s\n", topic, message);
        mqtt.publish(topic, MQTT_QOS, MQTT_RETAIN, message);
    }
}

void mqttSend(const char * topic, const char * message) {
    String path = mqttTopic + String(topic);
    mqttSendRaw(path.c_str(), message);
}

void mqttSubscribeRaw(const char * topic) {
    if (mqtt.connected()) {
        DEBUG_MSG("[MQTT] Subscribing to %s\n", topic);
        mqtt.subscribe(topic, MQTT_QOS);
    }
}

void mqttSubscribe(const char * topic) {
    String path = mqttTopic + String(topic);
    mqttSubscribeRaw(path.c_str());
}

void mqttRegister(void (*callback)(unsigned int, const char *, const char *)) {
    _mqtt_callbacks.push_back(callback);
}

void _mqttOnConnect(bool sessionPresent) {

    DEBUG_MSG("[MQTT] Connected!\n");

    #if MQTT_SKIP_RETAINED
        mqttConnectedAt = millis();
    #endif

    // Build MQTT topics
    buildTopics();
    mqtt.setWill((mqttTopic + MQTT_HEARTBEAT_TOPIC).c_str(), MQTT_QOS, MQTT_RETAIN, (char *) "0");

    // Say hello and report our IP and VERSION
    mqttSend(MQTT_IP_TOPIC, getIP().c_str());
    mqttSend(MQTT_VERSION_TOPIC, APP_VERSION);
    char buffer[50];
    getFSVersion(buffer);
    mqttSend(MQTT_FSVERSION_TOPIC, buffer);

    // Send connect event to subscribers
    for (unsigned char i = 0; i < _mqtt_callbacks.size(); i++) {
        (*_mqtt_callbacks[i])(MQTT_CONNECT_EVENT, NULL, NULL);
    }

}

void _mqttOnDisconnect(AsyncMqttClientDisconnectReason reason) {

    DEBUG_MSG("[MQTT] Disconnected!\n");

    // Send disconnect event to subscribers
    for (unsigned char i = 0; i < _mqtt_callbacks.size(); i++) {
        (*_mqtt_callbacks[i])(MQTT_DISCONNECT_EVENT, NULL, NULL);
    }

}

void _mqttOnMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {

    char message[len+1];
    strlcpy(message, payload, len+1);

    DEBUG_MSG("[MQTT] Received %s => %s", topic, message);

    #if MQTT_SKIP_RETAINED
        if (millis() - mqttConnectedAt < MQTT_SKIP_TIME) {
			DEBUG_MSG(" - SKIPPED\n");
			return;
		}
    #endif

	DEBUG_MSG("\n");

    // Send message event to subscribers
    // Topic is set to the specific part each one might be checking
    for (unsigned char i = 0; i < _mqtt_callbacks.size(); i++) {
        (*_mqtt_callbacks[i])(MQTT_MESSAGE_EVENT, topic, message);
    }

}

void mqttConnect() {

    if (!mqtt.connected()) {

        String host = getSetting("mqttServer", MQTT_SERVER);
        String port = getSetting("mqttPort", MQTT_PORT);
        String user = getSetting("mqttUser");
        String pass = getSetting("mqttPassword");

		if (host.length() == 0) return;

        DEBUG_MSG("[MQTT] Connecting to broker at %s", (char *) host.c_str());

        mqtt.setServer(host.c_str(), port.toInt());
        mqtt
            .setKeepAlive(MQTT_KEEPALIVE)
            .setCleanSession(false);

        if ((user != "") && (pass != "")) {
            DEBUG_MSG(" as user '%s'.\n", (char *) user.c_str());
            char username[user.length()+1];
            user.toCharArray(username, user.length()+1);
            char password[pass.length()+1];
            pass.toCharArray(password, pass.length()+1);
            mqtt.setCredentials(username, password);
        } else {
            DEBUG_MSG(" anonymously\n");
        }

        mqtt.connect();

    }

}

void mqttSetup() {
    mqtt.onConnect(_mqttOnConnect);
    mqtt.onDisconnect(_mqttOnDisconnect);
    mqtt.onMessage(_mqttOnMessage);
}

void mqttLoop() {

    static unsigned long lastPeriod = 0;

    if (WiFi.status() == WL_CONNECTED) {

        if (!mqtt.connected()) {

        	unsigned long currPeriod = millis() / MQTT_RECONNECT_DELAY;
        	if (currPeriod != lastPeriod) {
        	    lastPeriod = currPeriod;
                mqttConnect();
            }

        }

    }

}
