#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <YoutubeApi.h>
#include <FacebookApi.h>
#include <Adafruit_NeoPixel.h>
#include "Config.h"

//NodeMCU pins definition
#define D0 16
#define D1 5
#define D2 4
#define D3 0
#define D4 2
#define D5 14
#define D6 12
#define D7 13
#define D8 15
#define D9 3
#define D10 1

#define mediaCount 4
#define mediaDurationDefaultValue 4

#define refreshInterval 2000

#define ledPin D8//Connect ws2811 LED matrix with NodeMCU pin number D8
#define ledAmount 320
#define brightnessDefaultValue 5

#define settingsResetPin D7
#define settingsResetGndPin D6

#define pannelHeight 8
#define pannelWidth 40

#define eepromCheckValue 123

ESP8266WebServer server(80);
WiFiClientSecure client;

FacebookApi facebookApi(client, facebookAccessToken, facebookAppId, facebookAppSecret);
YoutubeApi youtubeApi(youtubeApiKey, client);

Adafruit_NeoPixel bande = Adafruit_NeoPixel(ledAmount, ledPin, NEO_GRB + NEO_KHZ800);

const String mediaName[mediaCount] = {"YouTube", "Twitter", "Facebook", "Instagram"};
unsigned int mediaDuration[mediaCount] = {mediaDurationDefaultValue, mediaDurationDefaultValue, mediaDurationDefaultValue, mediaDurationDefaultValue};
const unsigned int mediaCallLimits[mediaCount] = {0, 0, 0, 200}; // Limite de calls à l'API en calls/h
unsigned long mediaLastCallMillis[mediaCount];
unsigned long mediaLastValue[mediaCount];
bool firstCallDone[mediaCount];
bool mediaEnabled[mediaCount];

byte pannel[pannelHeight][pannelWidth][3];

byte brightness = brightnessDefaultValue;

byte previousMedia = 0;
unsigned long previousNumber = 0;







long power(long base, long exponent)
{
  long result = base;
  
  for(long i = 0; i <= exponent; i++)
  {
    if(i == 0)
    {
      result = 1;
    }
    else
    {
      result = result * base;
    }
  }

  return result;
}




int getFacebookFanCount(String pageId)
{
  return facebookApi.getPageFanCount(pageId);
}


void printMediaSettings()
{
  for(int media = 0; media < mediaCount; media++)
  {
    Serial.print(mediaName[media]);
    Serial.print(" - Enabled : ");
    Serial.print(mediaEnabled[media]);
    Serial.print(" - Duration : ");
    Serial.println(mediaDuration[media]);
  }
}



String generateIndexHtml(String message = "")
{
  String html = "<!DOCTYPE html>\n<html>\n<head>\n<meta charset=\"UTF-8\">\n<title>Follower Counter</title>\n";
  html += "</head>\n<body>\n";
  html += message + "\n<br>\n";
  html += "<form action=\"/index\" method=\"get\">Brightness % <input type=\"number\" min=\"0\" max=\"100\" name=\"brightness\" value=\"" + String(brightness) + "\"><input type=\"submit\" value=\"Apply\"></form>";
  html += "<table class=\"table\">\n";
  html += "<tr><td>Media</td><td>Enabled</td><td>Duration</td></tr>";

  for(int media = 0; media < mediaCount; media++)
  {
    html += "<tr>\n<form action=\"/index\" method=\"get\">\n<input type=\"hidden\" name=\"media\" value=\"" + String(media) + "\">\n";
    html += "<td>" + mediaName[media] + "</td>\n";
    html += "<td><input type=\"hidden\" name=\"enabled\" value=\"" + String(mediaEnabled[media] ? "true" : "false") + "\" checked>";
    html += "<input type=\"checkbox\" onclick=\"this.previousSibling.value = !(this.previousSibling.value == 'true')\" " + String(mediaEnabled[media] ? "checked" : "") + "></td>\n";
    html += "<td><input type=\"number\" name=\"duration\" value=\"" + String(mediaDuration[media]) + "\"></td>\n";
    html += "<td><input type=\"submit\" value=\"Apply\"></td>\n";
    html += "</form>\n</tr>\n";
  }

  html += "</table>\n";
  html += "<button onClick=\"window.location = window.location.pathname\">Refresh</button>";
  html += "\n</body>\n</html>";

  return html;
}

void handleIndex()
{
  if (!server.authenticate(webServerUsername, webServerPassword))
  {
    return server.requestAuthentication();
  }
  
  String message = "";
  if (server.arg("media") != "" && server.arg("enabled") != "")
  {
    int media = server.arg("media").toInt();
    bool enabled = server.arg("enabled") == "true";
    
    if(media >= 0 && media < mediaCount)
    {
      if(server.arg("duration") != "")
      {
        int duration = server.arg("duration").toInt();
        if(duration > 0)
        {
          mediaDuration[media] = duration;
          mediaEnabled[media] = enabled;

          writeMediaSettingsToEeprom(media);

          message = mediaName[media] + " settings changed.";
        }
        else
        {
          message = "Incorrect query";
        }
      }
    }
  }
  else if (server.arg("brightness") != "")
  {
    int newBrightness = server.arg("brightness").toInt();
    if(newBrightness >= 0 && newBrightness <= 100) // La vraie limite max de brightness est 255, mais le courant nécessaire devient trop élevée pour un port USB
    {
      brightness = newBrightness;

      bande.setBrightness(brightness);
      bande.show();

      writeBrightnessSettingToEeprom();

      message = "Brightness changed.";
    }
  }

  server.send(200, "text/html", generateIndexHtml(message));
}

void setup() {
  Serial.begin(115200);
  delay(10);

  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.hostname(wirelessHostname);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  
  ArduinoOTA.setPassword(otaPassword);
  
  ArduinoOTA.begin();

  server.on("/index", handleIndex);
  server.begin();

  pinMode(settingsResetPin, INPUT_PULLUP);
  pinMode(settingsResetGndPin, OUTPUT);

  digitalWrite(settingsResetGndPin, LOW);
  
  EEPROM.begin(512);

  if(EEPROM.read(0) == eepromCheckValue && digitalRead(settingsResetPin))
  {
    readSettingsFromEeprom();
  }
  else
  {
    for(int i = 0; i < mediaCount; i++)
    {
      mediaEnabled[i] = true;
    }

    writeSettingsToEeprom();
  }
  
  printMediaSettings();
  Serial.println();

  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  bande.begin();

  bande.setBrightness(brightness);

  printLogoWithAnimation(YtLogo, YtLogoColors);

  print6DigitsNumberWithAnimation(0, 11, 0, 255, 255, 255, 1);

  refreshDisplay();
}

void printMediaLogoWithAnimation(int media)
{
  if(media == 0)
  {
    printLogoWithAnimation(YtLogo, YtLogoColors);
  }
  else if(media == 1)
  {
    printLogoWithAnimation(TwLogo, TwLogoColors);
  }
  else if(media == 2)
  {
    printLogoWithAnimation(FbLogo, FbLogoColors);
  }
  else if(media == 3)
  {
    printLogoWithAnimation(InstaLogo, InstaLogoColors);
  }

  return;
}

int getMediaValue(int media)
{
  int value = -1;

  if(mediaCallLimits[media] != 0)
  {
    unsigned long minTimeBetweenCalls = 3600000 / mediaCallLimits[media];
    if(millis() - mediaLastCallMillis[media] < minTimeBetweenCalls && firstCallDone[media])
    {
      value = mediaLastValue[media];
      Serial.print("Skipped ");
      Serial.print(mediaName[media]);
      Serial.println(" call for API restriction. Last known value used.");
      return value;
    }
  }

  Serial.print(mediaName[media]);
  Serial.println(" API call.");
  
  if(media == 0)
  {
    value = getYoutubeSubscriberCount(youtubeChannelId);
  }
  else if(media == 1)
  {
    value = getTwitterFollowerCount(twitterPageName);
  }
  else if(media == 2)
  {
    value = getFacebookFanCount(facebookPageId);
  }
  else if(media == 3)
  {
    value = getInstagramFollowerCount(instagramPageId, instagramAccessToken);
  }

  firstCallDone[media] = true;
  mediaLastCallMillis[media] = millis();
  mediaLastValue[media] = value;
  
  return value;
}

void delayWithHandling(long ms)
{
  unsigned long delayStartMillis = millis();

  while(millis() - delayStartMillis < ms)
  {
    delay(50);
    ArduinoOTA.handle();
    server.handleClient();
  }
}

void loop()
{
  ArduinoOTA.handle();
  server.handleClient();

  for(int media = 0; media < mediaCount; media++)
  {
    if(!mediaEnabled[media])
    {
      continue; // Le média n'est pas activé
    }
    else
    {
      bool logoDisplayed = previousMedia == media;
      
      previousMedia = media;

      unsigned long mediaStartMillis = millis();
      unsigned long lastRefreshMillis = 0;
      
      while(millis() - mediaStartMillis < mediaDuration[media] * 1000)
      { 
        if(!mediaEnabled[media])
        {
          break; // Le média n'est pas activé
        }
        
        if(millis() - lastRefreshMillis >= refreshInterval)
        {
          lastRefreshMillis = millis();

          int value = getMediaValue(media);
          
          Serial.print(mediaName[media]);
          Serial.print(" : ");
          Serial.println(value);
          Serial.println();
  
          if(!logoDisplayed)
          {
            printMediaLogoWithAnimation(media);
            print6DigitsNumberWithAnimation(value, 11, 0, 255, 255, 255, 1);
            
            logoDisplayed = true;
          }
          else
          {
            print6DigitsNumberWithAnimation(value, 11, 0, 255, 255, 255);
          }
  
          previousNumber = value;
  
          refreshDisplay();
        }
  
        delayWithHandling(50);
      }
    }
  }
}
