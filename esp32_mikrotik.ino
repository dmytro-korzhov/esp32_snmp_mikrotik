#include <TimeLib.h>
#include <WiFi.h>
#include <Arduino_SNMP.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Ping.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include "OneWire.h"
#include "DallasTemperature.h"
#include "U8g2lib.h"
#include "driver/gpio.h"

// конфигурация OLED дисплея
U8G2_SSD1322_NHD_256X64_F_4W_SW_SPI u8g2(U8G2_R0, 18, 23, 5, 13, 14);

// переменные для вольтметра
int analogvalue;
float input_volt = 0.0;
float temp=0.0;
float r1=82400.0; //сопротивление резистора r1
float r2=7000.0; // сопротивление резистора r2


// Порт GPIO с кнопкой смены сим
#define PIN_BUTTON 35
// Порт GPIO c кнопкой выключения дисплея
#define PIN_DISPLAY_OFF 34
// Порт GPIO c кнопкой контраста дисплея
#define PIN_CONTRAST 16
// Порт GPIO с температурными сенсорами
#define ONE_WIRE_BUS 15
// Порт ADC для подключения вольтметра
#define voltpin 33
//кнопка брелка 1
#define starlineremote_1 27
//кнопка брелка 2
#define starlineremote_2 26
//кнопка брелка 3
#define starlineremote_3 25
// Минимальный таймаут между событиями нажатия кнопки
#define TM_BUTTON 1000



// Настраиваем экземпляр oneWire для связи с устройством OneWire
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// Адреса температурных датчиков
DeviceAddress sensor_out = { 0x28, 0x23, 0x2C, 0xEA, 0xC, 0x0, 0x0, 0xC1 };
DeviceAddress sensor_int = { 0x28, 0xDB, 0xE6, 0xEA, 0xC, 0x0, 0x0, 0x8D };
void gettemp();

// настройки подключения к wi-fi
const char* host = "esp32";
const char* ssid = "Waflya";
const char* password = "9032330866";

//переменные для SNMP time
const int timezone = 3;
unsigned int snmptime = 0;
void printDigits(int digits);
void getSNMPtime();

//описание массива для корректного отображения календаря
const char *calendar[2][13] = {
  {0, "января", "февраля", "марта", "апреля", "мая", "июня", "июля", "августа", "сентября", "октября", "ноября", "декабря"},
  {0, "воскресение", "понедельник", "вторник", "среда", "четверг", "пятница", "суббота"},
};


//переменные для кнопки
uint32_t ms_btn = 0;
bool state_btn  = true;
void taskButton1( void *pvParameters );
SemaphoreHandle_t btn1Semaphore;
void change_sim();
int pwrs;
int contrast;

//параметры дисплея
LiquidCrystal_I2C lcd(0x27,20,4);

// переменные для SNMP
unsigned int rsrp = 0; //уровень сигнала
char ChangeSim[250]; //запуск скрипта смены сим-карты
char * changesim = ChangeSim;
//unsigned int sinr = 0; //уровень SINR
unsigned long timeLast = 0;

//промежуточные переменные для корректного вывода на дисплеей
String OperatorCode;
String CountryCode;
char* Operator;
char* Country;
String PrintRSRP;
int countryIndex;
int operatorIndex;
String PrintDate;
char printdate[40];
int printdatex;
String OperCount;
char opercount[15];
int printopercount;
String PrintTemp;
char printtemp[8];
int printtempx;
int forreboot = 0;

//описание массива вида [страна][оператор] для определения и вывода на дисплей имя сотового оператора
const char *operators[3][100] = {
  {0},
  {0, "MTS", "MegaFon", "NSS", 0, "ETK", "SkyLink", "Letay", "Vainah", "SkyLink", "DTC", "YOTA", "Baikal", "KUGSM", "MegaFon", "Smarts", "Miatel", "Utel", 0, 0, "Tele2", 0, 0, "Mobikom", 0, 0, 0, "Letay", "BeeLine", 0, 0, 0, 0, 0, 0, "Motiv", 0, 0, "Tambov-GSM", "Utel", 0, 0, "MTT", 0, 0, 0, 0, 0, "V-Tell", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "Tinkoff", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "BeeLine"},
  {0, "Vodafone", "BeeLine", "KyivStar", "Intertelekom", "GoldenTelekom", "Life:)", "Trimob", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "Telesystems", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "GoldenTelekom", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "Vodafone", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "KyivStar", "BeeLine", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "Phoenix"},
};


char Imsi[250]; //IMSI для получения кода оператора и страны
char * imsi = Imsi; //указатель char, используемый для ссылки на строку
int timeDelay = 5000; //задержка snmp-запросов в миллисекундах

// инициализация объектов, необходимых для SNMP
WiFiUDP Udp; // UDP используется для отправки и приема пакетов

SNMPAgent snmp = SNMPAgent("behemo");  // SNMP стартует с коммьюнити 'behemoth'
SNMPGet GetRequestImsi = SNMPGet("behemo", 0); 
SNMPGet GetRequestRsrp = SNMPGet("behemo", 0);
SNMPGet GetRequestChangeSim = SNMPGet("behemo", 0);
SNMPGet GetRequestTime = SNMPGet("behemo", 0);
//SNMPGet GetRequestSinr = SNMPGet("behemo", 0);
ValueCallback* callbackImsi;
ValueCallback* callbackRsrp;
ValueCallback* callbackTime;
ValueCallback* callbackChangeSim;
//ValueCallback* callbackSinr;

IPAddress netAdd = IPAddress(192,168,100,24); //IP-адрес устройства, с которого вы хотите получать информацию

WebServer server(80);

/*
 * Login page
 */

const char* loginIndex =
 "<form name='loginForm'>"
    "<table width='20%' bgcolor='A09F9F' align='center'>"
        "<tr>"
            "<td colspan=2>"
                "<center><font size=4><b>ESP32 Login Page</b></font></center>"
                "<br>"
            "</td>"
            "<br>"
            "<br>"
        "</tr>"
        "<tr>"
             "<td>Username:</td>"
             "<td><input type='text' size=25 name='userid'><br></td>"
        "</tr>"
        "<br>"
        "<br>"
        "<tr>"
            "<td>Password:</td>"
            "<td><input type='Password' size=25 name='pwd'><br></td>"
            "<br>"
            "<br>"
        "</tr>"
        "<tr>"
            "<td><input type='submit' onclick='check(this.form)' value='Login'></td>"
        "</tr>"
    "</table>"
"</form>"
"<script>"
    "function check(form)"
    "{"
    "if(form.userid.value=='admin' && form.pwd.value=='admin')"
    "{"
    "window.open('/serverIndex')"
    "}"
    "else"
    "{"
    " alert('Error Password or Username')/*displays error message*/"
    "}"
    "}"
"</script>";

/*
 * 
 * Server Index Page
 */
char* serverIndex =
"<div id='metrics' style='height: 400px;font-size: 500%;'></div>"
  "<form action='/startengine' method='get'>"
    "<input style='width: 99.5%;height: 80px;background:#8d95f3;cursor:pointer;border-radius: 5px;font-size:60px;font-weight:bold;margin: 5px' type='submit' value='Start Engine'>"
  "</form>"
  "<form action='/open' method='get'>"
    "<input style='width: 45%;height: 80px;background:#f38d8d;cursor:pointer;border-radius: 5px;float: left;font-size:40px;font-weight:bold;margin: 5px' type='submit' value='Open Behemoth'>"
  "</form>"
  "<form action='/close' method='get'>"
    "<input style='width: 45%;height: 80px;background:#8df39e;cursor:pointer;border-radius: 5px;float: right;font-size:40px;font-weight:bold;margin: 5px' type='submit' value='Close Behemoth'><br>"
  "</form>"
  "<form action='/find' method='get'>"
    "<input style='width: 45%;height: 80px;background:#f2ff33;cursor:pointer;border-radius: 5px;float: left;font-size:40px;font-weight:bold;margin: 5px' type='submit' value='Find Behemoth'>"
  "</form>"
  "<form action='/changesim' method='get'>"
    "<input style='width: 45%;height: 80px;background:#18d4ff;cursor:pointer;border-radius: 5px;float: right;font-size:40px;font-weight:bold;margin: 5px' type='submit' value='Change SIM'>"
  "</form>"
  "<form action='/stopengine' method='get'>"
    "<input style='width: 99.5%;height: 80px;background:#ff0000;cursor:pointer;border-radius: 5px;font-size:60px;font-weight:bold;margin: 5px' type='submit' value='Stop Engine'>"
  "</form>"
  "<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
  "<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
    "<input type='file' style='margin-top: 50px;width: 40%;height: 40px; font-size:20px' name='update'><br>"
    "<input type='submit' style='margin-top: 50px;width: 20%;height: 40px; font-size:20px' value='Update'>"
  "</form>"
  "<div id='prg' style='margin-top: 50px;font-size: 200%;float: bottom;'>progress: 0%</div><br>"
  "<script>"
  "$('#upload_form').submit(function(e){"
  "e.preventDefault();"
  "var form = $('#upload_form')[0];"
  "var data = new FormData(form);"
  " $.ajax({"
  "url: '/update',"
  "type: 'POST',"
  "data: data,"
  "contentType: false,"
  "processData:false,"
  "xhr: function() {"
  "var xhr = new window.XMLHttpRequest();"
  "xhr.upload.addEventListener('progress', function(evt) {"
  "if (evt.lengthComputable) {"
  "var per = evt.loaded / evt.total;"
  "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
  "}"
  "}, false);"
  "return xhr;"
  "},"
  "success:function(d, s) {"
  "console.log('success!')"
 "},"
 "error: function (a, b, c) {"
 "}"
 "});"
 "});"
 ""
 "setInterval(function() { "
   "$.ajax({"
     "url: '/metrics',"
     "type: 'GET',"
     "success: function(data) { $('#metrics').html(data); },"
     "error: function() { $('#metrics').html('no connection!!!'); },"
   "});"
 " }, 5000);"
 "</script>";

/*
 * setup function
 */

void setup() 
{
  u8g2.begin(); // инициализация OLED дисплея
  u8g2.enableUTF8Print();
  u8g2.setFont(u8g2_font_cu12_t_cyrillic);

  // put your setup code here, to run once:
  Serial.begin(115200);
  sensors.begin();
  delay(1000); //для стабилизации
  //lcd.init(); // Инициализация дисплея
  //lcd.backlight(); // Включение подсветки дисплея
  WiFi.begin(ssid, password);
  Serial.println("");
  // Ожидание подключения к сети
  while (WiFi.status() != WL_CONNECTED) 
    {
      delay(500);
      Serial.print(".");
      u8g2.setCursor(0, 15);
      u8g2.print(".");
      u8g2.sendBuffer();
      forreboot=forreboot + 1;
      if (forreboot > 20) {
        ESP.restart();
      }
      
    }
  Serial.println("");
  Serial.print("Connected to ");
//  lcd.clear();lcd.print("Connected to ");lcd.setCursor(0,1);
  u8g2.clear();u8g2.setCursor(0, 15);
  u8g2.print("Connected to ");
  Serial.println(ssid);
  u8g2.print(ssid);u8g2.setCursor(0,30);
  Serial.print("my IP address: ");
  u8g2.print("IP: ");
  Serial.println(WiFi.localIP());
  u8g2.print(WiFi.localIP());
  u8g2.sendBuffer();
  delay(2000);

    /*use mdns for host name resolution*/
  if (!MDNS.begin(host)) { //http://esp32.local
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");
  /*return index page which is stored in serverIndex */
  /*
   * 
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", loginIndex);
  });
  */
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });
  /*handling uploading firmware file */
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });
  server.on("/changesim", HTTP_GET, []() {
    change_sim();
    server.sendHeader("Location", "/",true);
    server.send(302, "text/plane",""); 
  });
  server.on("/metrics", HTTP_GET, []() {
    server.send(200, "text/html", metrics());  
  });
  server.on("/startengine", HTTP_GET, []() {
    pinMode(starlineremote_1, OUTPUT);
    digitalWrite(starlineremote_1, LOW);
    delay(3000);
    pinMode(starlineremote_1, INPUT);
    server.sendHeader("Location", "/",true);
    server.send(302, "text/plane",""); 
  });
  server.on("/close", HTTP_GET, []() {
    pinMode(starlineremote_1, OUTPUT);
    digitalWrite(starlineremote_1, LOW);
    delay(500);
    pinMode(starlineremote_1, INPUT);
    server.sendHeader("Location", "/",true);
    server.send(302, "text/plane",""); 
  });
  server.on("/open", HTTP_GET, []() {
    pinMode(starlineremote_2, OUTPUT);
    digitalWrite(starlineremote_2, LOW);
    delay(500);
     pinMode(starlineremote_2, INPUT);
    server.sendHeader("Location", "/",true);
    server.send(302, "text/plane","");
  });
  server.on("/find", HTTP_GET, []() {
    pinMode(starlineremote_3, OUTPUT);
    digitalWrite(starlineremote_3, LOW);
    delay(500);
    pinMode(starlineremote_3, INPUT);
    server.sendHeader("Location", "/",true);
    server.send(302, "text/plane",""); 
  });
  server.on("/stopengine", HTTP_GET, []() {
    pinMode(starlineremote_1, OUTPUT);
    digitalWrite(starlineremote_1, LOW);
    delay(1500);
    pinMode(starlineremote_1, INPUT);
    delay(500);
    pinMode(starlineremote_2, OUTPUT);
    digitalWrite(starlineremote_2, LOW);
    delay(500);
    pinMode(starlineremote_2, INPUT);
    server.sendHeader("Location", "/",true);
    server.send(302, "text/plane",""); 
  });
  
  server.begin();

  snmp.setUDP(&Udp);// дать snmp указатель на объект UDP
  snmp.begin();// запустить прослушиватель SNMP
  snmp.beginMaster(); // запустить отправителя SNMP

  
  

snmp.addIntegerHandler(".1.3.6.1.4.1.14988.1.1.16.1.1.4.2", &rsrp, true); // OID для получения RSRP
callbackRsrp = snmp.findCallback(".1.3.6.1.4.1.14988.1.1.16.1.1.4.2", false);
snmp.addStringHandler(".1.3.6.1.4.1.14988.1.1.16.1.1.12.2", &imsi, true);  // OID для получения IMSI
callbackImsi = snmp.findCallback(".1.3.6.1.4.1.14988.1.1.16.1.1.12.2", false);
snmp.addStringHandler(".1.3.6.1.4.1.14988.1.1.18.1.1.2.4", &changesim, true);  // OID для запуска скрипта смены сим-слота
callbackChangeSim = snmp.findCallback(".1.3.6.1.4.1.14988.1.1.18.1.1.2.4", false);
snmp.addIntegerHandler(".1.3.6.1.4.1.14988.1.1.12.1.0", &snmptime, true); // OID для получения времени
callbackTime = snmp.findCallback(".1.3.6.1.4.1.14988.1.1.12.1.0", false);

//snmp.addIntegerHandler(".1.3.6.1.4.1.14988.1.1.16.1.1.7.2", &sinr, true); // OID для получения SINR
//callbackSinr = snmp.findCallback(".1.3.6.1.4.1.14988.1.1.16.1.1.7.2", false);

// Запускается задача работы с кнопкой   
   xTaskCreateUniversal(taskButton1, "btn1", 4096, NULL, 5, NULL,1);

  // установка системного времени, полученного по SNMP
while (snmptime < 60){ // ждем минуту для установки времени, если время не устанавливается, идем дальше
  snmptime = snmptime + 1;
  getSNMPtime();
  delay(1000);
  }
snmptime = snmptime + (3600 * timezone);
setTime(snmptime);
lcd.clear();
}


void loop() {
  // put your main code here, to run repeatedly:
  voltmeter();
  snmp.loop();
  getSNMP();
  server.handleClient();
  sensors.requestTemperatures();
  delay(500);
}

void getSNMP(){
  //проверяем, не пора ли отправить запрос SNMP
  //слишком частые запросы могут вызвать проблемы
  if((timeLast + timeDelay) <= millis()){
    intermediateCalc (); // производим промежуточные вычисления
    //проверяем mikrotik на доступность
    bool success = Ping.ping("192.168.100.24", 2);
    if(!success){
      headerNotConnect();    
    }
    else {
//    SerialPrint (); // Вывод данных в COM-port
    //LcdPrint (); // Вывод данных на LCD-дисплей
    OledPrint (); // Вывод данных на OLED-дисплей
    }

    GetRequestRsrp.addOIDPointer(callbackRsrp);                
    GetRequestRsrp.setIP(WiFi.localIP());                 
    GetRequestRsrp.setUDP(&Udp);
    GetRequestRsrp.setRequestID(rand() % 5555);                
    GetRequestRsrp.sendTo(netAdd);               
    GetRequestRsrp.clearOIDList();
    snmp.resetSetOccurred();


    
/*    GetRequestSinr.addOIDPointer(callbackSinr);                
    GetRequestSinr.setIP(WiFi.localIP());                 
    GetRequestSinr.setUDP(&udp);
    GetRequestSinr.setRequestID(rand() % 5555);                
    GetRequestSinr.sendTo(netAdd);               
    GetRequestSinr.clearOIDList();
    snmp.resetSetOccurred();
*/      
    GetRequestImsi.addOIDPointer(callbackImsi);                
    GetRequestImsi.setIP(WiFi.localIP());                 
    GetRequestImsi.setUDP(&Udp);
    GetRequestImsi.setRequestID(rand() % 5555);                
    GetRequestImsi.sendTo(netAdd);               
    GetRequestImsi.clearOIDList();
    snmp.resetSetOccurred();

    timeLast = millis();
  }  
  
}

void getSNMPtime() {
  snmp.loop();
    GetRequestRsrp.addOIDPointer(callbackTime);                
    GetRequestRsrp.setIP(WiFi.localIP());                 
    GetRequestRsrp.setUDP(&Udp);
    GetRequestRsrp.setRequestID(rand() % 5555);                
    GetRequestRsrp.sendTo(netAdd);               
    GetRequestRsrp.clearOIDList();
    snmp.resetSetOccurred();
}

void intermediateCalc () { // промежуточные вычисления для корректного вывода

  OperatorCode = (imsi[3]);OperatorCode += (imsi[4]); // выделение из IMSI код оператора
  operatorIndex = OperatorCode.toInt();
  CountryCode = (imsi[0]);CountryCode += (imsi[1]);CountryCode += (imsi[2]); // выделение из IMSI код страны
  
  if (CountryCode == "250") {
    Country = "RU";
    countryIndex = 1; // строка в массиве operators
  }
  else if (CountryCode == "255"){
    Country = "UA";
    countryIndex = 2;
  }
  else {
    Country = "No Country";
    countryIndex = 0;
  }
  if ((UINT16_MAX-rsrp+1) > 115) {
      PrintRSRP = ("Low RSRP");
  }
  else {
   PrintRSRP = ("-");PrintRSRP += (String(UINT16_MAX-rsrp+1));PrintRSRP += (" dBm");
   }
  
}

void SerialPrint () {
  Serial.print("IMSI: ");Serial.print(imsi);Serial.println(); 
  Serial.print("RSRP: ");Serial.print(PrintRSRP);Serial.println();
  Serial.print("Operator Code: ");Serial.print(OperatorCode);Serial.println();
  Serial.print("Country Code: ");Serial.print(CountryCode);Serial.println();
  Serial.print("Country: ");Serial.print(Country);Serial.println();
  Serial.print("Operator: ");Serial.print(operators[countryIndex][operatorIndex]);Serial.println();
//  Serial.print("SINR: ");Serial.print(sinr);;Serial.println();
  Serial.println("----------------------");
}

void LcdPrint () {
  lcd.clear();
  lcd.print(PrintRSRP);
  lcd.setCursor(9,0);
  lcd.print(operators[countryIndex][operatorIndex]);lcd.print(" ");lcd.print(Country);
  lcd.setCursor(0,1);
  printDigits(hour());lcd.print(":");printDigits(minute());lcd.print("  ");printDigits(day());lcd.print("  ");lcd.print(calendar[0][month()]);lcd.print("  ");lcd.print(calendar[1][weekday()]);
  lcd.setCursor(0,3);
  lcd.print(int(round(sensors.getTempC(sensor_int))));lcd.print(" C");
  lcd.setCursor(15,3);
  lcd.print(int(round(sensors.getTempC(sensor_out))));lcd.print(" C");
}

void OledPrint () {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_cu12_t_cyrillic);
  u8g2.setCursor(0,15); u8g2.print(PrintRSRP);
    OperCount = (operators[countryIndex][operatorIndex]);OperCount += (" ");OperCount += (Country);
    OperCount.toCharArray(opercount, 15);
    printopercount = (256 - (u8g2.getUTF8Width(opercount)));
  u8g2.setCursor(printopercount, 15);
  u8g2.print(OperCount);
    PrintDate = (day());PrintDate += (" ");PrintDate += (calendar[0][month()]);PrintDate += (", ");PrintDate += (calendar[1][weekday()]);
    PrintDate.toCharArray(printdate,40);
    printdatex=((256 - (u8g2.getUTF8Width(printdate)))/2);
  u8g2.setCursor(printdatex, 60);
  u8g2.print(PrintDate);
  u8g2.setCursor(0,37);
  u8g2.setFont(u8g2_font_cu12_tf);
  u8g2.print(int(round(sensors.getTempC(sensor_out))));u8g2.print("°C");
  PrintTemp = (int(round(sensors.getTempC(sensor_int))));PrintTemp += ("°C");
  PrintTemp.toCharArray(printtemp,15);
  printtempx=(256-u8g2.getUTF8Width(printtemp));
  u8g2.setCursor(printtempx,37);
  u8g2.print(PrintTemp);
  u8g2.setFont(u8g2_font_cu12_t_cyrillic);
  u8g2.setCursor(0,60);
  u8g2.print(input_volt);
  u8g2.print(" V");
  //u8g2.print(analogvalue);
//  u8g2.drawUTF8(0, 30, "Как заебала эта хуйня!");
  u8g2.setFont(u8g2_font_timB24_tn);
  u8g2.setCursor(90, 45);
  printDigits(hour());u8g2.print(":");printDigits(minute());
  u8g2.sendBuffer();
}

void headerNotConnect() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_cu12_t_cyrillic);
  u8g2.setCursor(81,15);
  u8g2.print("Not Connected");
  u8g2.sendBuffer();
  forreboot=forreboot + 1;
      if (forreboot > 10) {
        ESP.restart();
      }
  delay(2000);
}

void printDigits(int digits) {
  // вспомогательная функция для печати данных о времени 
  // на монитор порта; добавляет в начале двоеточие и ноль:
    if (digits < 10)
    u8g2.print('0');
  u8g2.print(digits);
}

void IRAM_ATTR ISR_btn1(){
// Прерывание по кнопке, отпускаем семафор  
   xSemaphoreGiveFromISR( btn1Semaphore, NULL );
}
 
void taskButton1( void *pvParameters ){
// Определяем режим работы GPIO с кнопкой   
   pinMode(PIN_BUTTON,INPUT_PULLUP);
   pinMode(PIN_DISPLAY_OFF,INPUT_PULLUP);
   pinMode(PIN_CONTRAST,INPUT_PULLUP);
   // Создаем семафор     
   btn1Semaphore = xSemaphoreCreateBinary();
// Сразу "берем" семафор чтобы не было первого ложного срабатывания кнопки   
   xSemaphoreTake( btn1Semaphore, 100 );
   while(true){
// Запускаем обработчик прерывания (кнопка замыкает GPIO на землю)
      attachInterrupt(PIN_BUTTON, ISR_btn1, CHANGE);
      attachInterrupt(PIN_DISPLAY_OFF, ISR_btn1, CHANGE);
      attachInterrupt(PIN_CONTRAST, ISR_btn1, CHANGE);
// Ждем "отпускание" семафора
      xSemaphoreTake( btn1Semaphore, portMAX_DELAY );
// Отключаем прерывание для устранения повторного срабатывания прерывания во время обработки
      detachInterrupt(PIN_BUTTON);
      detachInterrupt(PIN_DISPLAY_OFF);
      detachInterrupt(PIN_CONTRAST);
      bool st = digitalRead(PIN_BUTTON);
      bool st1 = digitalRead(PIN_DISPLAY_OFF);
      bool st2 = digitalRead(PIN_CONTRAST);
      uint32_t ms = millis();
// Проверка изменения состояния кнопки или превышение таймаута      
      if( st != state_btn || ms - ms_btn > TM_BUTTON){
          state_btn = st;
          ms_btn    = ms;
          if( st == LOW ){
           change_sim();           
         }
      }
      if( st1 != state_btn || ms - ms_btn > TM_BUTTON){
          state_btn = st1;
          ms_btn    = ms;
          if( st1 == LOW ){
           poweroffdisplay();           
         }
      }
      if( st2 != state_btn || ms - ms_btn > TM_BUTTON){
          state_btn = st1;
          ms_btn    = ms;
          if( st2 == LOW ){
           dayornight();           
         }
      }
         
// Задержка для устранения дребезга контактов
          vTaskDelay(TM_BUTTON);
   }
}


void change_sim() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_cu12_t_cyrillic);
  u8g2.setCursor(75,15);
  u8g2.print("Change Sim-slot");
  u8g2.sendBuffer();
  delay(2000);
  
  GetRequestChangeSim.addOIDPointer(callbackChangeSim);                
  GetRequestChangeSim.setIP(WiFi.localIP());                 
  GetRequestChangeSim.setUDP(&Udp);
  GetRequestChangeSim.setRequestID(rand() % 5555);                
  GetRequestChangeSim.sendTo(netAdd);               
  GetRequestChangeSim.clearOIDList();
  snmp.resetSetOccurred();
}

String metrics() {
  String result;
  result += "Temp in : " + String(int(round(sensors.getTempC(sensor_int))));result += " С";result += "<br>";
  result += "Temp out: " + String(int(round(sensors.getTempC(sensor_out))));result += " С";result += "<br>";
  result += "RSSI: " + PrintRSRP;result += "    ";result += operators[countryIndex][operatorIndex];result += " ";result += Country;result += "<br>";
  result += "Voltage : " + String(input_volt);result += " V";result += "<br>";
  return result;
}

void voltmeter() {
  analogvalue = analogRead(voltpin);
  input_volt = (((analogvalue * 3.3 ) / 4095) / (r2/(r1+r2)));
  if (input_volt < 0.1)
{
input_volt=0.0;
}
}

void poweroffdisplay() {
  if (pwrs == 0) {
    pwrs = 1;
  }
  else {
    pwrs = 0;
  }
  Serial.print(pwrs);
  u8g2.setPowerSave(pwrs);
 }

void dayornight() {
  if (contrast == 0) {
    contrast = 255;
  }
  else {
    contrast = 0;
  }
  u8g2.setContrast(contrast);
}
