// ESP32 only sketch (uses > 40kbytes RAM)

// Loads a PNG image from the internet with a specified URL,
// also loads a file from SPIFFS.
// Additionally, integrates OpenAI API for image generation.

// Use IDE to upload files to SPIFFS!

//#define USE_ADAFRUIT_GFX // Comment out to use TFT_eSPI

#define USE_LINE_BUFFER  // Enable for faster rendering

#define WIFI_SSID ""
#define WIFI_PASS ""
#define OPENAI_API_KEY "sk-****"


#if defined(USE_ADAFRUIT_GFX)
  #define TFT_CS  5
  #define TFT_DC  26
  #define TFT_RST 27
  #include <Adafruit_ILI9341.h>
  Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);
#else
  #include <TFT_eSPI.h>              // Hardware-specific library
  TFT_eSPI tft = TFT_eSPI();         // Invoke custom library
#endif

// Include SPIFFS
#define FS_NO_GLOBALS
#include <FS.h>
#include "SPIFFS.h" // Required for ESP32 only

#include <HTTPClient.h>
#include <WiFiClientSecure.h> // Required for secure connections
#include <ArduinoJson.h>      // Used for parsing OpenAI API response
#include "support_functions.h"

bool imageGenerated = false;

void setup()
{
  Serial.begin(115200);
  tft.begin();

  if(!SPIFFS.begin(true)){
    Serial.println("SPIFFS Mount Failed");
    return;
  }

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("\n WiFi connected.\n");

  // Generate an image using OpenAI API if not already generated
  if (!imageGenerated) {
    Serial.println("Generating image using OpenAI API...");
    String openaiImageUrl = generateImage();

    if (openaiImageUrl != "") {
      Serial.println("Image URL found: " + openaiImageUrl);

      // Download and save the generated image to SPIFFS
      if (downloadImageToSPIFFS(openaiImageUrl)) {
        // Load the image from SPIFFS and display it
        load_file(SPIFFS, "/generated_image.png");
        imageGenerated = true;
      } else {
        Serial.println("Failed to download and save the image.");
      }
    } else {
      Serial.println("Failed to generate image.");
    }
  }
}

void loop()
{
  // No need to repeatedly load images here, leave empty for now
  delay(10000); // Delay to reduce processor usage, adjust as necessary
}

String generateImage() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.setTimeout(15000); // Increase timeout to 15 seconds
    http.begin("https://api.openai.com/v1/images/generations");
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Bearer " OPENAI_API_KEY);

    String requestBody = "{\"model\": \"dall-e-2\", \"prompt\": \"a pegasus\", \"n\": 1, \"size\": \"256x256\"}";
    int httpResponseCode = http.POST(requestBody);

    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println(response);

      StaticJsonDocument<4096> jsonResponse; // Increase size to handle larger responses
      DeserializationError error = deserializeJson(jsonResponse, response);

      if (!error) {
        Serial.println("Parsing JSON response...");
        const char* imageUrl = jsonResponse["data"][0]["url"];
        http.end();
        return String(imageUrl);
      } else {
        Serial.println("Failed to parse JSON response");
      }
    } else {
      Serial.print("HTTP error: ");
      Serial.println(httpResponseCode);
    }

    http.end();
  } else {
    Serial.println("WiFi Disconnected");
  }
  return "";
}

bool downloadImageToSPIFFS(String url) {
  WiFiClientSecure client;
  client.setInsecure(); // Disable SSL certificate verification

  HTTPClient http;
  http.begin(client, url);
  int httpResponseCode = http.GET();

  if (httpResponseCode == HTTP_CODE_OK) {
    fs::File file = SPIFFS.open("/generated_image.png", FILE_WRITE);
    if (!file) {
      Serial.println("Failed to open file for writing");
      return false;
    }

    WiFiClient* stream = http.getStreamPtr();
    uint8_t buffer[128];
    size_t size = 0;

    while (http.connected() && (size = stream->available())) {
      size = stream->readBytes(buffer, sizeof(buffer));
      file.write(buffer, size);
    }

    file.close();
    http.end();
    Serial.println("Image successfully downloaded and saved to SPIFFS");
    return true;
  } else {
    Serial.print("Failed to download image, HTTP error: ");
    Serial.println(httpResponseCode);
    http.end();
    return false;
  }
}