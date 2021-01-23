#include "ArduinoMqttClient.h"

#include <ESP8266WiFi.h>
#include <Adafruit_NeoPixel.h>

#include "arduino_secrets.h"

#define BRKR_PORT  8883
#define BRKR_URL   "mqtt.flespi.io"

#define LED_ACTIVITY 2
#define LED_SIGNAL   0

WiFiClientSecure net;
MqttClient client(net);

int lastMillis = 0;
Adafruit_NeoPixel strip = Adafruit_NeoPixel(32, 15, NEO_GRB + NEO_KHZ800);
uint8_t lastProg = 0;

void showProgress(uint16_t p, uint32_t c)
{
	uint8_t prog = p*strip.numPixels()/100;
	for (uint8_t i = lastProg; i < prog; i++)
	{
		strip.setPixelColor(i, c);
		strip.show();
		delay(20);
	}

	lastProg = prog;

	if (p == 100)
		lastProg = 0;
}

void colorWipe(uint32_t c, uint8_t wait)
{
	for (uint16_t i = 0; i < strip.numPixels(); i++)
	{
		strip.setPixelColor(i, c);
		strip.show();
		delay(wait);
	}
}

void connect()
{
	Serial.print("Connecting to WiFi");
	while (WiFi.status() != WL_CONNECTED)
	{
		digitalWrite(LED_ACTIVITY, LOW);
		delay(200);
		digitalWrite(LED_ACTIVITY, HIGH);
		delay(200);
		Serial.print(".");
	}
	Serial.println();

	Serial.print("Connected, IP address: ");
	Serial.println(WiFi.localIP());
	showProgress(66, strip.Color(255, 255, 0));

	Serial.print("\nConnecting to broker");

	client.beginWill("nodes/AirSignal/infos/connection", false, 1);
	client.print("Node died!");
	client.endWill();

	while (!client.connect(BRKR_URL, BRKR_PORT))
	{
		digitalWrite(LED_ACTIVITY, LOW);
		delay(100);
		digitalWrite(LED_ACTIVITY, HIGH);
		delay(100);
		Serial.print(".");
		delay(800);
	}

	Serial.println("\nconnected!");
	showProgress(100, strip.Color(0, 255, 0));

	client.beginMessage("nodes/AirSignal/infos/connection");
	client.print("Node connected!");
	client.endMessage();

	client.subscribe("nodes/AirSignal/actors/#");

	colorWipe(strip.Color(0, 0, 0), 10); // Off
}

void setPixelXY(uint8_t x, uint8_t y, uint32_t c)
{
	if (x < 8 || y < 4) {
		strip.setPixelColor(x + 8 * y, c);
	}
}

void drawFrame(uint32_t c)
{
	for (uint8_t i = 0; i < 8; i++)
	{
		setPixelXY(i, 0, c);
		setPixelXY(i, 3, c);
	}

	setPixelXY(0, 1, c);
	setPixelXY(7, 1, c);
	setPixelXY(0, 2, c);
	setPixelXY(7, 2, c);

	strip.show();
}

void fillFrame(uint32_t c)
{
	for (uint8_t i = 1; i < 7; i++)
	{
		setPixelXY(i, 1, c);
		setPixelXY(i, 2, c);
	}

	strip.show();
}

void messageReceived(int messageSize)
{
	String topic = client.messageTopic();
	Serial.print("incoming: ");
	Serial.print(topic);
	Serial.print(" - ");

	unsigned char buf[messageSize+1];

	while(client.available()) {
		client.read(buf, messageSize);
	}
	buf[messageSize] = '\0';

	String payload(reinterpret_cast<char*>(buf));
	Serial.println(payload);

	digitalWrite(LED_ACTIVITY, LOW);
	delay(10);
	digitalWrite(LED_ACTIVITY, HIGH);

	if (topic.endsWith("LED"))
	{
		if (payload.equals("on"))
		{
			digitalWrite(LED_SIGNAL, LOW);
		}
		else if (payload.equals("off"))
		{
			digitalWrite(LED_SIGNAL, HIGH);
		}
		else
		{
			Serial.println(F("payload not suitable"));
		}
	}
	else if(topic.endsWith("mode"))
	{
		if (payload.equals("drainDirtWater"))
		{
			drawFrame(strip.Color(255, 128, 0));
			fillFrame(strip.Color(255, 0, 0));
		}
		else if (payload.equals("fillFreshWater"))
		{
			drawFrame(strip.Color(0, 0, 255));
			fillFrame(strip.Color(255, 0, 0));
		}
		else
		{
			Serial.println(F("payload not suitable"));
		}
	}
	else if(topic.endsWith("action"))
	{
		if (payload.equals("go")) {
			fillFrame(strip.Color(0, 255, 0));
		}
		else if (payload.equals("stop"))
		{
			fillFrame(strip.Color(255, 0, 0));
		}
		else
		{
			Serial.println(F("payload not suitable"));
		}
	}
}

void setup()
{
	Serial.begin(115200);
	Serial.println();
	Serial.println("AirSignal - MQTT-Client");
	Serial.println();

	strip.begin();
	strip.setBrightness(4);
	strip.show();

	showProgress(33, strip.Color(255, 0, 0));

	pinMode(LED_ACTIVITY, OUTPUT);
	digitalWrite(LED_ACTIVITY, HIGH);
	pinMode(LED_SIGNAL, OUTPUT);
	digitalWrite(LED_SIGNAL, HIGH);

	WiFi.hostname("HUZZAH");
	WiFi.setAutoReconnect(true);
	WiFi.begin(WIFI_SSID, WIFI_PWD);

	net.setInsecure(); // Root cert is not available

	client.onMessage(messageReceived);
	client.setUsernamePassword("", MQTT_TOKEN);
	client.setId("AirSignal");

	connect();
}

void loop()
{
	client.poll();
	delay(10);  // <- fixes some issues with WiFi stability

	if (!client.connected())
	{
		connect();
	}

	// publish a message roughly every 10 seconds.
	if (millis() - lastMillis > 10000)
	{
		lastMillis = millis();
		client.beginMessage("nodes/AirSignal/infos/ping");
		client.print("Ping...");
		client.endMessage();

		digitalWrite(LED_ACTIVITY, LOW);
		delay(10);
		digitalWrite(LED_ACTIVITY, HIGH);
	}
}
