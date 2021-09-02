/*
 * OXRS_MQTT.cpp
 * 
 */

#include "Arduino.h"
#include "OXRS_MQTT.h"

OXRS_MQTT::OXRS_MQTT(PubSubClient& client) 
{
  this->_client = &client;
}

void OXRS_MQTT::setClientId(const char * deviceId)
{ 
  strcpy(_clientId, deviceId);
}

void OXRS_MQTT::setClientId(const char * deviceType, byte deviceMac[6])
{ 
  sprintf_P(_clientId, PSTR("%s-%02x%02x%02x"), deviceType, deviceMac[3], deviceMac[4], deviceMac[5]);  
}

void OXRS_MQTT::setAuth(const char * username, const char * password)
{ 
  strcpy(_username, username);
  strcpy(_password, password);
}

void OXRS_MQTT::setTopicPrefix(const char * prefix)
{ 
  strcpy(_topicPrefix, prefix);
}

void OXRS_MQTT::setTopicSuffix(const char * suffix)
{ 
  strcpy(_topicSuffix, suffix);
}

void OXRS_MQTT::onConfig(callback callback)
{ 
  _onConfig = callback; 
}

void OXRS_MQTT::onCommand(callback callback)
{ 
  _onCommand = callback; 
}

void OXRS_MQTT::loop()
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
      Serial.print(F("Connecting to MQTT broker..."));

      if (_connect()) 
      {
        Serial.println(F("success"));

        char topic[64];
        Serial.println(F("MQTT topics..."));
        Serial.print(F(" [config]    <-- "));
        Serial.println(_getTopic(topic, MQTT_CONFIG_TOPIC));
        Serial.print(F(" [commands]  <-- "));
        Serial.println(_getTopic(topic, MQTT_COMMAND_TOPIC));
        Serial.print(F(" [status]    --> "));
        Serial.println(_getTopic(topic, MQTT_STATUS_TOPIC));
        Serial.print(F(" [telemetry] --> "));
        Serial.println(_getTopic(topic, MQTT_TELEMETRY_TOPIC));
      }
      else
      {
        // Reconnection failed, so backoff
        if (_backoff < MQTT_MAX_BACKOFF_COUNT) { _backoff++; }
        _lastReconnectMs = millis();

        Serial.print(F("failed, retry in "));
        Serial.print(_backoff * MQTT_BACKOFF_SECS);
        Serial.println(F("s"));
      }
    }
  }
}

void OXRS_MQTT::receive(char * topic, uint8_t * payload, unsigned int length) 
{
  Serial.print(F("[recv] "));
  Serial.print(topic);
  Serial.print(F(" "));
  if (length == 0)
  {
    Serial.println(F("null"));
  }
  else
  {
    for (int i; i < length; i++)
    {
      Serial.print((char)payload[i]);
    }
    Serial.println();
  }

  // Tokenise the topic (skipping any prefix) to get the root topic
  char * token;
  token = strtok(&topic[strlen(_topicPrefix)], "/");

  // Deserialise the payload
  StaticJsonDocument<256> json;
  deserializeJson(json, payload);
  
  // Forward to the appropriate callback
  if (strncmp(token, "conf", 4) == 0)
  {
    if (_onConfig) { _onConfig(json.as<JsonObject>()); }
  }
  else if (strncmp(token, "cmnd", 4) == 0)
  {
    if (_onCommand) { _onCommand(json.as<JsonObject>()); }
  }
  else
  {
    Serial.println(F("[erro] invalid topic, ignoring message"));
  }
}

boolean OXRS_MQTT::publishStatus(JsonObject json)
{
  return _publish(MQTT_STATUS_TOPIC, json);
}

boolean OXRS_MQTT::publishTelemetry(JsonObject json)
{
  return _publish(MQTT_TELEMETRY_TOPIC, json);
}

boolean OXRS_MQTT::_publish(const char * topicType, JsonObject json)
{
  if (!_client->connected()) { return false; }
  
  char topic[64];
  _getTopic(topic, topicType);

  char buffer[256];
  serializeJson(json, buffer);
  
  Serial.print(F("[send] "));
  Serial.print(topic);
  Serial.print(F(" "));
  Serial.println(buffer);
  
  _client->publish(topic, buffer);
  return true;
}

boolean OXRS_MQTT::_connect()
{
  // Get our LWT topic
  char topic[64];
  sprintf_P(topic, PSTR("%s/%s"), _getTopic(topic, MQTT_STATUS_TOPIC), MQTT_LWT_SUFFIX);
  
  // Attempt to connect to the MQTT broker
  boolean success = _client->connect(_clientId, _username, _password, topic, MQTT_LWT_QOS, MQTT_LWT_RETAIN, MQTT_LWT_OFFLINE);
  if (success)
  {
    // Publish our 'online' message so anything listening is notified
    _client->publish(topic, MQTT_LWT_ONLINE, MQTT_LWT_RETAIN);

    // Subscribe to our config and command topics
    _client->subscribe(_getTopic(topic, MQTT_CONFIG_TOPIC));
    _client->subscribe(_getTopic(topic, MQTT_COMMAND_TOPIC));
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