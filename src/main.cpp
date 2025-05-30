// We will use wifi
#include <WiFiManager.h>
#include <WiFiClientSecure.h>
//#include <lvgl.h>
// We will use SPIFFS and FS
#include <SPIFFS.h>
#include <FS.h>
#include <TFT_eSPI.h>
#include <TFT_eWidget.h>
#include <XPT2046_Touchscreen.h>
// We use JSON as data format. Make sure to have the lib available
#include <ArduinoJson.h>
// Working with c++ strings
#include <string>
#include <base64.h>
// Includes for the server
#include <HTTPSServer.hpp>
#include <HTTPClient.h>
#include <SSLCert.hpp>
//#include <HTTPRequest.hpp>
//#include <HTTPResponse.hpp>
//#include <util.hpp>

// Include the jpeg decoder library
#include <JPEGDEC.h>
//#include <TJpg_Decoder.h> 
#include "Free_Fonts.h"
#include "Preferences.h"
//we may be able to trim some fat here if ESP has built in time tools that cant spit out time_t 
#include <time.h>
// Define the name of the directory for public files in the SPIFFS parition
#define DIR_PUBLIC "/public"
// Spotify API credentials
// #define CLIENT_ID "b7aaed0542244cd2a065d335dad714b3"
// #define CLIENT_SECRET "73237d02d2b34061956bccfafc6e50c5"
// TODO: Configure your WiFi here
#define WIFI_SSID "2WIRE471"
#define WIFI_PSK  "9573436692"


String CLIENT_ID = "";//"b7aaed0542244cd2a065d335dad714b3";
String CLIENT_SECRET = "";//"73237d02d2b34061956bccfafc6e50c5";


// We need to specify some content-type mapping, so the resources get delivered with the
// right content type and are displayed correctly in the browser
char contentTypes[][2][32] = {
  {".html", "text/html"},
  {".css",  "text/css"},
  {".js",   "application/javascript"},
  {".json", "application/json"},
  {".png",  "image/png"},
  {".jpg",  "image/jpg"},
  {"", ""}
};


// The HTTPS Server comes in a separate namespace. For easier use, include it here.
using namespace httpsserver;

SSLCert * getCertificate();

void handleSPIFFS(HTTPRequest * req, HTTPResponse * res);
void handleCurrent(HTTPRequest * req, HTTPResponse * res);
void handleGetEvents(HTTPRequest * req, HTTPResponse * res);
void handleSpotifyCallback(HTTPRequest * req, HTTPResponse * res);
void handleDevSetup(HTTPRequest * req, HTTPResponse * res);

// We just create a reference to the server here. We cannot call the constructor unless
// we have initialized the SPIFFS and read or created the certificate
HTTPSServer * secureServer;
const char* _developer_credentials_page PROGMEM = R"=====(
<html>
  <head>
      <title>ESP Spotify Login</title>
    </head>
    <body>
      <center>
        <h1>Enter Dev Credentials</h1>      
        <form action='/dev' method='post'> 
          <div>
              <label>Client Id</label> <input type="text" name="clientID" id="clientId" style="width: 80px;margin-left: 25px;margin-bottom: 10px;">
          </div>
          <div>
              <label>Client Secret</label> <input type="text" name="clientSecret" id="clientSecret" style="width: 80px;margin-bottom: 20px;">
          </div>
          <div>
              <input type="submit" value="Save">
          </div>
        </form>
      </center>    
  </body>
  </html>
  )=====";


const char* _login_page PROGMEM = R"=====(
  <HTML>
    <HEAD>
      <TITLE>ESP Spotify Login</TITLE>
    </HEAD>
    <BODY>
      <CENTER>
        <H1>Spotify Login</H1>
        <a href="https://accounts.spotify.com/authorize?response_type=code&client_id=%s&redirect_uri=https://%s/callback&scope=ugc-image-upload playlist-read-collaborative playlist-modify-private playlist-modify-public playlist-read-private user-read-playback-position user-read-recently-played user-top-read user-modify-playback-state user-read-currently-playing user-read-playback-state user-read-private user-read-email user-library-modify user-library-read user-follow-modify user-follow-read streaming app-remote-control">Log in to spotify</a>
      </CENTER>
    </BODY>
  </HTML>
  )=====";


// Spotify API constants
const char* SPOTIFY_API_ENDPOINT = "api.spotify.com";
const char* SPOTIFY_TOKEN_ENDPOINT = "accounts.spotify.com";
const char* SPOTIFY_TOKEN_URL = "/api/token";
const int SPOTIFY_PORT = 443;
//bool codeReceived = false;

TFT_eSPI tft = TFT_eSPI();
#define BUFFER_SIZE (TFT_WIDTH * 10)
#define XPT2046_IRQ 36   // T_IRQ
#define XPT2046_MOSI 32  // T_DIN
#define XPT2046_MISO 39  // T_OUT
#define XPT2046_CLK 25   // T_CLK
#define XPT2046_CS 33    // T_CS
#define LCD_BACKLIGHT_PIN 21
#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 320

#define DEFAULT_CAPTIVE_SSID "SpotifyRemoteAP"
#define UPDATE_INTERVAL 600000UL  // 10 minutes

// Button dimensions and spacing
#define BUTTON_HEIGHT 50
#define BUTTON_MARGIN 10  // Space from bottom and sides
#define BUTTON_SPACING 10 // Space 

//BEGIN IMPORT OF NEW DOWNLOAD AND JPG STUFF


  // Global variables for positioning
int g_display_x_offset = 0;
int g_display_y_offset = 20;
int g_scale_multiplier = 1;

// Buffer size for downloading - larger buffer = faster download
const size_t DOWNLOAD_BUFFER_SIZE = 1024;//4096;
JPEGDEC jpg;

int JPEGDraw(JPEGDRAW *pDraw) {
  // Fix color format - swap red and blue channels
  // uint16_t *pixels = (uint16_t*)pDraw->pPixels;
  // int pixelCount = pDraw->iWidth * pDraw->iHeight;
  
  // for (int i = 0; i < pixelCount; i++) {
  //   uint16_t pixel = pixels[i];
  //   uint16_t r = (pixel & 0xF800) >> 11;
  //   uint16_t g = (pixel & 0x07E0);
  //   uint16_t b = (pixel & 0x001F) << 11;
  //   pixels[i] = b | g | r;
  // }
  
  // Apply centering offset to EVERY pixel block
  int centered_x = pDraw->x + g_display_x_offset;
  int centered_y = pDraw->y + g_display_y_offset;
  
  // Debug: print first few draw calls to see what's happening
  static int draw_count = 0;
  if (draw_count < 3) {
    Serial.printf("Draw call %d: pDraw->x=%d, pDraw->y=%d, offset=(%d,%d), final=(%d,%d)\n", 
                  draw_count, pDraw->x, pDraw->y, g_display_x_offset, g_display_y_offset, centered_x, centered_y);
    draw_count++;
  }
  
  // Always draw with the offset - don't check bounds, let TFT library handle clipping
  tft.pushImage(centered_x, centered_y, pDraw->iWidth, pDraw->iHeight, pDraw->pPixels);
  
  return 1;
}

// Function to display JPEG from SPIFFS
//Im not sure if i need this because of displayJPEGScaled?
//look for what we use as the callback in setup to make sure
void displayJPEG(const char *filename, int xpos, int ypos) {
  Serial.printf("Displaying JPEG: %s\n", filename);
  
  // Check if file exists
  if (!SPIFFS.exists(filename)) {
    Serial.println("File not found in SPIFFS!");
    tft.setCursor(10, 10);
    tft.setTextColor(TFT_RED);
    tft.println("album.jpg not found!");
    return;
  }
  
  // Open the file
  File jpegFile = SPIFFS.open(filename, "r");
  if (!jpegFile) {
    Serial.println("Failed to open file!");
    return;
  }
  
  // Get file size
  size_t fileSize = jpegFile.size();
  Serial.printf("File size: %d bytes\n", fileSize);
  
  // Read file into buffer
  uint8_t *jpegBuffer = (uint8_t*)malloc(fileSize);
  if (!jpegBuffer) {
    Serial.println("Failed to allocate memory!");
    jpegFile.close();
    return;
  }
  
  jpegFile.read(jpegBuffer, fileSize);
  jpegFile.close();
  
  // Open and decode the JPEG from memory buffer
  if (jpg.openRAM(jpegBuffer, fileSize, JPEGDraw)) {
    Serial.printf("Image size: %d x %d\n", jpg.getWidth(), jpg.getHeight());
    
    // Calculate scaling if image is larger than screen
    int img_width = jpg.getWidth();
    int img_height = jpg.getHeight();
    
    // Optional: Center the image
    int display_x = xpos;
    int display_y = ypos;
    
    if (img_width < SCREEN_WIDTH) {
      display_x = (SCREEN_WIDTH - img_width) / 2;
    }
    if (img_height < SCREEN_HEIGHT) {
      display_y = (SCREEN_HEIGHT - img_height) / 2;
    }
    
    // Decode and display
    jpg.decode(display_x, display_y, 0); // 0 = no scaling
    jpg.close();
    
    Serial.println("JPEG displayed successfully!");
  } else {
    Serial.println("Failed to open JPEG from RAM!");
    tft.setCursor(10, 10);
    tft.setTextColor(TFT_RED);
    tft.println("Failed to decode JPEG!");
  }
  
  // Free the allocated memory
  free(jpegBuffer);
}

void displayJPEGScaled(const char *filename, int screen_width, int screen_height, int y_position) {
  if (!SPIFFS.exists(filename)) {
    Serial.println("File not found!");
    tft.setCursor(10, 10);
    tft.setTextColor(TFT_RED);
    tft.println("album.jpg not found!");
    return;
  }
  
  // Clear screen first
  //tft.fillScreen(TFT_BLACK);
  
  File jpegFile = SPIFFS.open(filename, "r");
  if (!jpegFile) {
    Serial.println("Failed to open file!");
    return;
  }
  
  size_t fileSize = jpegFile.size();
  Serial.printf("File size: %d bytes\n", fileSize);
  
  uint8_t *jpegBuffer = (uint8_t*)malloc(fileSize);
  if (!jpegBuffer) {
    Serial.println("Memory allocation failed!");
    jpegFile.close();
    return;
  }
  
  jpegFile.read(jpegBuffer, fileSize);
  jpegFile.close();
  
  // First pass: get image dimensions without drawing
  if (jpg.openRAM(jpegBuffer, fileSize, nullptr)) {
    int img_width = jpg.getWidth();
    int img_height = jpg.getHeight();
    jpg.close();
    
    Serial.printf("Image dimensions: %dx%d\n", img_width, img_height);
    Serial.printf("Screen dimensions: %dx%d\n", screen_width, screen_height);
    
    // For 300x300 image on 320x240 screen:
    // Full size (300x300) is too tall for 240px height
    // 1/2 scale = 150x150 fits perfectly and looks good
    // 1/4 scale = 75x75 might be too small
    
    int scale_factor = 0; // Start with smallest
    int final_width = img_width / 8;
    int final_height = img_height / 8;
    
    // Try each scale to find the largest that fits both width AND height
    for (int test_scale = 3; test_scale >= 0; test_scale--) { // Start from largest
      int divisor = 8 >> test_scale; // 1, 2, 4, 8
      int test_width = img_width / divisor;
      int test_height = img_height / divisor;
      
      Serial.printf("Testing scale %d: %dx%d\n", test_scale, test_width, test_height);
      
      if (test_width <= screen_width && test_height <= screen_height) {
        scale_factor = test_scale;
        final_width = test_width;
        final_height = test_height;
        Serial.printf("Scale %d fits!\n", test_scale);
        break; // Found the largest that fits
      }
    }
    
    // Calculate positioning
    g_display_x_offset = (screen_width - final_width) / 2;  // Always center horizontally
    
    // Handle Y positioning based on parameter
    if (y_position == -1) {
      // Center vertically
      g_display_y_offset = (screen_height - final_height) / 2;
    } else {
      // Use specified Y position (top margin)
      g_display_y_offset = y_position;
    }
    
    Serial.printf("Selected scale factor %d (1/%d size)\n", scale_factor, 8 >> scale_factor);
    Serial.printf("Final size: %dx%d\n", final_width, final_height);
    Serial.printf("Centered at: (%d, %d)\n", g_display_x_offset, g_display_y_offset);
    
    // Second pass: actually draw the image
    if (jpg.openRAM(jpegBuffer, fileSize, JPEGDraw)) {
      // Force decode at 0,0 - our callback will handle positioning
      jpg.decode(0, 0, scale_factor);
      jpg.close();
      
      // Draw border for verification (green = success)
      tft.drawRect(g_display_x_offset - 1, g_display_y_offset - 1, 
                   final_width + 2, final_height + 2, TFT_GREEN);
      
      Serial.println("Image displayed and centered successfully!");
    } else {
      Serial.println("Second decode failed!");
    }
  } else {
    Serial.println("Failed to open JPEG!");
  }
  
  free(jpegBuffer);
}

size_t getImageSize(const String& fileName) {
    if (SPIFFS.exists("/" + fileName)) {
        File file = SPIFFS.open("/" + fileName, FILE_READ);
        size_t size = file.size();
        file.close();
        return size;
    }
    return 0;
}

//END IMPORT OF NEW STUFF

// Create an array of button instances to use in for() loops
// This is more useful where large numbers of buttons are employed
ButtonWidget btnRwd = ButtonWidget(&tft);
ButtonWidget btnPlay = ButtonWidget(&tft);
ButtonWidget btnFwd = ButtonWidget(&tft);

ButtonWidget* btn[] = {&btnRwd , &btnPlay, &btnFwd};;
uint8_t buttonCount = sizeof(btn) / sizeof(btn[0]);

SPIClass touchscreenSPI = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);
//think this is for lvgl, uncomment if things get weird
//uint32_t draw_buf[DRAW_BUF_SIZE / 4];
int x,y,z;

// Struct to store token response
struct SpotifyTokens {
  String authCode;
  String accessToken;
  String refreshToken;
  int expiresIn;
  String tokenType;
  String scope;
  unsigned long obtainedAt; // Timestamp when token was obtained
};

struct SpotifyRequest{
  String Method;
  String Request;
};

struct SpotifyDevice{
 String id;            
 bool is_active;
 String name;
 bool SupportsVol;
 int VolLevel;            
};

struct SpotifyTrack{
String CurrentTrackId ="";
String ArtistName;
String TrackName;
String ImageLink;
bool CurrentlyPlaying = false;
bool TrackChanged;
bool InfoLoaded;
};



// NTP server configuration
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -6 * 3600;  // Adjust for your timezone (CST = -6)
const int daylightOffset_sec = 3600;   // Daylight saving time offset

//we can probably get rid of this, but lets keep for now to see what we need and what we dont
struct TokenData {
  time_t retrievedAt;        // Unix timestamp when token was retrieved
  unsigned long expiresIn;   // Token lifetime in seconds
  bool isValid;              // Flag to indicate if stored data is valid
  String accessToken;
  String refreshToken; 
};

// Global variable to store tokens
SpotifyTokens spotifyTokens;
SpotifyRequest spotifyRequest;
SpotifyDevice spotifyDevice;
SpotifyTrack spotifyTrack;

DynamicJsonDocument spotifyResponse(2048);

String REDIRECT_URI ="";

int imageOffsetX = 26, imageOffsetY = 20;

static Preferences prefs;
time_t now = time(nullptr);
void initializeTime() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
 
  //not sure if this is needed?
  // Serial.print("Waiting for NTP time sync");
  // time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {    
    Serial.print(".");
    now = time(nullptr);
  }
  // Serial.println();
  // Serial.printf("Current time: %s", ctime(&now));
}

// // This next function will be called during decoding of the jpeg file to
// // render each block to the TFT.  If you use a different TFT library
// // you will need to adapt this function to suit.
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap)
{
  // Stop further decoding as image is running off bottom of screen
  if ( y >= tft.height() ) return 0;

  // This function will clip the image block rendering automatically at the TFT boundaries
  tft.pushImage(x, y, w, h, bitmap);
 
  // String imageLink = "";
  
  // bool loaded_ok = getFile(imageLink.substring(1,imageLink.length()-1).c_str(), "/albumArt.jpg"); // Note name preceded with "/"
  // Serial.println("Image load was: ")

  // Return 1 to decode next block
  return 1;
}

// Function to download and display an image from URL
// bool getFile(String url, String filename) {

/**
 * Encodes a string in Base64 format
 */
String base64Encode(const String &input) {
  const char* base64_chars = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";
    
  unsigned char* bytes_to_encode = (unsigned char*)input.c_str();
  unsigned int in_len = input.length();
  String encoded;
  
  int i = 0;
  int j = 0;
  unsigned char char_array_3[3];
  unsigned char char_array_4[4];
  
  while (in_len--) {
    char_array_3[i++] = *(bytes_to_encode++);
    if (i == 3) {
      char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
      char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
      char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
      char_array_4[3] = char_array_3[2] & 0x3f;
      
      for(i = 0; i < 4; i++) {
        encoded += base64_chars[char_array_4[i]];
      }
      i = 0;
    }
  }
  
  if (i) {
    for(j = i; j < 3; j++) {
      char_array_3[j] = '\0';
    }
    
    char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
    char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
    char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
    
    for (j = 0; j < i + 1; j++) {
      encoded += base64_chars[char_array_4[j]];
    }
    
    while((i++ < 3)) {
      encoded += '=';
    }
  }
  
  return encoded;
}

struct songDetails{
    int durationMs;
    String album;
    String artist;
    String song;
    String Id;
    bool isLiked;    
};

class SpotConn {
public:
	SpotConn(){
        client = WiFiClientSecure();
        client.setInsecure();
    }
    
    int JPEGDrawCentered(JPEGDRAW *pDraw) {
      // Fix color format - swap red and blue channels
      uint16_t *pixels = (uint16_t*)pDraw->pPixels;
      int pixelCount = pDraw->iWidth * pDraw->iHeight;
      
      for (int i = 0; i < pixelCount; i++) {
        uint16_t pixel = pixels[i];
        uint16_t r = (pixel & 0xF800) >> 11;
        uint16_t g = (pixel & 0x07E0);
        uint16_t b = (pixel & 0x001F) << 11;
        pixels[i] = b | g | r;
      }
      
      // Apply our centering offset
      int display_x = pDraw->x + g_display_x_offset;
      int display_y = pDraw->y + g_display_y_offset;
      
      // Only draw if within screen bounds
      if (display_x >= 0 && display_y >= 0 && 
          display_x < SCREEN_WIDTH && display_y < SCREEN_HEIGHT) {
        tft.pushImage(display_x, display_y, pDraw->iWidth, pDraw->iHeight, pDraw->pPixels);
      }
      
      return 1;
    }

    void LoadDevice()
    {
      spotifyRequest.Method = "GET";
      spotifyRequest.Request = "/v1/me/player/devices HTTP/1.1";

      Serial.println("loading device request start");

      if(makeSpotifyApiRequestASDOC2(spotifyRequest, SPOTIFY_API_ENDPOINT, "") )
      {
            
        Serial.println("loading device array");

        JsonArray devices = spotifyResponse["devices"].as<JsonArray>();

        Serial.println("loaded device array");

        int deviceCount = devices.size();
        Serial.println(deviceCount);

        for (int i = 0; i < deviceCount; i++) {
            
            Serial.print(i+1);
            Serial.print(". ");
            Serial.println(devices[i]["type"].as<String>());      
            
            if (devices[i]["type"].as<String>() == "Computer")
            {
                
                spotifyDevice2.id = devices[i]["id"].as<String>();
                
                Serial.print("Device ID: ");
                Serial.println(spotifyDevice.id);

                spotifyDevice2.is_active = devices[i]["is_active"].as<bool>();
                spotifyDevice2.name = devices[i]["name"].as<String>();
                spotifyDevice2.SupportsVol = devices[i]["supports_volume"].as<bool>();
                spotifyDevice2.VolLevel = devices[i]["volume_percent"].as<int>(); 
                deviceActive = true;
                noDeviceDrawn = false; // reset this in case we lose an active device so the message can display again
            }
        }
        spotifyResponse.clear();
        devicesLoaded = true;
        //return true;
      }
      else{
          //only should reach this if request fails, so try again?
          spotifyResponse.clear();
          deviceActive = false;
          devicesLoaded = false;
          //return false;
      } 
    }
    void drawNoDeviceActive(){
        tft.fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT - 60 , TFT_BLACK);                
        tft.setTextDatum(MC_DATUM);
        tft.setTextWrap(true);
        tft.setCursor(tft.width()/2,tft.height() / 2);
        tft.println("Press Play on App or PC");
    }
    bool getAccessToken(String code) {
          
      
        Serial.println("Connecting to Spotify API...");
    
        // Connect to Spotify API server
        if (!client.connect(SPOTIFY_TOKEN_ENDPOINT, SPOTIFY_PORT)) {
          Serial.println("Connection to Spotify API failed!");
          return false;
        }
        
        Serial.println("Connected to Spotify API");
        Serial.println("LOGIN INFO:");
        Serial.println("Client id");
        //Serial.println(clientId);
        Serial.println("Client secret");
        //Serial.println(clientSecret);
        //Serial.println(code);
        // Prepare request body
        // Note: Spotify token endpoint expects application/x-www-form-urlencoded content
        String requestBody = "grant_type=authorization_code";
        requestBody += "&code=" + code;
        requestBody += "&redirect_uri=https://192.168.1.125/callback";// + urlEncode(redirectUri);
        
        //we need to figure out how to store the client id and secret in a way that the user wont have to modify code by hand
        String authString = String(CLIENT_ID) + ":" + String(CLIENT_SECRET);
        String authHeader = "Authorization: Basic " + base64Encode(authString); 

        Serial.println("AuthHeader = " + authHeader);
        
        // Make the POST request
        client.println("POST " + String(SPOTIFY_TOKEN_URL) + " HTTP/1.1");
        client.println("Host: " + String(SPOTIFY_TOKEN_ENDPOINT));
        client.println("Content-Type: application/x-www-form-urlencoded");
        client.println("Content-Length: " + String(requestBody.length()));
        client.println(authHeader);
        client.println("Connection: close");
        client.println();
        client.println(requestBody);
        
        Serial.println("Request sent, waiting for response...");
        
        // Wait for response
        unsigned long timeout = millis();
        while (client.available() == 0) {
          if (millis() - timeout > 10000) {
            Serial.println("Request timeout!");
            client.stop();
            return false;
          }
        }
        
        // Skip HTTP headers
        char endOfHeaders[] = "\r\n\r\n";
        if (!client.find(endOfHeaders)) {
          Serial.println("Invalid response!");
          return false;
        }
        
        // Parse JSON response
        // Use a dynamic buffer for ArduinoJson
        DynamicJsonDocument doc(2048); // Adjust size based on expected response
        
        DeserializationError error = deserializeJson(doc, client);
        client.stop();
        
        if (error) {
          Serial.print("JSON parsing failed: ");
          Serial.println(error.c_str());
          
          Serial.println("Request: GetAuthCode" );

          return false;
        }
        
        // Check for error response
        if (doc.containsKey("error")) {
          Serial.print("Spotify API error: ");
          Serial.println(doc["error"].as<String>());
          if (doc.containsKey("error_description")) {
            Serial.println(doc["error_description"].as<String>());
          }
          return false;
        }
        authCode = code;
        accessToken = doc["access_token"].as<const char*>();
        Serial.print("Refresh Token from Callback :");
        refreshToken = doc["refresh_token"].as<String>();
        Serial.println(refreshToken);
        tokenExpireTime = doc["expires_in"].as<ulong>(); //"expires_in": 3600 () this is in seconds
        tokenStartTime = millis();
        

        saveTokenData(tokenExpireTime, accessToken, refreshToken);
        
        // Serial.println("Token Start Time: " + String(tokenStartTime));
        // prefs.putString("apiAccessToken", accessToken);
        // prefs.putString("apiRefreshToken", refreshToken);
        // prefs.putInt("apiStartTime", tokenStartTime);
        // prefs.putInt("apiExpireTime", tokenExpireTime);
        
        accessTokenSet = true;

        Serial.println(accessToken);
        Serial.println(refreshToken);
        
        if (doc.containsKey("scope")) {
          scope = doc["scope"].as<String>();
        }
        
        // Store timestamp when token was obtained
        spotifyTokens.obtainedAt = millis();
        
        Serial.println("Successfully retrieved Spotify access token!");
        Serial.print("Token expires in: ");
        Serial.print(spotifyTokens.expiresIn);
        Serial.println(" seconds");
        
        //tokenExchanged = true;

        return accessTokenSet;
      }
    void setAccessToken()//(String accToken, String refToken, int exptStartTime, int expTime)
    {
      //if tokens are loaded from memory assume they are good, 
      //but really we need to check somehow to see if it should be refreshed.
        TokenData token = loadTokenData();
        Serial.println("Token Data Set");
        Serial.println("Access Token = "  + token.accessToken);
        Serial.println("Refresh Token = " + token.refreshToken);

        accessToken = token.accessToken;
        refreshToken = token.refreshToken;
        tokenStartTime = token.retrievedAt;
        tokenExpireTime = token.expiresIn;
        
        //this seems to be calculating wrong, so lets revist this later when we tighten up stored token logic
        // if((millis() - tokenStartTime)/1000 > tokenExpireTime)
        // {
          Serial.print("Token start time: ");
          Serial.println(tokenStartTime);
          Serial.print("Token Exp Time: ");
          Serial.println(tokenExpireTime);
          Serial.print("Calculatation: ");
          Serial.println("Access Token: " + token.accessToken);
          Serial.println("Refresh Token: " + token.refreshToken);
        //   Serial.println((millis() - tokenStartTime)/1000);

        //   accessTokenSet = false;
        //   createRefreshToken();
        // }
        // else
        // {
          Serial.println("Access token set from memory");
          accessTokenSet = true;
        //}
    }
    bool refreshAccessToken(){
        // Check if we have a refresh token
      //TokenData token = loadTokenData();
        if (refreshToken.length() == 0) {
        Serial.println("No refresh token available!");
        return false;
      }
      
      // Create secure client
      WiFiClientSecure client;
      client.setInsecure();
      
      if (!client.connect(SPOTIFY_TOKEN_ENDPOINT, SPOTIFY_PORT)) {
        Serial.println("Connection to Spotify API failed!");
        return false;
      }
      
      // Prepare request body
      Serial.println("Applying Grant Type, Refresh Token =" + refreshToken);
      String requestBody = "grant_type=refresh_token&refresh_token=" + refreshToken;
            
      // Prepare authorization header
      //remember to switch this up to user configured values
      String authString = String(CLIENT_ID) + ":" + String(CLIENT_SECRET);
      String authHeader = "Authorization: Basic " + base64Encode(authString);
      
      // Make the POST request
      client.println("POST " + String(SPOTIFY_TOKEN_URL) + " HTTP/1.1");
      client.println("Host: " + String(SPOTIFY_TOKEN_ENDPOINT));
      client.println("Content-Type: application/x-www-form-urlencoded");
      client.println("Content-Length: " + String(requestBody.length()));
      client.println(authHeader);
      client.println("Connection: close");
      client.println();
      client.println(requestBody);
      
      // Wait for response
      unsigned long timeout = millis();
      while (client.available() == 0) {
        if (millis() - timeout > 10000) {
          Serial.println("Request timeout!");
          client.stop();
          return false;
        }
      }
      
      // Skip HTTP headers
      char endOfHeaders[] = "\r\n\r\n";
      if (!client.find(endOfHeaders)) {
        Serial.println("Invalid response!");
        return false;
      }
      
      // Parse JSON response
      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, client);
      client.stop();
      
      if (error) {
        Serial.print("JSON parsing failed: ");        
        Serial.println(error.c_str());

        Serial.println("Request: createRefreshToken");
       
        accessTokenSet = false;
        return accessTokenSet;
      }
      
      // Check for error response
      if (doc.containsKey("error")) {
        Serial.print("Spotify API error: ");
        Serial.println(doc["error"].as<String>());
        if (doc.containsKey("error_description")) {
          Serial.println(doc["error_description"].as<String>());
        }
        
        accessTokenSet = false;
        return accessTokenSet;
      }
      
      String newRefreshToken = "";
      // Update token information
      accessToken = doc["access_token"].as<String>();      
      newRefreshToken = doc["refresh_token"].as<String>();
      //ok, well i guess i gotta literally check this for null as a string value lol, drove me fucking bonkers
      //better way possibly though? string comparisons blow.
      if (newRefreshToken != NULL && newRefreshToken != "null" && newRefreshToken.length() > 0)
      {
        Serial.println("SETTING NEW REFRESH TOKEN");
        Serial.println("NEW REFRESH TOKEN = " +newRefreshToken);
        refreshToken = newRefreshToken;          
      }
      else
      {
        Serial.println("NEW REFRESH TOKEN BLANK, DO NOT SET");        
      }
      
      tokenExpireTime = doc["expires_in"].as<ulong>();
      tokenStartTime = millis();

      Serial.println("Saving token data after refresh");
      Serial.println("Access Token: " + doc["access_token"].as<String>());
      Serial.println("Refresh Token: " + doc["refresh_token"].as<String>());
      
      saveTokenData(tokenExpireTime, accessToken, refreshToken);
      
      accessTokenSet = true;

      Serial.println("Successfully refreshed Spotify access token!");
    
      return accessTokenSet;
    }
    bool getTrackInfo(){

        spotifyRequest.Method = "GET";
        spotifyRequest.Request = "/v1/me/player/currently-playing HTTP/1.1";
        bool success = false;
        bool refresh = false;

        if (makeSpotifyApiRequestASDOC2(spotifyRequest, SPOTIFY_API_ENDPOINT, "")){
            
            JsonArray artists = spotifyResponse["item"]["album"]["artists"].as<JsonArray>();
            JsonArray images = spotifyResponse["item"]["album"]["images"].as<JsonArray>();
            String requestId = spotifyResponse["item"]["id"].as<String>();
            
            Serial.println("CurrentTrackId " + spotifyResponse["item"]["id"].as<String>());
            Serial.println("TrackName " + spotifyResponse["item"]["name"].as<String>());
            Serial.println("ArtistName "+ artists[0]["name"].as<String>());

            String songId = spotifyResponse["item"]["id"].as<String>();
            currentSong.album = "PLACEHOLDER";//spotifyResponse["item"]["name"].as<String>(); //getValue(https,"name");            
            currentSong.song = spotifyResponse["item"]["name"].as<String>();
            
            currentSong.artist = artists[0]["name"].as<String>();
            //double check this in the json payload
            currentSong.durationMs = 0;//spotifyResponse["item"]["duration_ms"].as<int>();
            
            Serial.println("DurationMS: " + currentSong.durationMs);

            //seems to break when we set play status from here, but doing it in the button press isnt technically right either
            //lets try to fix this here now that we got it working the other way.
            bool playingStatusChanged = isPlaying != spotifyResponse["is_playing"].as<bool>();
            
            Serial.println("playing status changed = " + String(playingStatusChanged));

            isPlaying = spotifyResponse["is_playing"].as<bool>();
            
            // if(playingStatusChanged)
            // {
            //   Serial.println("Toggle btn text from get track info, isPlaying = " + String(isPlaying));  
              toggleBtnText();
            //}            
            Serial.println("is playing (gettackinfo)" + String(isPlaying));
            
            //Faster load but smaller image, start with 4
            String imageLink = images[1]["url"].as<String>();
            
            
            if(imageLink.length() <= 0)
            {
              Serial.println("image link is null SKIPPING");              
            }
            else{                
              Serial.println("song Id: " + songId);
              Serial.println("currentSong.Id: " + currentSong.Id);

              if (songId != currentSong.Id || currentSong.Id.length() == 0){                  
                  currentSong.Id = songId;
                  refresh = true;
                  if(SPIFFS.exists("/albumArt.jpg") == true) {
                      SPIFFS.remove("/albumArt.jpg");
                  }                  
                  
                  bool loaded_ok = downloadImageToSPIFFS(imageLink, "albumArt.jpg");          
                  
                  if(loaded_ok){
                    refresh = true;
                    tft.fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT - 60 , TFT_BLACK);                             
                  }                                                                       
              }
            }                                
            
            success = true;
          }        
        
        //draw screen logic here, should keep this intact
        // Disconnect from the Spotify API
        if(success){            
            drawScreen(refresh);

            lastSongPositionMs = currentSongPositionMs;
        }

        spotifyResponse.clear();
        return success;
      }       
    bool findLikedStatus(String songId){
        //we dont have this yete but add it in for funzies
        spotifyRequest.Method = "GET";
        spotifyRequest.Request = "v1/me/tracks/contains?ids=" + songId + " HTTP/1.1";
        
      // String url = "https://api.spotify.com/v1/me/tracks/contains?ids="+songId;
        // https.begin(*client,url);
        // String auth = "Bearer " + String(accessToken);
        // https.addHeader("Authorization",auth);
        // https.addHeader("Content-Type","application/json");
        // int httpResponseCode = https.GET();
        // bool success = false;
        // // Check if the request was successful
        // if (httpResponseCode == 200) {
        //     String response = https.getString();
        //     https.end();
        //     return(response == "[ true ]");
        // } else {
        //     Serial.print("Error toggling liked songs: ");
        //     Serial.println(httpResponseCode);
        //     String response = https.getString();
        //     Serial.println(response);
        //     https.end();
        // }

        
        // // Disconnect from the Spotify API
        
        return true;// success;
    }
    bool toggleLiked(String songId){
       //we dont have this yete but add it in for funzies 
        // String url = "https://api.spotify.com/v1/me/tracks/contains?ids="+songId;
        // https.begin(*client,url);
        // String auth = "Bearer " + String(accessToken);
        // https.addHeader("Authorization",auth);
        // https.addHeader("Content-Type","application/json");
        // int httpResponseCode = https.GET();
        // bool success = false;
        // // Check if the request was successful
        // if (httpResponseCode == 200) {
        //     String response = https.getString();
        //     https.end();
        //     if(response == "[ true ]"){
        //         currentSong.isLiked = false;
        //         dislikeSong(songId);
        //     }else{
        //         currentSong.isLiked = true;
        //         likeSong(songId);
        //     }
        //     drawScreen(false,true);
        //     Serial.println(response);
        //     success = true;
        // } else {
        //     Serial.print("Error toggling liked songs: ");
        //     Serial.println(httpResponseCode);
        //     String response = https.getString();
        //     Serial.println(response);
        //     https.end();
        // }

        
        // // Disconnect from the Spotify API
        
        return true; //success;
    }
    void drawLoadingScreen()
    {
      Serial.println("hit loading screen");
      tft.fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT - 60 , TFT_BLACK);
      
      tft.setCursor(SCREEN_WIDTH / 6 , SCREEN_HEIGHT / 2);
      tft.println("LOADING");
    }
    bool drawScreen(bool fullRefresh = false, bool likeRefresh = false){
        
        if(fullRefresh){
          
            if (SPIFFS.exists("/albumArt.jpg") == true) {              
                Serial.println("Drawing Album Art");  
                displayJPEGScaled("/albumArt.jpg", SCREEN_WIDTH, SCREEN_HEIGHT,20);
                Serial.println("Drawing album art done");
                Serial.println("Current Artist: " + currentSong.artist);
                Serial.println("Current Song: " + currentSong.song);
            }else{
              Serial.println("cant locate album art");                
            }
            tft.setTextDatum(MC_DATUM);
            tft.setTextWrap(true);
            
            tft.setCursor(0,195);
            tft.setCursor((int)(tft.width()/2 - tft.textWidth(currentSong.artist) / 2), tft.getCursorY());
            if (currentSong.artist == "" || currentSong.artist.length() == 0)
            {
              Serial.println("no artist");
              tft.println("MISSING");
            }else{
              Serial.println("Drawing Artist: " + currentSong.artist);
            tft.println(currentSong.artist);
            }
            tft.setCursor(0,215);          
            tft.setCursor((int)(tft.width()/2 - tft.textWidth(currentSong.song) / 2), tft.getCursorY());
            
            if (currentSong.song == "" || currentSong.song.length() == 0)
            {
              Serial.println("no song");
              tft.println("MISSING");
            }else{
              Serial.println("Drawing Song: " + currentSong.song);
              tft.println(currentSong.song);
            }
            
        }

        //we dont have this implemented yet, but plan for it later
        // if(fullRefresh || likeRefresh){
        //     if(currentSong.isLiked){
        //         TJpgDec.setJpgScale(1);
        //         TJpgDec.drawFsJpg(128-20, 0, "/heart.jpg");
        //     //    tft.fillCircle(128-10,10,10,TFT_GREEN);
        //     }else{
        //         tft.fillRect(128-21,0,21,21,TFT_BLACK);
        //     }
        // }
        // if(lastSongPositionMs > currentSongPositionMs){
        //     tft.fillSmoothRoundRect(
        //         tft.width() / 2 - rectWidth / 2 + 2,
        //         140 + 2,
        //         rectWidth  - 4,
        //         rectHeight - 4,
        //         10,
        //         TFT_BLACK
        //     );
        //     lastSongPositionMs = currentSongPositionMs;
        // }
        // tft.fillSmoothRoundRect(
        //     tft.width() / 2 - rectWidth / 2 + 2,
        //     140 + 2,
        //     rectWidth * (currentSongPositionMs/currentSong.durationMs) - 4,
        //     rectHeight - 4,
        //     10,
        //     TFT_GREEN
        // );
        // Serial.println(currentSongPositionMs);
        // Serial.println(currentSong.durationMs);
        // Serial.println(currentSongPositionMs/currentSong.durationMs);        
        return true;
    }
    bool togglePlay(){    
        Serial.println("is playing before action" + String(isPlaying));
        if(isPlaying)
        {
          Serial.println("Sending pause request");
          spotifyRequest.Method = "PUT"; 
          spotifyRequest.Request = "/v1/me/player/pause HTTP/1.1";  ///v1/me/player/play
          if(makeSpotifyApiRequestASDOC2(spotifyRequest, SPOTIFY_API_ENDPOINT, ""))  
          {
            toggleBtnText();            
          }
        }
        else
        {          
          Serial.println("Sending play request");
          spotifyRequest.Method = "PUT"; 
          spotifyRequest.Request = "/v1/me/player/play HTTP/1.1";          
          if(makeSpotifyApiRequestASDOC2(spotifyRequest, SPOTIFY_API_ENDPOINT, ""))  
          {
            toggleBtnText();            
          }
        }
        
        
        Serial.println("Is Playing Start: " + String(isPlaying));
        isPlaying = !isPlaying;
        Serial.println("Is Playing End: " + String(isPlaying));

        return true; //success;
    }
    void toggleBtnText()
    {

      Serial.println("Toggle btn text");
        
        if(isPlaying)
        {          
          Serial.println("Track is playing mark as pause");
          Serial.println("Is Playing = " + String(isPlaying));
                                
          btnPlay.drawSmoothButton(false, 3, TFT_BLACK, "Pause");
          //btnPlay.drawButton(false, "Pause");// .initButtonUL(buttonPlayX, buttonY, buttonWidth, BUTTON_HEIGHT, TFT_WHITE, TFT_BLUE, TFT_WHITE, "YEET", 1);
        }
        else
        { 
          Serial.println("Track is paused mark as play");
          Serial.println("Is Playing = " + String(isPlaying));
                
          btnPlay.drawSmoothButton(false, 3, TFT_BLACK, "Play");                      
          //btnPlay.initButtonUL(buttonPlayX, buttonY, buttonWidth, BUTTON_HEIGHT, TFT_WHITE, TFT_BLUE, TFT_WHITE, "YAAT", 1);
        }
    }
    bool adjustVolume(int vol){
         //we dont have this yete but add it in for funzies 
        // String url = "https://api.spotify.com/v1/me/player/volume?volume_percent=" + String(vol);
        // https.begin(*client,url);
        // String auth = "Bearer " + String(accessToken);
        // https.addHeader("Authorization",auth);
        // int httpResponseCode = https.PUT("");
        // bool success = false;
        // // Check if the request was successful
        // if (httpResponseCode == 204) {
        //     // String response = https.getString();
        //     currVol = vol;
        //     success = true;
        // }else if(httpResponseCode == 403){
        //      currVol = vol;
        //     success = false;
        //     Serial.print("Error setting volume: ");
        //     Serial.println(httpResponseCode);
        //     String response = https.getString();
        //     Serial.println(response);
        // } else {
        //     Serial.print("Error setting volume: ");
        //     Serial.println(httpResponseCode);
        //     String response = https.getString();
        //     Serial.println(response);
        // }

        
        // // Disconnect from the Spotify API
        // https.end();
        return true; //success;
    }
    bool skipForward(){
        bool success = false;
        spotifyRequest.Method= "POST";
        spotifyRequest.Request = "/v1/me/player/next HTTP/1.1";
       
        
        // if(makeSpotifyApiRequestASDOC2(spotifyRequest, SPOTIFY_API_ENDPOINT, ""))
        // {
          makeSpotifyApiRequestASDOC2(spotifyRequest, SPOTIFY_API_ENDPOINT, "");
          drawLoadingScreen();
          getTrackInfo();
          success = true;
        // }
        // else
        // {
        //   Serial.println("Skip fwd request fail, loading screen not drawn.");
        // }

        return success;
    }
    bool skipBack(){       
        bool success = false;
        spotifyRequest.Method= "POST";
        spotifyRequest.Request = "/v1/me/player/previous HTTP/1.1";
        
        // if()
          makeSpotifyApiRequestASDOC2(spotifyRequest, SPOTIFY_API_ENDPOINT, "");
        // {
          drawLoadingScreen();
          getTrackInfo();
          success = true;
        // }
        // else
        // {
        //   Serial.println("Skip back request fail, loading screen not drawn.");
        // }

        return success;
    }    
    bool makeSpotifyApiRequestASDOC2(SpotifyRequest apiRequest, const char* baseURI, const String body = "")  {
      //WiFiClientSecure client;
      //client.setInsecure();
      
      // if(accessTokenSet = false)
      // {
      //    createRefreshToken();
      // }


      if(client.connected())
      {
        client.stop();
        Serial.println("CLIENT CONNECTED!!");
      }

      if (!client.connect(baseURI, SPOTIFY_PORT)) {
        Serial.println("Connection failed");
        return false;
      }
      client.println(apiRequest.Method + " " + apiRequest.Request); //<-- literally the same string as above but this fails  
      client.println("Host: " + String(baseURI));

      Serial.println("Bearer token: " + accessToken);

      client.println("Authorization: Bearer " + accessToken);
     
      
      if (apiRequest.Method == "POST" || apiRequest.Method == "PUT") {
        client.println("Content-Type: application/json");
        
        client.println("Content-Length: " + String(body.length()));
      }  
      
      
      client.println("Connection: close");
      client.println();
      
      if (body.length() > 0) {
        client.println(body);
      }
      
      // Wait for response
      unsigned long timeout = millis();
      while (client.available() == 0) {
        if (millis() - timeout > 10000) {
          client.stop();      
          Serial.println("Request timeout");
          return false;      
        }
      }
      
      // Skip HTTP headers
      char endOfHeaders[] = "\r\n\r\n";
      if (!client.find(endOfHeaders)) {
        Serial.println("Invalid response");
        return false;
      }
      
  
      // Read response
      DynamicJsonDocument doc(2048); // Adjust size based on expected response
      
      DeserializationError error = deserializeJson(doc, client);
      Serial.println("error:");
      Serial.println(error.c_str());
      client.stop();
      
      if (error) {
        Serial.print("JSON parsing failed: ");
        Serial.println(error.c_str());
                

        Serial.println("Request: " + apiRequest.Request);
        return false;
      }
      
      
      // Check for error response
      if (doc.containsKey("error")) {
        Serial.print("Spotify API error: ");
        Serial.println(doc["error"].as<String>());
        Serial.print("Status");
        Serial.println(doc["error"]["status"].as<String>());
        int statusCode = doc["error"]["status"].as<int>();
        //lame first attenpt at trying to get a refresh token generated somehow
        // if (statusCode == 401)
        // {
        //     //need to reset access token
        //     needsRefresh = true;
            
        // }
        Serial.print("Message");
        Serial.println(doc["error"]["message"].as<String>());
        

      //   if (doc.containsKey("error_description")) {
      //     Serial.println(doc["error_description"].as<String>());
      //     tft.drawString(doc["error_description"].as<String>(), tft.width()/6-6, 175);
      //   }
         return false;
       }

      spotifyResponse = doc;

      return true;
    }
    bool downloadImageToSPIFFS(String imageUrl,  String fileName) {
    // Check if file already exists
    if (SPIFFS.exists("/" + fileName)) {
        
        Serial.println("File already exists: " + fileName + " - Skipping download");
        //just clear out for now to force downloads
        SPIFFS.remove("/"+fileName);
        
        return true;
        //
      }

       if ((WiFi.status() == WL_CONNECTED)) {
           client.setInsecure();
            
            //HTTPClient http;
            https.begin(client, imageUrl);

       }
      
      //HTTPClient http;
      
      Serial.println("Downloading: " + imageUrl);
      Serial.println("Saving as: " + fileName);
      
      // Handle HTTPS URLs (like Spotify CDN)
     
      
      https.setTimeout(15000); // 15 second timeout for HTTPS
      
      // Add headers for Spotify CDN compatibility
      // https.addHeader("User-Agent", "Mozilla/5.0 (ESP32) AppleWebKit/537.36");
      // https.addHeader("Accept", "image/webp,image/apng,image/*,*/*;q=0.8");
      // https.addHeader("Accept-Encoding", "identity"); // Disable compression for simplicity
      // https.addHeader("Connection", "close");
      Serial.println("made it to get");
      int httpCode = https.GET();
      Serial.println("GET Success");

      if (httpCode != HTTP_CODE_OK) {
          Serial.printf("HTTP GET failed, error: %d\n", httpCode);
          if (httpCode == -5) {
              Serial.println("Error -5: Connection refused or SSL handshake failed");
              Serial.println("This often happens with HTTPS sites. Retrying...");
          }
          https.end();
          return false;
      }
      
      // Get content length for progress tracking
      int contentLength = https.getSize();
      Serial.printf("Content length: %d bytes\n", contentLength);
      
      // Check available SPIFFS space
      size_t totalBytes = SPIFFS.totalBytes();
      size_t usedBytes = SPIFFS.usedBytes();
      size_t freeBytes = totalBytes - usedBytes;
      
      if (contentLength > 0 && contentLength > freeBytes) {
          Serial.println("Not enough space in SPIFFS");
          https.end();
          return false;
      }
      
      // Open file for writing
      File file = SPIFFS.open("/" + fileName, FILE_WRITE);
      if (!file) {
          Serial.println("Failed to create file");
          https.end();
          return false;
      }
      
      // Get stream for reading
      WiFiClient* stream = https.getStreamPtr();
      
      // Download with large buffer for speed
      //uint8_t buffer[DOWNLOAD_BUFFER_SIZE];
      uint8_t* buffer = (uint8_t*)malloc(DOWNLOAD_BUFFER_SIZE);
      if (!buffer) {
          Serial.println("Failed to allocate download buffer");
          return false;
      }
      
      size_t totalDownloaded = 0;
      unsigned long startTime = millis();
      
      while (https.connected() && (contentLength <= 0 || totalDownloaded < contentLength)) {
        size_t availableData = stream->available();
        
        if (availableData > 0) {
            size_t readBytes = stream->readBytes(buffer, 
                min(availableData, (size_t)DOWNLOAD_BUFFER_SIZE));
            
            if (readBytes > 0) {
                size_t writtenBytes = file.write(buffer, readBytes);
                if (writtenBytes != readBytes) {
                    Serial.println("Write error!");
                    break;
                }
                totalDownloaded += readBytes;
                
                // Progress update and yield
                if (totalDownloaded % 10240 == 0 || 
                    (contentLength > 0 && totalDownloaded >= contentLength)) {
                    float progress = contentLength > 0 ? 
                        (float)totalDownloaded / contentLength * 100 : 0;
                    Serial.printf("Downloaded: %d bytes", totalDownloaded);
                    if (contentLength > 0) {
                        Serial.printf(" (%.1f%%)", progress);
                    }
                    Serial.println();
                }
                
                // Add yield every iteration when processing data
                yield(); // or ESP.wdtFeed() for explicit watchdog reset
            }
        } else {
            delay(10); // Increase delay and ensure yield
      }
  
    // Timeout check
    if (millis() - startTime > 30000) {
        Serial.println("Download timeout");
        break;
    }
}
      file.close();
      https.end();
      
      unsigned long downloadTime = millis() - startTime;
      float speed = totalDownloaded / (downloadTime / 1000.0) / 1024.0; // KB/s
      
      Serial.printf("Download completed: %d bytes in %lu ms (%.2f KB/s)\n", 
                    totalDownloaded, downloadTime, speed);
      

      Serial.println("Made it to file save");
      Serial.println("File name: " + fileName);                 
      // Verify file was written correctly
      if (SPIFFS.exists("/" + fileName)) {
          File verifyFile = SPIFFS.open("/" + fileName, FILE_READ);
          size_t fileSize = verifyFile.size();
          verifyFile.close();
          
          if (contentLength > 0 && fileSize != contentLength) {
              Serial.printf("Warning: File size mismatch. Expected: %d, Got: %d\n", 
                          contentLength, fileSize);
              return false;
          }
          
          Serial.println("File saved successfully: " + fileName);
          return true;
      }
      
      Serial.println("File verification failed");
      return false;
  }

    //TOKEN MANAGMENT
    void saveTokenData(unsigned long expiresInSeconds, String accessToken, String refreshToken )  {
      //prefs.begin("token", false);
      
      time_t now = time(nullptr);
      
      prefs.putString("accessToken", accessToken);
      prefs.putString("refreshToken", refreshToken);
      
      prefs.putULong("retrievedAt", (unsigned long)now);
      prefs.putULong("expiresIn", expiresInSeconds);
      prefs.putBool("valid", true);
      
      loadTokenData();
      //prefs.end();
      
      Serial.printf("Token saved - Retrieved: %s", ctime(&now));
      Serial.printf("Token expires in: %lu seconds\n", expiresInSeconds);
    }
    TokenData loadTokenData() {
      TokenData token;
      
      //prefs.begin("token", true);
      
      token.accessToken = prefs.getString("accessToken", "");
      token.refreshToken = prefs.getString("refreshToken", "");
      token.retrievedAt = (time_t)prefs.getULong("retrievedAt", 0);
      token.expiresIn = prefs.getULong("expiresIn", 0);
      token.isValid = prefs.getBool("valid", false);
    
      //prefs.end();
      Serial.println("Retrieved Token Data REFRESH TOKEN = " + token.refreshToken);
      return token;
    }
    bool isTokenExpired() {
      TokenData token = loadTokenData();
      
      if (!token.isValid || token.retrievedAt == 0) {
        Serial.println("No valid token found");
        accessTokenSet = false;
        return true;
      }
      
      //time_t now = time(nullptr);
      time_t expirationTime = token.retrievedAt + token.expiresIn;
      
      Serial.println("Now: " + String(now));
      Serial.println("Expiration Time: " + String(expirationTime));
      Serial.println("Retrieved at: " + String(token.retrievedAt));
      Serial.println("Expires In: " + String(token.expiresIn));

      bool expired = now >= expirationTime;
      
      if (expired) {
        Serial.println("Token has expired");
        Serial.printf("Retrieved at: %s", ctime(&token.retrievedAt));
        Serial.printf("Expired at: %s", ctime(&expirationTime));
        Serial.printf("Current time: %s", ctime(&now));
      } else {
        long remainingSeconds = expirationTime - now;
        Serial.printf("Token valid for %ld more seconds\n", remainingSeconds);
      }
      
      return expired;
    }
    void invalidateToken() {
      prefs.begin("token", false);
      prefs.putBool("valid", false);
      prefs.end();
      Serial.println("Token invalidated");
    }
    void clearTokenData() {
      //prefs.begin("token", false);
      prefs.clear();
      accessTokenSet = false;
      //prefs.end();
      Serial.println("All token data cleared");
    }
    long getTokenRemainingSeconds() {
      TokenData token = loadTokenData();
      
      if (!token.isValid || token.retrievedAt == 0) {
        return -1; // No valid token
      }
      
      time_t now = time(nullptr);
      time_t expirationTime = token.retrievedAt + token.expiresIn;
      
      if (now >= expirationTime) {
        return 0; // Token expired
      }
      
      return expirationTime - now;
    }
    //END TOKEN MANAGEMENT

    //just using these for testing, remove for prod unless we need some way to show user their tokens
    String ShowAccessToken()
    {
      return accessToken;
    }
    String ShowRefreshToken()
    {
      return refreshToken;
    }
    
    void flushClient()
    {
      client.flush();      
    }

    bool accessTokenSet = false;
    bool needsRefresh = false;
    bool deviceActive = false;
    bool noDevSettings = false;
    long tokenStartTime;
    int tokenExpireTime;
    songDetails currentSong;
    SpotifyDevice spotifyDevice2;
    float currentSongPositionMs;
    float lastSongPositionMs;
    int currVol;
    bool isPlaying = false;    
    //being dumb, do not do this
    bool skipbtndraw = false;
    bool devicesLoaded = false;
    bool noDeviceDrawn = false;
private:
    WiFiClientSecure client;
    HTTPClient https;
    // bool isPlaying = false;
    //bool buttonsDrawn = false;
    //JsonDocument spotifyResponse2;//not sure if we need the buffer size or not (this ends up needing the buffer size)
    String authCode;
    String accessToken;
    String refreshToken;
    String scope;
};

void wifi_splash_screen() {
  tft.drawString("Connect to" + String(DEFAULT_CAPTIVE_SSID) , 20, 175);
  tft.drawString("or visit Http:192.168.1.4" , 20, 185);
}
void flush_wifi_splashscreen(uint32_t ms = 200) {
  uint32_t start = millis();
  while (millis() - start < ms) {
    
    delay(5);
  }
}
void apModeCallback(WiFiManager *mgr) {
  wifi_splash_screen();
  flush_wifi_splashscreen();
}

SpotConn spotifyConnection;
String clientID;
String clientSecret;

void btnRwd_pressAction(void)
{
    if (btnRwd.justPressed()) {
      if(spotifyConnection.skipBack())
      {
        Serial.println("Playback Started");
        spotifyConnection.isPlaying = true; //
        spotifyConnection.toggleBtnText();
      }   
      else
      {
        Serial.println("Playback Failed");
      }
      btnRwd.drawSmoothButton(false);
    }
}
void btnPlay_pressAction(void)
{  
  Serial.println("Play btn pressed");
  if (btnPlay.justPressed()) {
  
    if(spotifyConnection.togglePlay())
    {
     
      //String action = String(spotifyConnection.isPlaying? "Pause" : "Play");
      
      Serial.println("isPlaying = " + spotifyConnection.isPlaying);            
     // btnPlay.drawSmoothButton(false, 3, TFT_BLACK, String(action));//.drawButton(false, String(action));
      
    }   
    else
    {
      Serial.println("Playback Failed");
    }
      
    btnPlay.drawSmoothButton(false);    
    
  }
}
void btnFwd_pressAction(void)
{
  if (btnFwd.justPressed()) {
    
    if(spotifyConnection.skipForward())
    {
      spotifyConnection.isPlaying = true;
      spotifyConnection.toggleBtnText();
      Serial.println("Playback Started");
    }   
    else
    {
      Serial.println("Playback Failed");
    }
      

    Serial.print("Fwd button just pressed");
    btnFwd.drawSmoothButton(false);
  }
}

void initButtons() {

      int totalUsableWidth = SCREEN_WIDTH - (2 * BUTTON_MARGIN) - (2 * BUTTON_SPACING);
      int buttonWidth = totalUsableWidth / 3;
      
      // Y position for all buttons (same for all since they're aligned at bottom)
      int buttonY = SCREEN_HEIGHT - BUTTON_HEIGHT - BUTTON_MARGIN;
      
      // X positions for each button
      int buttonRwdX = BUTTON_MARGIN;
      int buttonPlayX = buttonRwdX + buttonWidth + BUTTON_SPACING;
      int buttonFwdX = buttonPlayX + buttonWidth + BUTTON_SPACING;
      
      // Initialize rwd
      btnRwd.initButtonUL(buttonRwdX, buttonY, buttonWidth, BUTTON_HEIGHT, TFT_WHITE, TFT_BLUE, TFT_WHITE, "<<", 1);
      btnRwd.drawButton();
      btnRwd.setPressAction(btnRwd_pressAction);
      btnRwd.drawSmoothButton(false, 3, TFT_DARKGREEN); // Rounded corners
      
      // Initialize play
       if(spotifyConnection.isPlaying)
       {
        btnPlay.initButtonUL(buttonPlayX, buttonY, buttonWidth, BUTTON_HEIGHT, TFT_WHITE, TFT_BLUE, TFT_WHITE, "Pause", 1);
      
       }
       else{
        btnPlay.initButtonUL(buttonPlayX, buttonY, buttonWidth, BUTTON_HEIGHT, TFT_WHITE, TFT_BLUE, TFT_WHITE, "Play", 1);
        
       }
      btnPlay.drawButton();
      btnPlay.setPressAction(btnPlay_pressAction);
      btnPlay.drawSmoothButton(false, 3, TFT_DARKGREEN);
      
      // Initialize fwd
      btnFwd.initButtonUL(buttonFwdX, buttonY, buttonWidth, BUTTON_HEIGHT, TFT_WHITE, TFT_BLUE, TFT_WHITE, ">>", 1);
      btnFwd.setPressAction(btnFwd_pressAction);
      btnFwd.drawButton();
      btnFwd.drawSmoothButton(false, 3, TFT_DARKGREEN);      
    }

void setup() {
  // For logging
  Serial.begin(115200);
  //SPIFFS.format();
  // Try to mount SPIFFS without formatting on failure
  if (!SPIFFS.begin(false)) {
    // If SPIFFS does not work, we wait for serial connection...
    while(!Serial);
    delay(1000);

    // Ask to format SPIFFS using serial interface
    Serial.print("Mounting SPIFFS failed. Try formatting? (y/n): ");
    while(!Serial.available());
    Serial.println();

    // If the user did not accept to try formatting SPIFFS or formatting failed:
    if (Serial.read() != 'y' || !SPIFFS.begin(true)) {
      Serial.println("SPIFFS not available. Stop.");
      while(true);
    }
    Serial.println("SPIFFS has been formated.");
  }
  else
  {
    if (SPIFFS.exists("/albumArt.jpg"))
    {
      SPIFFS.remove("/albumArt.jpg");
    }

  }
  Serial.println("SPIFFS has been mounted.");

  // Now that SPIFFS is ready, we can create or load the certificate
  SSLCert *cert = getCertificate();
  if (cert == NULL) {
    Serial.println("Could not load certificate. Stop.");
    while(true);
  }
  
  tft.init();
  tft.setFreeFont(FF17);
  tft.setSwapBytes(true);
  pinMode(LCD_BACKLIGHT_PIN, OUTPUT);
  // Init touchscreen
  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);

  touchscreen.begin(touchscreenSPI);
 
  touchscreen.setRotation(0);
  
  tft.fillScreen(TFT_BLACK);
  tft.setTextWrap(true, false);
  tft.setTextSize(1);
  
    
  //tft.drawString(message , 20, 175);
  tft.setCursor(20, 155); // Set the cursor to (10, 20)
  tft.print("LOADING....");
  
  WiFiManager wm;
  wm.setAPCallback(apModeCallback);
  wm.autoConnect(DEFAULT_CAPTIVE_SSID);
  //not too sure how needed this is, but it adds such a shit delay to program start
  //but if shit doesnt calculate right maybe we need to put it back
  initializeTime();

  //attempt to load credentials from memory
  prefs.begin("devcredentials", false);
  

  CLIENT_ID = prefs.getString("clientId","");
  CLIENT_SECRET = prefs.getString("clientSecret", "");
  
  String accessToken;
  String refrehshToken;
  int expireStart;
  int expireTime;
  


  //until we get the refresh token scheme handled, make us log in for a fresh access key each time
  //sucks but once the token expires we just get stuck
  accessToken = prefs.getString("apiAccessToken");
  refrehshToken = prefs.getString("apiRefreshToken");
  expireStart = prefs.getInt("apiStartTime");
  expireTime = prefs.getInt("apiExpireTime");
  
  if(CLIENT_ID == "" || CLIENT_SECRET == "")
  {
    //no dev settings saved, use logic to tell loop to display dev page
    spotifyConnection.noDevSettings = true;
  }
  else{       
      if(!spotifyConnection.isTokenExpired() )
      {
        Serial.println("Setting access token....");
        spotifyConnection.loadTokenData();
        Serial.println("Access Token:" + spotifyConnection.ShowAccessToken());
        Serial.println("Refresh Token:" + spotifyConnection.ShowRefreshToken());
        Serial.println("Expire Start:" + String(expireStart));
        Serial.println("Expire End:" + String(expireTime));
       

        spotifyConnection.setAccessToken();
        //(accessToken, refrehshToken, expireStart, expireTime);
      }
      else{
        Serial.println("No access token saved");
        Serial.println("Access Token:" + accessToken);
        Serial.println("Refresh Token:" + refrehshToken);
        Serial.println("Expire Start:" + String(expireStart));
        Serial.println("Expire End:" + String(expireTime));
        spotifyConnection.clearTokenData();
      }
  }
  
  //quick sanity test, fail on first go, reboot and it should work on second?
  prefs.putString("clientId", CLIENT_ID);
  prefs.putString("clientSecret", CLIENT_SECRET);


  // Create the server with the certificate we loaded before
  secureServer = new HTTPSServer(cert);

  // We register the SPIFFS handler as the default node, so every request that does
  // not hit any other node will be redirected to the file system.
  ResourceNode * spiffsNode = new ResourceNode("", "", &handleSPIFFS);
  secureServer->setDefaultNode(spiffsNode);

  //I think here we should send up a page to configure the client id and client secret on initial load
  ResourceNode * getEventsNode = new ResourceNode("/", "GET", &handleGetEvents);
  secureServer->registerNode(getEventsNode);

  //spotify callback route to handle intitial token exchange
  ResourceNode* callbackNode = new ResourceNode("/callback", "GET", &handleSpotifyCallback);
  secureServer->registerNode(callbackNode);

  //we can get rid of this for prod
  ResourceNode* handleCurrentSongNode = new ResourceNode("/current", "GET", &handleCurrent);
  secureServer->registerNode(handleCurrentSongNode);

  ResourceNode* handleDevSetupNode = new ResourceNode("/dev", "GET", &handleDevSetup);
  secureServer->registerNode(handleDevSetupNode);
  
  
  ResourceNode* handleDevSetupSaveNode = new ResourceNode("/dev", "POST", &handleDevSetup);
  secureServer->registerNode(handleDevSetupSaveNode);
  

  //action="/action_page.php" method="get"
  Serial.println("Starting server...");
  secureServer->start();
  
  //do we need to stop server once we have initial token exchange completed? essentially that is all we need
  //the secure server for.
  if (secureServer->isRunning()) {
    Serial.println("Server ready.");
  }
    
   
  //spotifyConnection.client.setCACert(_spotify_root_ca);
  
}

bool messageDrawn = false;

String messageName = "";
long timeLoop;
long refreshLoop;
long noDeviceLoop;
bool serverOn = true;
// bool buttonsDrawn  = false;

// void loop(){
//   delay(5);
// }

void loop() {  

if(spotifyConnection.accessTokenSet){
    //Serial.println("Access Token Set");
      //leave server on if token needs to be refreshed
        if(serverOn && !spotifyConnection.needsRefresh){
            secureServer->stop();
            serverOn = false;
            messageDrawn = false;
        }        
        else if(( millis() - refreshLoop) > 5000 ){
            Serial.println("LOOP: Refresh Token = " + spotifyConnection.ShowRefreshToken());

            if(spotifyConnection.getTokenRemainingSeconds() <= 3559)// set back to 120 after testing!!//120)
            {
              Serial.println("Token expiring in < 2 min, refreshing");
              spotifyConnection.refreshAccessToken();
            }

             Serial.println("Devices Loaded = "  + String(spotifyConnection.devicesLoaded));
            if (!spotifyConnection.devicesLoaded){
              Serial.println("Loading Device");
              spotifyConnection.LoadDevice();
              Serial.println("Device Loaded");
             }
            
             Serial.println("Devices Active = "  + String(spotifyConnection.spotifyDevice2.is_active));
            if(!spotifyConnection.spotifyDevice2.is_active)
            {
              Serial.println("Drawing no device screen");
              if(!spotifyConnection.noDeviceDrawn)
              {
                spotifyConnection.drawNoDeviceActive();
              }              
              spotifyConnection.LoadDevice();
              Serial.println("End no device screen");
            }
            else
            {
              Serial.println("Get Track Info Start");
              
              if(spotifyConnection.getTrackInfo()){
                Serial.println("flushing client");            
                spotifyConnection.flushClient();
              }                        
              // spotifyConnection.drawScreen();
              refreshLoop = millis();
              Serial.println("Leaving Get Track Info");
            
            }
            
            if(!spotifyConnection.skipbtndraw)
            {
              Serial.println("Drawing buttons");
              initButtons();
              spotifyConnection.skipbtndraw = true;
            }
        }     
    }
  else{
      if(spotifyConnection.noDevSettings)
      {
        if(!messageDrawn){
            tft.fillScreen(TFT_BLACK);
             // Set the text wrap mode to false for horizontal wrapping and true for vertical wrapping
            tft.setTextWrap(true, false);
            tft.setTextSize(1);
            
             
            //tft.drawString(message , 20, 175);
            tft.setCursor(20, 155); // Set the cursor to (10, 20)
            tft.print("Visit");
            tft.setCursor(20, 175); // Set the cursor to (10, 20)
            tft.print("Https://" + WiFi.localIP().toString() +"/Dev");
            tft.setCursor(20, 195); // Set the cursor to (10, 20)
            tft.print("to set your dev creds");
            //this needs to be rethought since this variable is et to true in other functions, just for testing mainly
            messageDrawn = true;
          }

      }
      else{
          if(!messageDrawn){
            Serial.println("No Access Token Set");
            tft.fillScreen(TFT_BLACK);
            tft.setTextWrap(true, false);
            tft.setTextSize(1);
            tft.drawString("Vist Https://" + WiFi.localIP().toString() + " to set your access token" , 20, 175);
            tft.drawString("to set your access token", 20, 200);
            //this needs to be rethought since this variable is et to true in other functions, just for testing mainly
            messageDrawn = true;
          }
      }

      
      secureServer-> loop();
    }  
    

    
    //insert touch screen read here
    static uint32_t scanTime = millis();        
    // Scan keys every 50ms at most
    if (millis() - scanTime >= 50) {
      bool pressed = (touchscreen.tirqTouched() && touchscreen.touched()); 

      if (pressed) {
          TS_Point p = touchscreen.getPoint();
          Serial.println("Screen Touched");
          x = map(p.x, 200, 3700, 1, SCREEN_WIDTH);
          y = map(p.y, 240, 3800, 1, SCREEN_HEIGHT);
          //z = p.z;
      }
      scanTime = millis();
      for (uint8_t b = 0; b < buttonCount; b++) {
        if (pressed) {
          if (btn[b]->contains(x, y)) {
            btn[b]->press(true);
            btn[b]->pressAction();
          }
        }
        else {
          btn[b]->press(false);
          btn[b]->releaseAction();
        }    
      }
        
          // int volRequest = map(analogRead(A0),0,1023,0,100);
          // if(abs(volRequest - spotifyConnection.currVol) > 2){
          //     spotifyConnection.adjustVolume(volRequest);
          // }
          timeLoop = millis();
      }      
  
      delay(5);
}

/**
 * This function will either read the certificate and private key from SPIFFS or
 * create a self-signed certificate and write it to SPIFFS for next boot
 */
SSLCert * getCertificate() {
  // Try to open key and cert file to see if they exist
  File keyFile = SPIFFS.open("/key.der");
  File certFile = SPIFFS.open("/cert.der");

  // If now, create them 
  if (!keyFile || !certFile || keyFile.size()==0 || certFile.size()==0) {
    Serial.println("No certificate found in SPIFFS, generating a new one for you.");
    Serial.println("If you face a Guru Meditation, give the script another try (or two...).");
    Serial.println("This may take up to a minute, so please stand by :)");

    SSLCert * newCert = new SSLCert();
    // The part after the CN= is the domain that this certificate will match, in this
    // case, it's esp32.local.
    // However, as the certificate is self-signed, your browser won't trust the server
    // anyway.
    int res = createSelfSignedCert(*newCert, KEYSIZE_1024, "CN=esp32.local,O=acme,C=DE");
    if (res == 0) {
      // We now have a certificate. We store it on the SPIFFS to restore it on next boot.

      bool failure = false;
      // Private key
      keyFile = SPIFFS.open("/key.der", FILE_WRITE);
      if (!keyFile || !keyFile.write(newCert->getPKData(), newCert->getPKLength())) {
        Serial.println("Could not write /key.der");
        failure = true;
      }
      if (keyFile) keyFile.close();

      // Certificate
      certFile = SPIFFS.open("/cert.der", FILE_WRITE);
      if (!certFile || !certFile.write(newCert->getCertData(), newCert->getCertLength())) {
        Serial.println("Could not write /cert.der");
        failure = true;
      }
      if (certFile) certFile.close();

      if (failure) {
        Serial.println("Certificate could not be stored permanently, generating new certificate on reboot...");
      }

      return newCert;

    } else {
      // Certificate generation failed. Inform the user.
      Serial.println("An error occured during certificate generation.");
      Serial.print("Error code is 0x");
      Serial.println(res, HEX);
      Serial.println("You may have a look at SSLCert.h to find the reason for this error.");
      return NULL;
    }

	} else {
    Serial.println("Reading certificate from SPIFFS.");

    // The files exist, so we can create a certificate based on them
    size_t keySize = keyFile.size();
    size_t certSize = certFile.size();

    uint8_t * keyBuffer = new uint8_t[keySize];
    if (keyBuffer == NULL) {
      Serial.println("Not enough memory to load privat key");
      return NULL;
    }
    uint8_t * certBuffer = new uint8_t[certSize];
    if (certBuffer == NULL) {
      delete[] keyBuffer;
      Serial.println("Not enough memory to load certificate");
      return NULL;
    }
    keyFile.read(keyBuffer, keySize);
    certFile.read(certBuffer, certSize);

    // Close the files
    keyFile.close();
    certFile.close();
    Serial.printf("Read %u bytes of certificate and %u bytes of key from SPIFFS\n", certSize, keySize);
    return new SSLCert(certBuffer, certSize, keyBuffer, keySize);
  }
}

/**
 * This handler function will try to load the requested resource from SPIFFS's /public folder.
 * 
 * If the method is not GET, it will throw 405, if the file is not found, it will throw 404.
 */
void handleSPIFFS(HTTPRequest * req, HTTPResponse * res) {
	
  // We only handle GET here
  if (req->getMethod() == "GET") {
    // Redirect / to /index.html
    std::string reqFile = req->getRequestString()=="/" ? "/index.html" : req->getRequestString();

    // Try to open the file
    std::string filename = std::string(DIR_PUBLIC) + reqFile;

    // Check if the file exists
    if (!SPIFFS.exists(filename.c_str())) {
      // Send "404 Not Found" as response, as the file doesn't seem to exist
      res->setStatusCode(404);
      res->setStatusText("Not found");
      res->println("404 Not Found");
      return;
    }

    File file = SPIFFS.open(filename.c_str());

    // Set length
    res->setHeader("Content-Length", httpsserver::intToString(file.size()));

    // Content-Type is guessed using the definition of the contentTypes-table defined above
    int cTypeIdx = 0;
    do {
      if(reqFile.rfind(contentTypes[cTypeIdx][0])!=std::string::npos) {
        res->setHeader("Content-Type", contentTypes[cTypeIdx][1]);
        break;
      }
      cTypeIdx+=1;
    } while(strlen(contentTypes[cTypeIdx][0])>0);

    // Read the file and write it to the response
    uint8_t buffer[256];
    size_t length = 0;
    do {
      length = file.read(buffer, 256);
      res->write(buffer, length);
    } while (length > 0);

    file.close();
  } else {
    // If there's any body, discard it
    req->discardRequestBody();
    // Send "405 Method not allowed" as response
    res->setStatusCode(405);
    res->setStatusText("Method not allowed");
    res->println("405 Method not allowed");
  }
}

// Callback handler for Spotify redirect
void handleSpotifyCallback(HTTPRequest* request, HTTPResponse* response) {  
  // Get parameters
  ResourceParameters* params = request->getParams();
  std::string spotifyCode; 
  
  if (params->getQueryParameter("code", spotifyCode)) {
    
    //new method using class which should store state of codes and stuff, neato.
    Serial.println("Getting Acces Token from Callback");
    if(spotifyConnection.getAccessToken(spotifyCode.c_str()))
    //if (exchangeCodeForToken(String(spotifyCode.c_str()), CLIENT_ID, CLIENT_SECRET, REDIRECT_URI))
    {
      //spotifyConnection.setSpotifyDeviceIsActive();
        // Success response
        response->setStatusCode(200);
        response->setStatusText("OK");
        response->setHeader("Content-Type", "text/html");

        String html = \
            "<HEAD>\n" \
            "<TITLE>ESP Spotify Login</TITLE>\n" \
            "</HEAD>\n" \
            "<html><body><h1>Authorized and Token exchanged!</h1>\n" \
            "<p>You can close this window.</p>\n" \ 
            "<p>Here is your base64 Header:</p>\n" \
            "<p> Authorization: Basic " + base64Encode(String(CLIENT_ID) + ":" + String(CLIENT_SECRET))  + "</p>\n" \
            "<p>Here is your code:</p>\n" \
            "<p>" + String(spotifyCode.c_str()) + "</p>\n" \
            "<p>Here is your refresh token:</p>\n" \
            "<p>"+  spotifyConnection.ShowRefreshToken() +"</p>\n" \
            "<p>Here is your access token:</p>\n" \
            "<p>"+  spotifyConnection.ShowAccessToken() +"</p>\n" \
            "<p><a href='/current'>Click to show current song</a><p>\n" \
            "</body></html>\n";
        
        response->print(html);
    }
    else
    {
        // Error response
        response->setStatusCode(400);
        response->setStatusText("Bad Request");
        response->setHeader("Content-Type", "text/html");
        
        String html = "<html><body><h1>Token Exchange failed</h1>";
        
        response->print(html);
    }          
  } else {
    // Error response
    response->setStatusCode(400);
    response->setStatusText("Bad Request");
    response->setHeader("Content-Type", "text/html");
    
    String html = "<html><body><h1>Authorization failed</h1>";
    html += "<p>No code parameter found.</p></body></html>";
    
    response->print(html);
  }
}
void handleGetEvents(HTTPRequest * req, HTTPResponse * res) {
 // Status code is 200 OK by default.
  // We want to deliver a simple HTML page, so we send a corresponding content type:
  res->setHeader("Content-Type", "text/html");
  char page[1000];
  Serial.println("Client Id: " + CLIENT_ID);
  String c = "b7aaed0542244cd2a065d335dad714b3";
  sprintf(page, _login_page, c.c_str(), WiFi.localIP().toString());
  Serial.println(page);
  res->print(page);//(_login_page);
  // The response implements the Print interface, so you can use it just like
  // you would write to Serial etc.
  // res->println("<!DOCTYPE html>");
  // res->println("<html>");
  // res->println("<head><title>Hello World!</title></head>");
  // res->println("<body>");
  // res->println("<h1>Hello World!</h1>");
  // res->print("<p>Your server is running for ");
  // // A bit of dynamic data: Show the uptime
  // res->print((int)(millis()/1000), DEC);
  // res->println(" seconds.</p>");
  // res->println("</body>");
  // res->println("</html>");
  
}
void handleCurrent(HTTPRequest * req, HTTPResponse * res){
  res->setHeader("Content-Type", "text/html");
  //make call to api post request here
  
  spotifyRequest.Method = "GET"; 
  spotifyRequest.Request = "/v1/me/player/currently-playing HTTP/1.1";
   
  // if (makeSpotifyApiRequestASDOC(spotifyRequest, SPOTIFY_API_ENDPOINT, "")) {
  //   //request success
  //    // Success response
  //       res->setStatusCode(200);
  //       res->setStatusText("OK");
  //       res->setHeader("Content-Type", "text/html");
                
  //       JsonArray artists = spotifyResponse["item"]["album"]["artists"].as<JsonArray>();
  //       JsonArray image = spotifyResponse["item"]["album"]["images"].as<JsonArray>();
  //       int artistCount = artists.size();
  //       Serial.print("Found ");
  //       Serial.print(artistCount);
  //       Serial.println(" artists:");
        
  //       const char* trackName = spotifyResponse["item"]["name"].as<const char*>();
        
  //       Serial.println("track name");
  //       Serial.println(trackName);
        
  //       const char* artistName;
  //       for (int i = 0; i < artistCount; i++) {
  //           artistName = artists[i]["name"];
            
  //           Serial.print(i+1);
  //           Serial.print(". ");
  //           Serial.print(artistName);                        
  //       }

  //       const char* imgUrl = image[1]["url"];
  //       int height = image[1]["height"];
  //       int width = image[1]["width"];

  //       String html = \
  //           "<HEAD>\n" \
  //           "<TITLE>Currently Playing</TITLE>\n" \
  //           "<meta http-equiv='refresh' content='3' />\n" \
  //           "</HEAD>\n" \
  //           "<html><body><h1 style='text-align: center'>Currently on Deck!</h1>\n" \            
  //           "<p style='text-align: center'> <img src='"+ String(imgUrl) +"' style='Width:"+ String(width)+"px;Height:"+ String(height) +"px' /></p>\n" \
  //           "<p style='text-align: center'> <strong>Artist:</strong> " + String(artistName)  + "</p>\n" \
  //           "<p style='text-align: center'> <strong>Track:</strong> " + String(trackName)  + "</p>\n" \
  //           "<p style='text-align: center'>\n "\
  //               "<input type='button' value='Back'>\n "\
  //               "<input type='button' value='Play/Pause'>\n "\
  //               "<input type='button' value='Next'>\n "\
  //           "</p>\n "\
  //           "</body></html>\n";

  //       res->print(html); 
  // }
  // else{
  //   //request failed
  //   // Error response
  //       res->setStatusCode(400);
  //       res->setStatusText("Bad Request");
  //       res->setHeader("Content-Type", "text/html");
        
  //       String html = "<html><body><h1>Token Exchange failed</h1>";
        
  //       res->print(html);
  // }  
}
void handleDevSetup(HTTPRequest * req, HTTPResponse * res)
{
    
  Serial.print("Method is: ");  
  String method =String(req->getMethod().c_str()); 
  if( method =="GET")
  {
    Serial.println(method);
    res->setHeader("Content-Type", "text/html");    
    res->print(_developer_credentials_page);
  }
  else{
      Serial.println(method);
      res->setHeader("Content-Type", "text/html");
  
      size_t bodySize = req->getContentLength();
      
      if (bodySize > 0) {
        char* buffer = new char[bodySize + 1];
        size_t bytesRead = req->readBytes((byte*)buffer, bodySize);
        buffer[bytesRead] = '\0';
        
        String formData = String(buffer);
        delete[] buffer;
        
        Serial.println("Received form data:");
        Serial.println(formData);

        // Count pairs first
        int pairCount = 1;
        for (int i = 0; i < formData.length(); i++) {
          if (formData.charAt(i) == '&') pairCount++;
        }
        
        String keys[pairCount];
        String values[pairCount];
        int currentPair = 0;
        
        // Parse pairs
        int start = 0;
        for (int i = 0; i <= formData.length(); i++) {
          if (i == formData.length() || formData.charAt(i) == '&') {
            String pair = formData.substring(start, i);
            int equalPos = pair.indexOf('=');
            
            if (equalPos != -1 && currentPair < pairCount) {
              keys[currentPair] = pair.substring(0, equalPos);
              values[currentPair] = pair.substring(equalPos + 1);
              
              // Basic URL decoding
              values[currentPair].replace("+", " ");
              values[currentPair].replace("%20", " ");
              
              Serial.println("Pair " + String(currentPair) + ": " + keys[currentPair] + " = " + values[currentPair]);
              currentPair++;
            }
            start = i + 1;
          }
        }
        String response = "";
        // Now you can access parsed data by index
        for (int i = 0; i < currentPair; i++) {

          response += "Processed: " + keys[i] + " -> " + values[i];
          
          if(keys[i]=="clientID")
          {
              prefs.putString("clientId", values[i]);
              CLIENT_ID = values[i];
          }
          else if(keys[i]=="clientSecret"){
              prefs.putString("clientSecret", values[i]);
              CLIENT_SECRET = values[i];
          }
          
          Serial.println("Processing: " + keys[i] + " -> " + values[i]);
        }
        
        res->setStatusCode(302); // Found (redirect)
        res->setStatusText("Found");
        res->setHeader("Location", "/"); // Redirect to root
        res->finalize(); // Important: finalize the response
        
      } else {
        res->setStatusCode(400);
        res->println("<html><body><h1>No form data received</h1></body></html>");
      }
  }
}
