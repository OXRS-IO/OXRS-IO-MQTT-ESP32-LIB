/*
 * OXRS_MQTT.h
 *
 */

#ifndef OXRS_MQTT_H
#define OXRS_MQTT_H

#include "Arduino.h"
#include <ArduinoJson.h>
#include <PubSubClient.h>

// Increase the max MQTT message size for ESP or RPi based MCUs
#if defined (ESP32)
#define MQTT_MAX_MESSAGE_SIZE           16384
#elif defined (ESP8266)
#define MQTT_MAX_MESSAGE_SIZE           8192
#elif defined (RASPBERRYPI_PICO)
#define MQTT_MAX_MESSAGE_SIZE           16384
#else
#define MQTT_MAX_MESSAGE_SIZE           256
#endif

// Enable streaming for ESP based MCUs
#if (defined ESP8266 || defined ESP32)
#define MQTT_ENABLE_STREAMING
#endif

#define MQTT_DEFAULT_PORT               1883
#define MQTT_BACKOFF_SECS               5
#define MQTT_MAX_BACKOFF_COUNT          12
#define MQTT_STREAMING_BUFFER_SIZE      64

// Return codes for loop()
#define MQTT_CONNECTED                  0
#define MQTT_RECONNECT_BACKING_OFF      1
#define MQTT_RECONNECT_FAILED           2

// Return codes for receive()
#define MQTT_RECEIVE_OK                 0
#define MQTT_RECEIVE_ZERO_LENGTH        1
#define MQTT_RECEIVE_JSON_ERROR         2
#define MQTT_RECEIVE_NO_CONFIG_HANDLER  3
#define MQTT_RECEIVE_NO_COMMAND_HANDLER 4

// Callback types for onConnected()
typedef void (* connectedCallback)(void);

// Callback types for onDisconnected() - returns PubSubClient connection state
// - see https://github.com/knolleary/pubsubclient/blob/2d228f2f862a95846c65a8518c79f48dfc8f188c/src/PubSubClient.h#L44
typedef void (* disconnectedCallback)(int);

// Callback type for onConfig() and onCommand()
typedef void (* jsonCallback)(JsonVariant);

class OXRS_MQTT
{
  public:
    OXRS_MQTT(PubSubClient& client);

    char * getClientId(void);
    void setClientId(const char * clientId);

    void setBroker(const char * broker, uint16_t port);
    void setAuth(const char * username, const char * password);
    void setTopicPrefix(const char * prefix);
    void setTopicSuffix(const char * suffix);

    char * getWildcardTopic(char topic[]);
    char * getLwtTopic(char topic[]);
    char * getAdoptTopic(char topic[]);
    char * getLogTopic(char topic[]);

    char * getConfigTopic(char topic[]);
    char * getCommandTopic(char topic[]);

    char * getStatusTopic(char topic[]);
    char * getTelemetryTopic(char topic[]);

    void onConnected(connectedCallback);
    void onDisconnected(disconnectedCallback);
    void onConfig(jsonCallback);
    void onCommand(jsonCallback);

    void setConfig(JsonVariant json);
    void setCommand(JsonVariant json);

    int loop(void);
    int receive(char * topic, byte * payload, unsigned int length);

    bool connected(void);
    void reconnect(void);

    bool publishAdopt(JsonVariant json);
    bool publishStatus(JsonVariant json);
    bool publishTelemetry(JsonVariant json);
    bool publish(JsonVariant json, char * topic, bool retained);

  private:
    PubSubClient* _client;

    char _broker[32];
    uint16_t _port = MQTT_DEFAULT_PORT;
    char _clientId[32];
    char _username[32];
    char _password[32];
    char _topicPrefix[32];
    char _topicSuffix[32];

    uint8_t _backoff;
    uint32_t _lastReconnectMs;
    bool _connect(void);

    connectedCallback _onConnected;
    disconnectedCallback _onDisconnected;
    jsonCallback _onConfig;
    jsonCallback _onCommand;

    char * _getTopic(char topic[], const char * topicType);
};
#endif
