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

// Порт GPIO к которому подключена кнопка
#define PIN_BUTTON 26
// Порт GPIO с температурными сенсорами
#define ONE_WIRE_BUS 15
// Минимальный таймаут между событиями нажатия кнопки
#define TM_BUTTON 100

// Настраиваем экземпляр oneWire для связи с устройством OneWire
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// Адреса температурных датчиков
DeviceAddress sensor1 = { 0x28, 0x23, 0x2C, 0xEA, 0xC, 0x0, 0x0, 0xC1 };
DeviceAddress sensor2 = { 0x28, 0xDB, 0xE6, 0xEA, 0xC, 0x0, 0x0, 0x8D };
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
  {0, "Jan", "Feb", "Mar", "Apr", "May" "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"},
  {0, "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"},
};


//переменные для кнопки
uint32_t ms_btn = 0;
bool state_btn  = true;
void taskButton1( void *pvParameters );
SemaphoreHandle_t btn1Semaphore;


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
String Operator;
String Country;
String PrintRSRP;
int countryIndex;
int operatorIndex;

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
 * Server Index Page
 */

const char* serverIndex =
"<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
"<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
   "<input type='file' name='update'>"
        "<input type='submit' value='Update'>"
    "</form>"
 "<div id='prg'>progress: 0%</div>"
 "<script>"
  "$('form').submit(function(e){"
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
 "</script>";

/*
 * setup function
 */

void setup() 
{
  // put your setup code here, to run once:
  Serial.begin(115200);
  sensors.begin();
  delay(1000); //для стабилизации
  lcd.init(); // Инициализация дисплея
  lcd.backlight(); // Включение подсветки дисплея
  WiFi.begin(ssid, password);
  Serial.println("");
  // Ожидание подключения к сети
  while (WiFi.status() != WL_CONNECTED) 
    {
      delay(500);
      Serial.print(".");
      lcd.print(".");
    }
  Serial.println("");
  Serial.print("Connected to ");
  lcd.clear();lcd.print("Connected to ");lcd.setCursor(0,1);
  Serial.println(ssid);
  lcd.print(ssid);lcd.setCursor(0,2);
  Serial.print("my IP address: ");
  lcd.print("IP: ");
  Serial.println(WiFi.localIP());
  lcd.print(WiFi.localIP());

    /*use mdns for host name resolution*/
  if (!MDNS.begin(host)) { //http://esp32.local
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");
  /*return index page which is stored in serverIndex */
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", loginIndex);
  });
  server.on("/serverIndex", HTTP_GET, []() {
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
while (snmptime < 10000){
  getSNMPtime();
  delay(1000);
  }
snmptime = snmptime + (3600 * timezone);
setTime(snmptime);
lcd.clear();
}


void loop() {
  // put your main code here, to run repeatedly:
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
    LcdPrint (); // Вывод данных на LCD-дисплей
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
  lcd.print(int(round(sensors.getTempC(sensor1))));lcd.print(" C");
  lcd.setCursor(15,3);
  lcd.print(int(round(sensors.getTempC(sensor2))));lcd.print(" C");
}

void headerNotConnect() {
  lcd.setCursor(0,0);
  lcd.print("   Not Connected    ");
  // затираем строку со временем и датой
  lcd.setCursor(0,1);
  lcd.print("                    "); 
  delay(2000);
}

void printDigits(int digits) {
  // вспомогательная функция для печати данных о времени 
  // на монитор порта; добавляет в начале двоеточие и ноль:
    if (digits < 10)
    lcd.print('0');
  lcd.print(digits);
}

void IRAM_ATTR ISR_btn1(){
// Прерывание по кнопке, отпускаем семафор  
   xSemaphoreGiveFromISR( btn1Semaphore, NULL );
}
 
void taskButton1( void *pvParameters ){
// Определяем режим работы GPIO с кнопкой   
   pinMode(PIN_BUTTON,INPUT_PULLUP);
// Создаем семафор     
   btn1Semaphore = xSemaphoreCreateBinary();
// Сразу "берем" семафор чтобы не было первого ложного срабатывания кнопки   
   xSemaphoreTake( btn1Semaphore, 100 );
   while(true){
// Запускаем обработчик прерывания (кнопка замыкает GPIO на землю)
      attachInterrupt(PIN_BUTTON, ISR_btn1, CHANGE);   
// Ждем "отпускание" семафора
      xSemaphoreTake( btn1Semaphore, portMAX_DELAY );
// Отключаем прерывание для устранения повторного срабатывания прерывания во время обработки
      detachInterrupt(PIN_BUTTON);
      bool st = digitalRead(PIN_BUTTON);
      uint32_t ms = millis();
// Проверка изменения состояния кнопки или превышение таймаута      
      if( st != state_btn || ms - ms_btn > TM_BUTTON){
          state_btn = st;
          ms_btn    = ms;
          if( st == LOW ){
            lcd.setCursor(0,3);
            lcd.print("  change sim-slot");

            GetRequestChangeSim.addOIDPointer(callbackChangeSim);                
            GetRequestChangeSim.setIP(WiFi.localIP());                 
            GetRequestChangeSim.setUDP(&Udp);
            GetRequestChangeSim.setRequestID(rand() % 5555);                
            GetRequestChangeSim.sendTo(netAdd);               
            GetRequestChangeSim.clearOIDList();
            snmp.resetSetOccurred();
           
         }
         
// Задержка для устранения дребезга контактов
          vTaskDelay(TM_BUTTON);
      }
   }
}
