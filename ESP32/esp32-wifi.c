/*
	*  This sketch sends data via HTTP GET requests to data.sparkfun.com service.
	*
	*  You need to get streamId and privateKey at data.sparkfun.com and paste them
	*  below. Or just customize this script to talk to other HTTP servers.
	*
*/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <FacebookApi.h>

const char* ssid     = "PLKKPT";
const char* password = "LaranjaM3canica";

String FACEBOOK_ACCESS_TOKEN = "EAAd1X69JKZCgBAAFKu8w2OTZAZAbmBYDMCo1XGORhjHjaecupFLRles7gkNRKZByomdWdLi51RI3mAZBNjaOqFmG4niHOb7uwmk3xQnJ8Xa58oGj2xRpEA6LxpIWhq0pzXuNQkwmbAIZBdVKp6KmJTpgDOmsYEQpriSP6ZC0pCPlfjCDe9detCi6XVlU42N8WZBecfC52E9owAZDZD";    // not needed for the page fan count
String FACEBOOK_APP_ID = "2099378660125688";
String FACEBOOK_APP_SECRET = "05ad93087889652b86fcbfd90257e263";

WiFiClientSecure client;
FacebookApi *api;

unsigned long api_delay = 5 * 60000; //time between api requests (5mins)
unsigned long api_due_time;

void setup()
{
    Serial.begin(115200);
    delay(10);
	
    // We start by connecting to a WiFi network
	
    Serial.println();
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);
	
    WiFi.begin(ssid, password);
	
    while (WiFi.status() != WL_CONNECTED) 
	{
        delay(500);
        Serial.print(".");
	}
	
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
	

	
	api = new FacebookApi(client, FACEBOOK_APP_ID, FACEBOOK_APP_SECRET, FACEBOOK_ACCESS_TOKEN);
}


void loop()
{
    
	if (millis() > api_due_time) 
	{
		int pageLikes = api->getPageFanCount("293445158049843");
		if(pageLikes >= 0)
		{
			Serial.print("Facebook page likes: ");
			Serial.println(pageLikes);
		} 
		else 
		{
			Serial.println("Error getting likes");
		}
		api_due_time = millis() + api_delay;
	}
	
	
}
