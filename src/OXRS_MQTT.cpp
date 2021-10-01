/*
 * OXRS_MQTT.cpp
 * 
 */

#include "Arduino.h"
#include "OXRS_MQTT.h"

#if defined(ARDUINO_ARCH_ESP32)
#include <SPIFFS.h>
#elif defined(ARDUINO_ARCH_ESP8266)
#include <FS.h>
#endif

OXRS_MQTT::OXRS_MQTT(PubSubClient& client) 
{
  this->_client = &client;
}

void OXRS_MQTT::getSetup(DynamicJsonDocument * json)
{
  // Populate with current setup
  json->getOrAddMember("broker").set(_broker);
  json->getOrAddMember("port").set(_port);
  json->getOrAddMember("clientId").set(_clientId);
  json->getOrAddMember("username").set(_username);
  json->getOrAddMember("password").set(_password);
  json->getOrAddMember("topicPrefix").set(_topicPrefix);
  json->getOrAddMember("topicSuffix").set(_topicSuffix);
}

void OXRS_MQTT::setSetup(DynamicJsonDocument * json)
{
  Serial.print(F("[mqtt] updating setup: "));
  serializeJson(*json, Serial);
  Serial.println();
  
  // Update with new setup
  if (json->containsKey("broker"))
  { 
    if (json->containsKey("port"))
    { 
      setBroker(json->getMember("broker"), json->getMember("port").as<uint16_t>());
    }
    else
    {
      setBroker(json->getMember("broker"), MQTT_DEFAULT_PORT);      
    }
  }
  
  if (json->containsKey("clientId"))
  { 
    setClientId(json->getMember("clientId"));
  }
  
  if (json->containsKey("username") && json->containsKey("password"))
  { 
    setAuth(json->getMember("username"), json->getMember("password"));
  }
  
  if (json->containsKey("topicPrefix"))
  { 
    setTopicPrefix(json->getMember("topicPrefix"));
  }

  if (json->containsKey("topicSuffix"))
  { 
    setTopicSuffix(json->getMember("topicSuffix"));
  }
  
  // Persist new setup so it is restored on boot
  _saveJson(json, MQTT_SETUP_FILENAME);
  
  // Attempt to reconnect using these new settings
  _reconnect();
}

void OXRS_MQTT::factoryReset(boolean formatFS)
{
  if (formatFS)
  {
    _formatFS();
  }
  else
  {
    DynamicJsonDocument empty(0);
    _saveJson(&empty, MQTT_SETUP_FILENAME);
    _saveJson(&empty, MQTT_CONFIG_FILENAME);
  }
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

void OXRS_MQTT::setClientId(const char * clientId)
{ 
  strcpy(_clientId, clientId);
}

void OXRS_MQTT::setClientId(const char * deviceType, byte deviceMac[6])
{
  sprintf_P(_clientId, PSTR("%s-%02x%02x%02x"), deviceType, deviceMac[3], deviceMac[4], deviceMac[5]);  
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

void OXRS_MQTT::onConfig(jsonCallback callback)
{ 
  _onConfig = callback; 
}

void OXRS_MQTT::onCommand(jsonCallback callback)
{ 
  _onCommand = callback; 
}

void OXRS_MQTT::begin(void)
{
  // Mount the SPIFFS
  _mountFS();

  DynamicJsonDocument json(2048);

  // Restore any persisted setup
  if (_loadJson(&json, MQTT_SETUP_FILENAME))
  {
    _restoreSetup(&json);
  }  
  
  // Restore any persisted config
  if (_loadJson(&json, MQTT_CONFIG_FILENAME))
  {
    _restoreConfig(&json);
  }
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
      Serial.print(F("[mqtt] connecting to "));
      Serial.print(_broker);
      Serial.print(F(":"));
      Serial.print(_port);      
      if (strlen(_username) > 0)
      {
        Serial.print(F(" as "));
        Serial.print(_username);
      }
      Serial.print(F("..."));

      int state = _connect();
      if (state == 0) 
      {
        Serial.println(F("done"));

        char topic[64];
        Serial.print(F("[mqtt] config    <-- "));
        Serial.println(getConfigTopic(topic));
        Serial.print(F("[mqtt] commands  <-- "));
        Serial.println(getCommandTopic(topic));
        Serial.print(F("[mqtt] status    --> "));
        Serial.println(getStatusTopic(topic));
        Serial.print(F("[mqtt] telemetry --> "));
        Serial.println(getTelemetryTopic(topic));
      }
      else
      {
        // Reconnection failed, so backoff
        if (_backoff < MQTT_MAX_BACKOFF_COUNT) { _backoff++; }
        _lastReconnectMs = millis();

        Serial.print(F("failed with error "));
        Serial.print(state);
        Serial.print(F(", retry in "));
        Serial.print(_backoff * MQTT_BACKOFF_SECS);
        Serial.println(F("s"));
      }
    }
  }
}

void OXRS_MQTT::receive(char * topic, byte * payload, unsigned int length)
{
  // Log each received message to serial for debugging
  Serial.print(F("[mqtt-rx] "));
  Serial.print(topic);
  Serial.print(F(" "));
  if (length == 0)
  {
    Serial.println(F("null"));
  }
  else
  {
    for (int i = 0; i < length; i++)
    {
      Serial.print((char)payload[i]);
    }
    Serial.println();
  }

  // Tokenise the topic (skipping any prefix) to get the root topic type
  char * topicType;
  topicType = strtok(&topic[strlen(_topicPrefix)], "/");

  DynamicJsonDocument json(2048);
  DeserializationError error = deserializeJson(json, payload);
  if (error) 
  {
    Serial.print(F("[erro] failed to deserialise JSON: "));
    Serial.println(error.f_str());
    return;
  }

  // Handle this JSON payload
  _handlePayload(topicType, &json);

  // If this is config then save so it can be restored on startup
  if (strncmp(topicType, MQTT_CONFIG_TOPIC, 4) == 0)
  {
    _saveJson(&json, MQTT_CONFIG_FILENAME);
  }
}

boolean OXRS_MQTT::publishStatus(JsonObject json)
{
  char topic[64];
  return _publish(getStatusTopic(topic), json);
}

boolean OXRS_MQTT::publishTelemetry(JsonObject json)
{
  char topic[64];
  return _publish(getTelemetryTopic(topic), json);
}

void OXRS_MQTT::_mountFS()
{
#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266)
  Serial.print(F("[file] mounting SPIFFS..."));
  if (!SPIFFS.begin())
  { 
    Serial.println(F("failed, might need formatting?"));
    return; 
  }
  Serial.println(F("done"));
#endif
}

void OXRS_MQTT::_formatFS()
{
#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266)
  Serial.print(F("[file] formatting SPIFFS..."));
  if (!SPIFFS.format())
  { 
    Serial.println(F("failed"));
    return; 
  }
  Serial.println(F("done"));
#endif
}

void OXRS_MQTT::_restoreSetup(DynamicJsonDocument * json)
{
  Serial.print(F("[mqtt] restoring setup: "));
  serializeJson(*json, Serial);
  Serial.println();

  if (json->containsKey("broker"))
  {
    if (json->containsKey("port"))
    { 
      setBroker(json->getMember("broker"), json->getMember("port").as<uint16_t>());
    }
    else
    {
      setBroker(json->getMember("broker"), MQTT_DEFAULT_PORT);
    }
  }
    
  if (json->containsKey("clientId"))
  { 
    setClientId(json->getMember("clientId"));
  }
    
  if (json->containsKey("username"))
  { 
    setAuth(json->getMember("username"), json->getMember("password"));
  }
    
  if (json->containsKey("topicPrefix"))
  {
    setTopicPrefix(json->getMember("topicPrefix"));
  }
    
  if (json->containsKey("topicSuffix"))
  {
    setTopicPrefix(json->getMember("topicSuffix"));
  }
}

void OXRS_MQTT::_restoreConfig(DynamicJsonDocument * json)
{
  Serial.print(F("[mqtt] restoring config: "));
  serializeJson(*json, Serial);
  Serial.println();

  _handlePayload(MQTT_CONFIG_TOPIC, json);
}

boolean OXRS_MQTT::_loadJson(DynamicJsonDocument * json, const char * filename)
{
#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266)
  Serial.print(F("[file] reading "));  
  Serial.print(filename);
  Serial.print(F("..."));

  File file = SPIFFS.open(filename, "r");
  if (!file) 
  {
    Serial.println(F("failed to open file"));
    return false;
  }
  
  if (file.size() == 0)
  {
    Serial.println(F("empty"));
    return false;
  }

  Serial.print(file.size());
  Serial.println(F(" bytes read"));
  
  DeserializationError error = deserializeJson(*json, file);
  if (error) 
  {
    Serial.print(F("[erro] failed to deserialise JSON: "));
    Serial.println(error.f_str());
    return false;
  }
  
  return json->isNull() ? false : true;
#else
  return false;
#endif
}

boolean OXRS_MQTT::_saveJson(DynamicJsonDocument * json, const char * filename)
{
#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266)
  Serial.print(F("[file] writing "));
  Serial.print(filename);
  Serial.print(F("..."));

  File file = SPIFFS.open(filename, "w");
  if (!file) 
  {
    Serial.println(F("failed to open file"));
    return false;
  }

  Serial.print(serializeJson(*json, file));
  Serial.println(F(" bytes written"));
  return true;
#else
  return false;
#endif
}

int OXRS_MQTT::_connect(void)
{
  // Set the broker address and port (in case they have changed)
  _client->setServer(_broker, _port);

  // Get our LWT topic
  char topic[64];
  sprintf_P(topic, PSTR("%s/%s"), getStatusTopic(topic), MQTT_LWT_SUFFIX);
  
  // Attempt to connect to the MQTT broker
  boolean success = _client->connect(_clientId, _username, _password, topic, MQTT_LWT_QOS, MQTT_LWT_RETAIN, MQTT_LWT_OFFLINE);
  if (success)
  {
    // Publish our 'online' message so anything listening is notified
    _client->publish(topic, MQTT_LWT_ONLINE, MQTT_LWT_RETAIN);

    // Subscribe to our config and command topics
    _client->subscribe(getConfigTopic(topic));
    _client->subscribe(getCommandTopic(topic));
  }

  return _client->state();
}

void OXRS_MQTT::_reconnect(void)
{
  // Disconnect from MQTT broker
  _client->disconnect();
  
  // Force a connect attempt immediately
  _backoff = 0;
  _lastReconnectMs = millis();
}

void OXRS_MQTT::_handlePayload(const char * topicType, DynamicJsonDocument * json)
{
  if (json->is<JsonArray>())
  {
    JsonArray array = json->as<JsonArray>();
    for (JsonVariant v : array)
    {
      _fireCallback(topicType, v.as<JsonObject>());
    }
  }
  else
  {
    _fireCallback(topicType, json->as<JsonObject>());
  }
}

void OXRS_MQTT::_fireCallback(const char * topicType, JsonObject json)
{
  // Forward to the appropriate callback
  if (strncmp(topicType, MQTT_CONFIG_TOPIC, 4) == 0)
  {
    if (_onConfig) 
    {
      _onConfig(json);
    }
    else
    {
      Serial.println(F("[warn] no config handler, ignoring message"));
    }
  }
  else if (strncmp(topicType, MQTT_COMMAND_TOPIC, 4) == 0)
  {
    if (_onCommand)
    {
      _onCommand(json);
    }
    else
    {
      Serial.println(F("[warn] no command handler, ignoring message"));
    }
  }
  else
  {
    Serial.println(F("[warn] invalid topic, ignoring message"));
  }  
}

boolean OXRS_MQTT::_publish(char * topic, JsonObject json)
{
  if (!_client->connected()) { return false; }
  
  char buffer[256];
  serializeJson(json, buffer);
  
  // Log each published message to serial for debugging
  Serial.print(F("[mqtt-tx] "));
  Serial.print(topic);
  Serial.print(F(" "));
  Serial.println(buffer);
  
  _client->publish(topic, buffer);
  return true;
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