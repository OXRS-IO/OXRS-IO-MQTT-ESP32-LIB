/*
 * OXRS_MQTT.h
 * 
 */

#ifndef OXRS_MQTT_H
#define OXRS_MQTT_H

#include "Arduino.h"
#include <ArduinoJson.h>
#include <PubSubClient.h>

static const char * MQTT_CONFIG_TOPIC     = "conf";
static const char * MQTT_COMMAND_TOPIC    = "cmnd";
static const char * MQTT_STATUS_TOPIC     = "stat";
static const char * MQTT_TELEMETRY_TOPIC  = "tele";

static const char * MQTT_LWT_SUFFIX       = "lwt";
static const char * MQTT_LWT_ONLINE       = "online";
static const char * MQTT_LWT_OFFLINE      = "offline";

#define MQTT_DEFAULT_PORT           1883
#define MQTT_LWT_QOS                0
#define MQTT_LWT_RETAIN             1

#define MQTT_BACKOFF_SECS           5
#define MQTT_MAX_BACKOFF_COUNT      12

// Callback type for onConfig() and onCommand()
typedef void (* jsonCallback)(JsonObject);

class OXRS_MQTT
{
  public:
    OXRS_MQTT(PubSubClient& client);

    void setJson(DynamicJsonDocument * json);

    void setBroker(const char * broker, uint16_t port);
    void setAuth(const char * username, const char * password);
    void setClientId(const char * clientId);
    void setClientId(const char * deviceType, byte deviceMac[6]);
    void setTopicPrefix(const char * prefix);
    void setTopicSuffix(const char * suffix);
    
    char * getWildcardTopic(char topic[]);
    char * getConfigTopic(char topic[]);
    char * getCommandTopic(char topic[]);
    char * getStatusTopic(char topic[]);
    char * getTelemetryTopic(char topic[]);
    
    void onConfig(jsonCallback);
    void onCommand(jsonCallback);

    void loop(void);    
    void receive(char * topic, byte * payload, unsigned int length);
    void reconnect(void);
    
    boolean publishStatus(JsonObject json);
    boolean publishTelemetry(JsonObject json);

  private:
    PubSubClient* _client;
    
    char _broker[32];
    uint16_t _port;
    char _username[32];
    char _password[32];
    char _clientId[32];
    char _topicPrefix[32];
    char _topicSuffix[32];
    
    uint8_t _backoff;
    uint32_t _lastReconnectMs;

    // Returns PubSubClient connection state 
    // - see https://github.com/knolleary/pubsubclient/blob/2d228f2f862a95846c65a8518c79f48dfc8f188c/src/PubSubClient.h#L44
    int _connect(void);

    jsonCallback _onConfig;
    jsonCallback _onCommand;
    
    void _handlePayload(const char * topicType, DynamicJsonDocument * json);
    void _fireCallback(const char * topicType, JsonObject json);

    boolean _publish(char * topic, JsonObject json);
    char * _getTopic(char topic[], const char * topicType);
};

#endif