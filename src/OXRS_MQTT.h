/*
 * OXRS_MQTT.h
 * 
 */

#ifndef OXRS_MQTT_H
#define OXRS_MQTT_H

#include "Arduino.h"
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <OXRS_LCD.h>


static const char * MQTT_CONFIG_TOPIC     = "conf";
static const char * MQTT_COMMAND_TOPIC    = "cmnd";
static const char * MQTT_STATUS_TOPIC     = "stat";
static const char * MQTT_TELEMETRY_TOPIC  = "tele";

static const char * MQTT_LWT_SUFFIX       = "lwt";
static const char * MQTT_LWT_ONLINE       = "online";
static const char * MQTT_LWT_OFFLINE      = "offline";

#define MQTT_LWT_QOS                0
#define MQTT_LWT_RETAIN             1

#define MQTT_BACKOFF_SECS           5
#define MQTT_MAX_BACKOFF_COUNT      12

// Callback type for onConfig() and onCommand()
typedef void (*callback)(JsonObject);

class OXRS_MQTT
{
  public:
    OXRS_MQTT(PubSubClient& client, OXRS_LCD * screen);

    void setClientId(const char * deviceId);
    void setClientId(const char * deviceType, byte deviceMac[6]);
    void setAuth(const char * username, const char * password);

    void setTopicPrefix(const char * prefix);
    void setTopicSuffix(const char * suffix);
    
    char * getConfigTopic(char topic[]);
    char * getCommandTopic(char topic[]);
    char * getStatusTopic(char topic[]);
    char * getTelemetryTopic(char topic[]);
    
    void onConfig(callback);
    void onCommand(callback);

    void loop();
    void receive(char * topic, uint8_t * payload, unsigned int length);

    boolean publishStatus(JsonObject json);
    boolean publishTelemetry(JsonObject json);

  private:
    PubSubClient* _client;
    
    char _clientId[32];
    char _username[32];
    char _password[32];

    char _topicPrefix[32];
    char _topicSuffix[32];
    
    uint8_t _backoff;
    uint32_t _lastReconnectMs;
    
    OXRS_LCD * _screen;

    boolean _connect();

    callback _onConfig;
    callback _onCommand;

    char * _getTopic(char topic[], const char * topicType);
    boolean _publish(char topic[], JsonObject json);
};

#endif