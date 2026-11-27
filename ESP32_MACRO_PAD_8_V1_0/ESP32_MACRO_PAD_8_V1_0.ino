#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <BleKeyboard.h>
#include "WebUI.h"

const uint8_t pins[]={0,1,3,4,5,6,7,10};
const uint8_t LED=8, BOOT=9;
const char* SSID="ESP32-MacroPad";
WebServer server(80);
BleKeyboard keyboard("Macro Pad 8","DIY",100);
DynamicJsonDocument cfg(65536);
bool ap=false, fsReady=false; uint32_t lastWeb=0, restartAt=0;
struct Button {bool raw=false, stable=false, held=false, pending=false; uint32_t edge=0, down=0, released=0;};
Button buttons[8], boot;
struct Job {String type, cmd;};
Job queue[8]; uint8_t qread=0,qwrite=0;
String steps; unsigned int pos=0; uint32_t due=0; bool running=false;
int active(){return cfg["active"].as<int>();}
void defaults(){
 cfg.clear(); cfg["version"]=1; cfg["active"]=0; cfg["name"]="Macro Pad 8"; cfg["password"]="Macro1234"; cfg["doubleMs"]=250; cfg["holdMs"]=650; cfg["debounceMs"]=25;
 const char* names[]={"Windows","Office","Design","Multimedia","Custom"};
 const char* commands[8][3]={{"CTRL+C","CTRL+A","CTRL+S"},{"CTRL+V","CTRL+ALT+V","F12"},{"CTRL+X","DELETE","CTRL+SHIFT+ESC"},{"CTRL+Z","CTRL+Y","ALT+F4"},{"excel","winword","outlook"},{"calc","chrome","explorer"},{"WIN+SHIFT+S","CTRL+P","WIN+D"},{"WIN+L","MUTE","PLAY"}};
 JsonArray profiles=cfg.createNestedArray("profiles");
 for(int p=0;p<5;p++){JsonObject pr=profiles.createNestedObject();pr["name"]=names[p];JsonArray keys=pr.createNestedArray("keys");for(int k=0;k<8;k++){JsonObject b=keys.createNestedObject();b["double"]=p==0;JsonArray a=b.createNestedArray("actions");for(int m=0;m<3;m++){JsonObject x=a.createNestedObject();x["type"]=p==0?((k==4||k==5)?"app":(k==7&&m>0?"media":"shortcut")):"none";x["cmd"]=p==0?commands[k][m]:"";x["name"]=p==0?commands[k][m]:"";}}}
}
bool strOK(JsonVariantConst v,size_t max){return v.is<const char*>() && strlen(v.as<const char*>())<=max;}
bool printable(JsonVariantConst v,size_t max){if(!strOK(v,max))return false;const char* s=v.as<const char*>();for(;*s;s++)if((unsigned char)*s<32||(unsigned char)*s>126)return false;return true;}
bool valid(JsonDocument& d){
 if(d.overflowed())return false;
 if(d["version"]!=1||!d["active"].is<int>()||d["active"].as<int>()<0||d["active"].as<int>()>4)return false;
 if(!printable(d["name"],20)||strlen(d["name"].as<const char*>())==0||!printable(d["password"],63)||strlen(d["password"].as<const char*>())<8)return false;
 const char* t[]={"doubleMs","holdMs","debounceMs"};int lo[]={100,350,10},hi[]={600,2000,80};for(int i=0;i<3;i++)if(!d[t[i]].is<int>()||d[t[i]].as<int>()<lo[i]||d[t[i]].as<int>()>hi[i])return false;
 if(d["holdMs"].as<int>()<=d["doubleMs"].as<int>())return false;
 if(d["profiles"].size()!=5||!d["profiles"].is<JsonArrayConst>())return false;
 for(JsonObjectConst p:d["profiles"].as<JsonArrayConst>()){if(!strOK(p["name"],40)||!p["keys"].is<JsonArrayConst>()||p["keys"].size()!=8)return false;for(JsonObjectConst k:p["keys"].as<JsonArrayConst>()){if(!k["double"].is<bool>()||!k["actions"].is<JsonArrayConst>()||k["actions"].size()!=3)return false;for(JsonObjectConst a:k["actions"].as<JsonArrayConst>()){if(!strOK(a["name"],40)||!strOK(a["type"],12)||!strOK(a["cmd"],240))return false;String type=a["type"].as<String>();if(type!="none"&&type!="shortcut"&&type!="app"&&type!="url"&&type!="file"&&type!="text"&&type!="media"&&type!="macro"&&type!="profile")return false;}}}return true;
}
bool save(JsonDocument& d){if(!fsReady)return false;File f=LittleFS.open("/config.tmp","w");if(!f)return false;size_t expected=measureJson(d);size_t n=serializeJson(d,f);f.close();if(n!=expected){LittleFS.remove("/config.tmp");return false;}LittleFS.remove("/config.bak");if(LittleFS.exists("/config.json")&&!LittleFS.rename("/config.json","/config.bak"))return false;if(!LittleFS.rename("/config.tmp","/config.json")){LittleFS.rename("/config.bak","/config.json");return false;}return true;}
bool load(const char* path){File f=LittleFS.open(path,"r");if(!f)return false;auto e=deserializeJson(cfg,f);f.close();return !e&&valid(cfg);}
uint8_t keyCode(String s){s.trim();s.toUpperCase();if(s.length()==1){char c=s[0];if(c>='A'&&c<='Z')return c+32;return c;}if(s=="CTRL")return KEY_LEFT_CTRL;if(s=="ALT")return KEY_LEFT_ALT;if(s=="SHIFT")return KEY_LEFT_SHIFT;if(s=="WIN"||s=="GUI")return KEY_LEFT_GUI;if(s=="ENTER")return KEY_RETURN;if(s=="ESC")return KEY_ESC;if(s=="TAB")return KEY_TAB;if(s=="SPACE")return ' ';if(s=="DELETE")return KEY_DELETE;if(s=="BACKSPACE")return KEY_BACKSPACE;if(s=="UP")return KEY_UP_ARROW;if(s=="DOWN")return KEY_DOWN_ARROW;if(s=="LEFT")return KEY_LEFT_ARROW;if(s=="RIGHT")return KEY_RIGHT_ARROW;if(s=="HOME")return KEY_HOME;if(s=="END")return KEY_END;if(s=="PAGEUP")return KEY_PAGE_UP;if(s=="PAGEDOWN")return KEY_PAGE_DOWN;if(s.startsWith("F")){int n=s.substring(1).toInt();if(n>=1&&n<=12&&s=="F"+String(n))return KEY_F1+n-1;}return 0;}
void shortcut(String s){uint8_t codes[6];int count=0;while(s.length()){int i=s.indexOf('+');String part=i<0?s:s.substring(0,i);uint8_t code=keyCode(part);if(!code||count>=6)return;codes[count++]=code;if(i<0)break;s=s.substring(i+1);}for(int i=0;i<count;i++)keyboard.press(codes[i]);delay(12);keyboard.releaseAll();}
void media(String s){s.trim();s.toUpperCase();if(s=="MUTE")keyboard.write(KEY_MEDIA_MUTE);else if(s=="PLAY")keyboard.write(KEY_MEDIA_PLAY_PAUSE);else if(s=="VOLUP")keyboard.write(KEY_MEDIA_VOLUME_UP);else if(s=="VOLDOWN")keyboard.write(KEY_MEDIA_VOLUME_DOWN);else if(s=="NEXT")keyboard.write(KEY_MEDIA_NEXT_TRACK);else if(s=="PREV")keyboard.write(KEY_MEDIA_PREVIOUS_TRACK);}
void typeText(const String& s){for(unsigned int i=0;i<s.length()&&keyboard.isConnected();i++){uint8_t c=s[i];if(c>=32&&c<=126)keyboard.write(c);else if(c=='\n')keyboard.write(KEY_RETURN);}}
void enqueue(int k,int m){JsonObject a=cfg["profiles"][active()]["keys"][k]["actions"][m];String type=a["type"].as<String>();if(type=="none")return;if(type=="profile"){String s=a["cmd"].as<String>();int p=s.toInt();if(p>=1&&p<=5){cfg["active"]=p-1;save(cfg);for(auto &b:buttons)b.pending=false;}return;}if(!keyboard.isConnected())return;uint8_t next=(qwrite+1)%8;if(next==qread)return;queue[qwrite]={type,a["cmd"].as<String>()};qwrite=next;}
void serviceJobs(){if(!keyboard.isConnected()){qread=qwrite;running=false;return;}if(running){if((int32_t)(millis()-due)<0)return;int end=steps.indexOf('\n',pos);String line=end<0?steps.substring(pos):steps.substring(pos,end);pos=end<0?steps.length()+1:end+1;line.trim();if(line.startsWith("WAIT ")){long ms=line.substring(5).toInt();due=millis()+constrain(ms,0L,10000L);}else if(line.startsWith("TEXT "))typeText(line.substring(5));else if(line.startsWith("KEY "))shortcut(line.substring(4));else if(line.startsWith("MEDIA "))media(line.substring(6));if(pos>steps.length())running=false;return;}if(qread==qwrite)return;Job j=queue[qread];qread=(qread+1)%8;if(j.type=="shortcut")shortcut(j.cmd);else if(j.type=="media")media(j.cmd);else if(j.type=="text")typeText(j.cmd);else if(j.type=="macro"){steps=j.cmd;pos=0;due=millis();running=true;}else if(j.type=="app"||j.type=="url"||j.type=="file"){if(j.cmd.indexOf('\n')>=0||j.cmd.indexOf('\r')>=0)return;shortcut("WIN+R");steps="WAIT 450\nTEXT "+j.cmd+"\nKEY ENTER";pos=0;due=millis();running=true;}}
void startAP(){if(ap)return;WiFi.mode(WIFI_AP);WiFi.softAPConfig(IPAddress(192,168,4,1),IPAddress(192,168,4,1),IPAddress(255,255,255,0));ap=WiFi.softAP(SSID,cfg["password"].as<const char*>());if(ap){server.begin();lastWeb=millis();}}
void stopAP(){server.stop();WiFi.softAPdisconnect(true);WiFi.mode(WIFI_OFF);ap=false;}
void touchWeb(){lastWeb=millis();server.sendHeader("Cache-Control","no-store");}
void routes(){server.on("/",HTTP_GET,[]{touchWeb();server.send_P(200,"text/html; charset=utf-8",WEB_UI);});server.on("/api/config",HTTP_GET,[]{touchWeb();String s;serializeJson(cfg,s);server.send(200,"application/json",s);});server.on("/api/config",HTTP_POST,[]{touchWeb();if(server.arg("plain").length()>55000){server.send(413,"text/plain","Too large");return;}DynamicJsonDocument incoming(65536);if(deserializeJson(incoming,server.arg("plain"))||!valid(incoming)){server.send(400,"text/plain","Invalid configuration");return;}bool reboot=cfg["name"].as<String>()!=incoming["name"].as<String>()||cfg["password"].as<String>()!=incoming["password"].as<String>();if(!save(incoming)){server.send(500,"text/plain","Storage failed");return;}cfg.set(incoming);for(auto &b:buttons)b.pending=false;server.send(200,"application/json",reboot?"{\"reboot\":true}":"{\"reboot\":false}");if(reboot)restartAt=millis()+1500;});server.onNotFound([]{server.send(404,"text/plain","Not found");});}
void scan(){uint32_t now=millis();int db=cfg["debounceMs"], dbl=cfg["doubleMs"], hold=cfg["holdMs"];
 for(int i=0;i<8;i++){Button& b=buttons[i];bool raw=!digitalRead(pins[i]);if(raw!=b.raw){b.raw=raw;b.edge=now;}if(raw!=b.stable&&now-b.edge>=db){b.stable=raw;if(raw){if(b.pending&&now-b.released>=dbl){b.pending=false;enqueue(i,0);}b.down=now;b.held=false;}else if(!b.held){bool enabled=cfg["profiles"][active()]["keys"][i]["double"].as<bool>();if(!enabled){enqueue(i,0);}else if(b.pending){b.pending=false;enqueue(i,1);}else{b.pending=true;b.released=now;}}}if(b.stable&&!b.held&&now-b.down>=hold){if(b.pending){b.pending=false;enqueue(i,0);}b.held=true;enqueue(i,2);}if(b.pending&&!b.stable&&now-b.released>=dbl){b.pending=false;enqueue(i,0);}}
 bool raw=!digitalRead(BOOT);if(raw!=boot.raw){boot.raw=raw;boot.edge=now;}if(raw!=boot.stable&&now-boot.edge>=db){boot.stable=raw;boot.down=now;boot.held=false;}if(boot.stable&&!boot.held&&now-boot.down>=3000){boot.held=true;if(ap)stopAP();else startAP();}}
void setup(){Serial.begin(115200);for(auto p:pins)pinMode(p,INPUT_PULLUP);pinMode(BOOT,INPUT_PULLUP);pinMode(LED,OUTPUT);digitalWrite(LED,HIGH);fsReady=LittleFS.begin(true);if(!fsReady||(!load("/config.json")&&!load("/config.bak"))){defaults();save(cfg);}keyboard.setName(cfg["name"].as<String>().c_str());keyboard.begin();routes();WiFi.mode(WIFI_OFF);}
void loop(){scan();if(ap){server.handleClient();if(millis()-lastWeb>=300000)stopAP();}serviceJobs();uint32_t t=millis();bool lit=ap?(t%1000<100||(t%1000>=200&&t%1000<300)):keyboard.isConnected()?true:t%1000<150;digitalWrite(LED,lit?LOW:HIGH);if(restartAt&&(int32_t)(t-restartAt)>=0)ESP.restart();delay(2);}
