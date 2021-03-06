#include <ESP8266WiFi.h>
#include <WiFiClient.h> 
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include "FS.h"

const int connect_wait = 20;

const char* ssid;
const char* password;
ESP8266WebServer server(80);
String network_list = "";

//////////////////////////
// CONFIG File
//////////////////////////
bool loadConfig() {
  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("Failed to open config file");
    return false;
  }

  size_t size = configFile.size();
  if (size > 1024) {
    Serial.println("Config file size is too large");
    return false;
  }

  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);

  // We don't use String here because ArduinoJson library requires the input
  // buffer to be mutable. If you don't use ArduinoJson, you may as well
  // use configFile.readString instead.
  configFile.readBytes(buf.get(), size);

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.parseObject(buf.get());

  if (!json.success()) {
    Serial.println("Failed to parse config file");
    return false;
  }

  ssid = json["ssid"];
  password = json["password"];

  Serial.print("Loaded SSID: ");
  Serial.print("|");
  Serial.print(ssid);
  Serial.println("|");
  Serial.print("Loaded Password: ");
  Serial.print("|");
  Serial.print(password);
  Serial.println("|");
  return true;
}

bool saveConfig(JsonObject& json) {
  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return false;
  }

  json.printTo(configFile);
  return true;
}


//////////////////////////
// Configuration AP / Station
//////////////////////////
void scan_networks(bool save_list){
  Serial.println("Scanning for Networks ..");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  
  Serial.println("scan start");
  // WiFi.scanNetworks will return the number of networks found
  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n == 0){
    Serial.println("no networks found");
  }else{
    Serial.print(n);
    Serial.println(" networks found");
    if(save_list) network_list = "<ul>";
    for (int i = 0; i < n; ++i){
      // Print SSID and RSSI for each network found
      if(save_list) network_list += "<li><a href='/?ssid="+WiFi.SSID(i)+"'>";
      if(save_list) network_list += WiFi.SSID(i);
      if(save_list) network_list += " (";
      if(save_list) network_list += WiFi.RSSI(i);
      if(save_list) network_list += ")";
      if(save_list) network_list += (WiFi.encryptionType(i) == ENC_TYPE_NONE)?" ":"*";
      if(save_list) network_list += "</a></li>";
      
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE)?" ":"*");
      delay(10);
    }
    if(save_list) network_list += "</ul>";
  }
  Serial.println("");
}

void setup_ap(){
  scan_networks(true);
  
  String ssid_string = "ESP8266_"+WiFi.macAddress().substring(9);
  char config_ssid[25];
  ssid_string.toCharArray(config_ssid,25);
  
  Serial.println("Configuring access point...");
  Serial.print("SSID: ");
  Serial.println(config_ssid);
  WiFi.softAP(config_ssid);

  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);

  ssid = "";
  password = "";
}

bool setup_station(){
  scan_networks(false);

  Serial.print("Connect to WiFi ");
  Serial.print(ssid);
  Serial.println(" ...");
  WiFi.begin(ssid, password);

  // Wait for connection
  int retry_count = 0;
  while(retry_count < connect_wait && WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
    retry_count++;
  }

  if(WiFi.status() != WL_CONNECTED){
    Serial.println("Connection failed ..");
    return false;
  }else{
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    return true;
  }
}


//////////////////////////
// WebServer
//////////////////////////
void handleRoot(){
  String message = "<html><head><title>ESP8266 "+WiFi.macAddress()+"</title></head><body>";
  message += "<h3>ESP8266 "+WiFi.macAddress()+"</h3>";

  // List of available APs
  message += "<h4>Available Networks</h4>";
  message += network_list;
  message += "NOTE: List is only updated on boot of ESP8266<br><br>";

  // Form for settings
  message += "<form action='/save' method='POST'><table>";
  message += "<tr><td><b>SSID:</b></td><td><input type='text' name='ssid' value='"+(ssid == "" ? server.arg("ssid") : String(ssid))+"'></td></tr>";
  message += "<tr><td><b>Password:</b></td><td><input type='text' name='password' value=''></td></tr>";
  message += "<tr><td colspan='2' align='center'><input type='submit' value='save'></td></tr>";
  message += "</table></form>";
  
  message += "</body></html>";
  server.send(200, "text/html", message);
}

void handleSave(){
  String message = "<html><head><title>ESP8266 "+WiFi.macAddress()+"</title></head><body>";
  message += "<h3>ESP8266 "+WiFi.macAddress()+"</h3>";
  
  if(server.method() == HTTP_POST && server.arg("ssid") != ""){
    Serial.println("--- New Settings ---");
    String new_ssid = server.arg("ssid");
    String new_password = server.arg("password");

    Serial.print("New SSID: ");
    Serial.println(new_ssid);
    Serial.print("New Password: ");
    Serial.println(new_password);

    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["ssid"] = new_ssid;
    json["password"] = new_password;
          
    if(!saveConfig(json)){
      Serial.println("Failed to save config");
      message += "Settings could not be saved";
    }else{
      Serial.println("Config saved");
      message += "Settings saved successfully - please reset me";
    }
  }else{
    message += "You told me something I don't understand ..";
  }

  message += "</body></html>";
  server.send(200, "text/html", message);
}

void handleNotFound(){
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  
  Serial.println("--- [404] ---");
  Serial.println(message);
  Serial.println("------");
}


//////////////////////////
// Setup
//////////////////////////
void setup() {
  Serial.begin(9600);
  Serial.println("");
  delay(1000);

  // Load Config and Connect to WiFi / setup AP
  Serial.println("Mounting FS...");
  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount file system");
    return;
  }
  if (!loadConfig()) {
    Serial.println("Failed to load config");
    setup_ap();
  } else {
    Serial.println("Config loaded");
    if(!setup_station()){
      setup_ap();
    }
  }

  // Setup Webserver
  server.on("/", handleRoot);
  server.on("/save", handleSave);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started");
}


//////////////////////////
// Loop
//////////////////////////
void loop() {
  server.handleClient();

  // Do whatever you want to do
}
