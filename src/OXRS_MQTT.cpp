/*
 * OXRS_MQTT.cpp
 * 
 */

#include "Arduino.h"
#include "OXRS_MQTT.h"

#ifdef MQTT_ENABLE_STREAMING
#include <StreamUtils.h>
#endif

// Topic constants
static const char * MQTT_CONFIG_TOPIC     = "conf";
static const char * MQTT_COMMAND_TOPIC    = "cmnd";
static const char * MQTT_STATUS_TOPIC     = "stat";
static const char * MQTT_TELEMETRY_TOPIC  = "tele";

OXRS_MQTT::OXRS_MQTT(PubSubClient& client) 
{
  this->_client = &client;

  // Set the buffer size (depends on MCU we are running on)
  _client->setBufferSize(MQTT_MAX_MESSAGE_SIZE);
}

char * OXRS_MQTT::getClientId(void)
{
  return _clientId;
}

void OXRS_MQTT::setClientId(const char * clientId)
{ 
  strcpy(_clientId, clientId);
}

void OXRS_MQTT::setBroker(const char * broker, uint16_t port)
{
    strcpy(_broker, broker);
    _port = port;
}

void OXRS_MQTT::setAuth(const char * username, const char * password)
{
  if (username == NULL)
  {
    _username[0] = '\0';
    _password[0] = '\0';
  }
  else
  {
    strcpy(_username, username);
    strcpy(_password, password);
  }
}

void OXRS_MQTT::setTopicPrefix(const char * prefix)
{ 
  if (prefix == NULL)
  {
    _topicPrefix[0] = '\0';
  }
  else
  {
    strcpy(_topicPrefix, prefix);
  }
}

void OXRS_MQTT::setTopicSuffix(const char * suffix)
{ 
  if (suffix == NULL) 
  {
    _topicSuffix[0] = '\0';
  }
  else
  {
    strcpy(_topicSuffix, suffix);
  }
}

char * OXRS_MQTT::getWildcardTopic(char topic[])
{
  return _getTopic(topic, "+");
}

char * OXRS_MQTT::getLwtTopic(char topic[])
{
  sprintf_P(topic, PSTR("%s/%s"), getStatusTopic(topic), "lwt");
  return topic;
}

char * OXRS_MQTT::getAdoptTopic(char topic[])
{
  sprintf_P(topic, PSTR("%s/%s"), getStatusTopic(topic), "adopt");
  return topic;
}

char * OXRS_MQTT::getLogTopic(char topic[])
{
  sprintf_P(topic, PSTR("%s/%s"), getStatusTopic(topic), "log");
  return topic;
}

char * OXRS_MQTT::getConfigTopic(char topic[])
{
  return _getTopic(topic, MQTT_CONFIG_TOPIC);
}

char * OXRS_MQTT::getCommandTopic(char topic[])
{
  return _getTopic(topic, MQTT_COMMAND_TOPIC);
}

char * OXRS_MQTT::getStatusTopic(char topic[])
{
  return _getTopic(topic, MQTT_STATUS_TOPIC);
}

char * OXRS_MQTT::getTelemetryTopic(char topic[])
{
  return _getTopic(topic, MQTT_TELEMETRY_TOPIC);
}

void OXRS_MQTT::onConnected(connectedCallback callback)
{ 
  _onConnected = callback;
}

void OXRS_MQTT::onDisconnected(disconnectedCallback callback)
{ 
  _onDisconnected = callback;
}

void OXRS_MQTT::onConfig(jsonCallback callback)
{ 
  _onConfig = callback;
}

void OXRS_MQTT::onCommand(jsonCallback callback)
{ 
  _onCommand = callback;
}

void OXRS_MQTT::setConfig(JsonVariant json)
{
  if (_onConfig)
  {
    _onConfig(json);
  }
}

void OXRS_MQTT::setCommand(JsonVariant json)
{
  if (_onCommand)
  {
    _onCommand(json);
  }
}

int OXRS_MQTT::loop(void)
{
  // Let the MQTT client handle any messages
  if (_client->loop())
  {
    // Currently connected so ensure we are ready to reconnect if it drops
    _backoff = 0;
    _lastReconnectMs = millis();
  }
  else
  {
    // Not connected so calculate the backoff interval and check if we need to try again
    uint32_t backoffMs = (uint32_t)_backoff * MQTT_BACKOFF_SECS * 1000;
    if ((millis() - _lastReconnectMs) > backoffMs)
    {
      // Attempt to connect
      if (!_connect()) 
      {
        // Reconnection failed, so backoff more
        if (_backoff < MQTT_MAX_BACKOFF_COUNT) { _backoff++; }
        _lastReconnectMs = millis();
        return MQTT_RECONNECT_FAILED;
      }
    }
    else
    {
      // Waiting for our reconnect backoff timer to expire
      return MQTT_RECONNECT_BACKING_OFF;
    }
  }

  return MQTT_CONNECTED;
}

int OXRS_MQTT::receive(char * topic, byte * payload, unsigned int length)
{
  // Ignore if an empty message
  if (length == 0) { return MQTT_RECEIVE_ZERO_LENGTH; }

  // Tokenise the topic (skipping any prefix) to get the root topic type
  char * topicType;
  topicType = strtok(&topic[strlen(_topicPrefix)], "/");

  DynamicJsonDocument json(MQTT_MAX_MESSAGE_SIZE);
  DeserializationError error = deserializeJson(json, payload);
  if (error) { return MQTT_RECEIVE_JSON_ERROR; }

  // Forward to the appropriate callback
  if (strncmp(topicType, MQTT_CONFIG_TOPIC, strlen(MQTT_CONFIG_TOPIC)) == 0)
  {
    if (!_onConfig) { return MQTT_RECEIVE_NO_CONFIG_HANDLER; }
    _onConfig(json.as<JsonVariant>());
  }
  else if (strncmp(topicType, MQTT_COMMAND_TOPIC, strlen(MQTT_COMMAND_TOPIC)) == 0)
  {
    if (!_onCommand) { return MQTT_RECEIVE_NO_COMMAND_HANDLER; }
    _onCommand(json.as<JsonVariant>());
  }
  
  return MQTT_RECEIVE_OK;
}

bool OXRS_MQTT::connected(void)
{
  return _client->connected();
}

void OXRS_MQTT::reconnect(void)
{
  // Disconnect from MQTT broker
  _client->disconnect();
  
  // Force a connect attempt immediately
  _backoff = 0;
  _lastReconnectMs = millis();
}

bool OXRS_MQTT::publishAdopt(JsonVariant json)
{
  char topic[64];
  return _publish(json, getAdoptTopic(topic), true);
}

bool OXRS_MQTT::publishStatus(JsonVariant json)
{
  char topic[64];
  return _publish(json, getStatusTopic(topic), false);
}

bool OXRS_MQTT::publishTelemetry(JsonVariant json)
{
  char topic[64];
  return _publish(json, getTelemetryTopic(topic), false);
}

bool OXRS_MQTT::getHassDiscoveryEnabled(void)
{
  return _hassDiscoveryEnabled;
}

void OXRS_MQTT::setHassDiscoveryEnabled(bool enabled)
{
  _hassDiscoveryEnabled = enabled;
}

void OXRS_MQTT::setHassDiscoveryTopicPrefix(const char * prefix)
{
  strcpy(_hassDiscoveryTopicPrefix, prefix);
}

void OXRS_MQTT::getHassDiscoveryJson(JsonVariant json, char * id)
{
  char uniqueId[64];
  sprintf_P(uniqueId, PSTR("%s_%s"), getClientId(), id);
  json["uniq_id"] = uniqueId;
  json["obj_id"] = uniqueId;

  char topic[64];
  json["avty_t"] = getLwtTopic(topic);
  json["avty_tpl"] = "{% if value_json.online == true %}online{% else %}offline{% endif %}";

  JsonObject dev = json.createNestedObject("dev");
  dev["name"] = getClientId();

  JsonArray ids = dev.createNestedArray("ids");
  ids.add(getClientId());
}

bool OXRS_MQTT::publishHassDiscovery(JsonVariant json, char * component, char * id)
{
  // Exit early if not enabled
  if (!_hassDiscoveryEnabled) { return false; }

  // Check for a null payload and ensure we send an empty JSON object
  // to clear any existing Home Assistant config
  if (json.isNull())
  {
    json = json.to<JsonObject>();
  }

  // Build the discovery topic and publish retained
  char topic[64];
  sprintf_P(topic, PSTR("%s/%s/%s/%s/config"), _hassDiscoveryTopicPrefix, component, getClientId(), id);
  return _publish(json, topic, true);
}

bool OXRS_MQTT::_connect(void)
{
  // Set the broker address and port (in case they have changed)
  _client->setServer(_broker, _port);

  // Build our LWT payload
  const int capacity = JSON_OBJECT_SIZE(1);
  StaticJsonDocument<capacity> lwtJson;
  lwtJson["online"] = false;
  
  // Get our LWT offline payload as raw string
  char lwtBuffer[24];
  serializeJson(lwtJson, lwtBuffer);
 
  // Attempt to connect to the MQTT broker
  char topic[64];
  bool success = _client->connect(_clientId, _username, _password, getLwtTopic(topic), 0, true, lwtBuffer);
  if (success)
  {
    // Subscribe to our config and command topics
    _client->subscribe(getConfigTopic(topic));
    _client->subscribe(getCommandTopic(topic));

    // Publish our LWT online payload now we are ready
    lwtJson["online"] = true;
    _publish(lwtJson.as<JsonVariant>(), getLwtTopic(topic), true);
 
    // Fire the connected callback
    if (_onConnected) { _onConnected(); }
  }
  else
  {
    // Fire the disconnected callback
    if (_onDisconnected) { _onDisconnected(_client->state()); }
  }

  return success;
}

char * OXRS_MQTT::_getTopic(char topic[], const char * topicType)
{
  if (strlen(_topicPrefix) == 0)
  {
    if (strlen(_topicSuffix) == 0)
    {
      sprintf_P(topic, PSTR("%s/%s"), topicType, _clientId);
    }
    else
    {
      sprintf_P(topic, PSTR("%s/%s/%s"), topicType, _clientId, _topicSuffix);
    }
  }
  else
  {
    if (strlen(_topicSuffix) == 0)
    {
      sprintf_P(topic, PSTR("%s/%s/%s"), _topicPrefix, topicType, _clientId);
    }
    else
    {
      sprintf_P(topic, PSTR("%s/%s/%s/%s"), _topicPrefix, topicType, _clientId, _topicSuffix);
    }
  }
  
  return topic;
}

bool OXRS_MQTT::_publish(JsonVariant json, char * topic, bool retained)
{
  if (!_client->connected()) { return false; }
  
#ifdef MQTT_ENABLE_STREAMING
  // Publish as a buffered stream
  _client->beginPublish(topic, measureJson(json), retained);
  BufferingPrint bufferedClient(*_client, MQTT_STREAMING_BUFFER_SIZE);
  serializeJson(json, bufferedClient);
  bufferedClient.flush();
  _client->endPublish();
#else
  // Write to a temporary buffer and then publish
  char buffer[MQTT_MAX_MESSAGE_SIZE];
  serializeJson(json, buffer);
  _client->publish(topic, buffer, retained);  
#endif

  return true;
}