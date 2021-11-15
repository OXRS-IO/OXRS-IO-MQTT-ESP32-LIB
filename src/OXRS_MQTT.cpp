/*
 * OXRS_MQTT.cpp
 * 
 */

#include "Arduino.h"
#include "OXRS_MQTT.h"

OXRS_MQTT::OXRS_MQTT(PubSubClient& client) 
{
  this->_client = &client;
  
  // Set the buffer size (depends on MCU we are running on)
  _client->setBufferSize(MQTT_MAX_MESSAGE_SIZE);
}

char * OXRS_MQTT::getClientId()
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

void OXRS_MQTT::setMqttConfig(JsonVariant json)
{
  // broker is mandatory so don't clear if not explicitly specified
  if (json.containsKey("broker"))
  { 
    if (json.containsKey("port"))
    { 
      setBroker(json["broker"], json["port"].as<uint16_t>());
    }
    else
    {
      setBroker(json["broker"], MQTT_DEFAULT_PORT);
    }
  }
  
  // client id is mandatory so don't clear if not explicitly specified
  if (json.containsKey("clientId"))
  { 
    setClientId(json["clientId"]);
  }
  
  if (json.containsKey("username") && json.containsKey("password"))
  { 
    setAuth(json["username"], json["password"]);
  }
  else
  {
    setAuth(NULL, NULL);
  }
  
  if (json.containsKey("topicPrefix"))
  { 
    setTopicPrefix(json["topicPrefix"]);
  }
  else
  {
    setTopicPrefix(NULL);
  }

  if (json.containsKey("topicSuffix"))
  { 
    setTopicSuffix(json["topicSuffix"]);
  }
  else
  {
    setTopicSuffix(NULL);
  }
}

void OXRS_MQTT::setDeviceConfig(JsonVariant json)
{
  if (_onConfig)
  {
    _onConfig(json);
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

void OXRS_MQTT::onConnected(voidCallback callback)
{ 
  _onConnected = callback;
}

void OXRS_MQTT::onDisconnected(voidCallback callback)
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

void OXRS_MQTT::loop(void)
{
  if (_client->loop())
  {
    // Currently connected so ensure we are ready to reconnect if it drops
    _backoff = 0;
    _lastReconnectMs = millis();
  }
  else
  {
    // Calculate the backoff interval and check if we need to try again
    uint32_t backoffMs = (uint32_t)_backoff * MQTT_BACKOFF_SECS * 1000;
    if ((millis() - _lastReconnectMs) > backoffMs)
    {
      if (strlen(_broker) == 0)
      {
        // No broker configured, so backoff
        if (_backoff < MQTT_MAX_BACKOFF_COUNT) { _backoff++; }
        _lastReconnectMs = millis();

        Serial.print(F("[mqtt] no broker configured, retry in "));
        Serial.print(_backoff * MQTT_BACKOFF_SECS);
        Serial.println(F("s"));
      }
      else
      {
        // Attempt to connect
        int state = _connect();
        if (state == 0) 
        {
          // Connection successful
          Serial.println(F("[mqtt] connected"));
        }
        else
        {
          // Reconnection failed, so backoff
          if (_backoff < MQTT_MAX_BACKOFF_COUNT) { _backoff++; }
          _lastReconnectMs = millis();

          Serial.print(F("[mqtt] failed to connect with error "));
          Serial.print(state);
          Serial.print(F(", retry in "));
          Serial.print(_backoff * MQTT_BACKOFF_SECS);
          Serial.println(F("s"));
        }
      }
    }
  }
}

void OXRS_MQTT::receive(char * topic, byte * payload, unsigned int length)
{
  // Ignore if an empty message
  if (length == 0) return;

  // Tokenise the topic (skipping any prefix) to get the root topic type
  char * topicType;
  topicType = strtok(&topic[strlen(_topicPrefix)], "/");

  DynamicJsonDocument json(MQTT_MAX_MESSAGE_SIZE);
  DeserializationError error = deserializeJson(json, payload);
  if (error) return;

  // Forward to the appropriate callback
  if (strncmp(topicType, MQTT_CONFIG_TOPIC, strlen(MQTT_CONFIG_TOPIC)) == 0)
  {
    if (_onConfig)
    {
      _onConfig(json.as<JsonVariant>());
    }
  }
  else if (strncmp(topicType, MQTT_COMMAND_TOPIC, strlen(MQTT_COMMAND_TOPIC)) == 0)
  {
    if (_onCommand)
    {
      _onCommand(json.as<JsonVariant>());
    }
  }
}

boolean OXRS_MQTT::connected(void)
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

boolean OXRS_MQTT::publishAdopt(JsonVariant json)
{
  char topic[64];
  return _publish(json, getAdoptTopic(topic), true);
}

boolean OXRS_MQTT::publishStatus(JsonVariant json)
{
  char topic[64];
  return _publish(json, getStatusTopic(topic), false);
}

boolean OXRS_MQTT::publishTelemetry(JsonVariant json)
{
  char topic[64];
  return _publish(json, getTelemetryTopic(topic), false);
}

int OXRS_MQTT::_connect(void)
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
  boolean success = _client->connect(_clientId, _username, _password, getLwtTopic(topic), 0, true, lwtBuffer);
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
    if (_onDisconnected) { _onDisconnected(); }
  }

  return _client->state();
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

boolean OXRS_MQTT::_publish(JsonVariant json, char * topic, boolean retained)
{
  if (!_client->connected()) { return false; }
  
  char buffer[MQTT_MAX_MESSAGE_SIZE];
  serializeJson(json, buffer);
  
  _client->publish(topic, buffer, retained);
  return true;
}