#include "WiFiEsp.h"
#include <Adafruit_GFX.h>
#include <RGBmatrixPanel.h>
#include "WiFiEspUdp.h"
#include <NTPClient.h>
#include <Time.h>
#include <RTClib.h>
#include "WifiCredentials.h"
#include <TimeLib.h>
#include <Wire.h>
#include <SHT21.h>
#include <RGBmatrixPanel.h>
#include <SPI.h>
#include <HTTPClient.h>
#include <TextFinder.h>
#include "Arduino.h"

#define CLK 11 // USE THIS ON ARDUINO MEGA
#define OE 9
#define LAT 10
#define A A0
#define B A1
#define C A2
#define D A3

// Definizione del pin a cui è collegato il sensore LDR
#define LDR_PIN A4 // Pin analogico utilizzato per il LDR

#define BUZZER_PIN 2

SHT21 sht; // Crea un'istanza della classe SHT2x

#define luminosita 1

#define NEWS_ITEMS_VIEWED 20

int ldrValue = 20;

WiFiEspClient client;
TextFinder finder(client);

// RSS Feed URL
// const char *rssFeedUrl = "https://news.google.com/rss?hl=it&gl=IT&ceid=IT:it";

// WiFiEspClient wifiClient;
// HttpClient httpClient = HttpClient(wifiClient, "news.google.com", 443);

RGBmatrixPanel matrix(A, B, C, D, CLK, LAT, OE, false, 64);

// standard colors
uint16_t myRED = matrix.Color333(7, 0, 0);
uint16_t myGREEN = matrix.Color333(0, 7, 0);
uint16_t myBLUE = matrix.Color333(0, 0, 7);
uint16_t myWHITE = matrix.Color333(7, 7, 7);
uint16_t myYELLOW = matrix.Color333(7, 7, 0);
uint16_t myCYAN = matrix.Color333(0, 7, 7);
uint16_t myMAGENTA = matrix.Color333(7, 0, 7);
uint16_t myShadow = matrix.Color333(4, 0, 7);
uint16_t myROSE = matrix.Color333(7, 0, 4);
uint16_t myBLACK = matrix.Color333(0, 0, 0);

char buffer_news_titolo[300];
// char buffer_news_descr[300];

const char *months[] = {"Gen", "Feb", "Mar", "Apr", "Mag", "Giu", "Lug", "Ago", "Set", "Ott", "Nov", "Dic"};
const char *wd[7] = {"Domenica", "Lunedi'", "Martedi'", "Mercoledi'", "Giovedi'", "Venerdi'", "Sabato"};

enum DataGiornoState
{
  DATA,
  GIORNO
}; // Stati possibili
DataGiornoState dataGiornoState = DATA; // Stato iniziale

enum TempHumidState
{
  TEMPERATURA,
  UMIDITA
};
TempHumidState tempHumidState = TEMPERATURA;

int h, m, s = -1;
uint16_t d, yr, mese, dow;

DateTime t;

String tempHum = "Temp: ?,Umid: ?";

int xFirstRow = 64;

int status = WL_IDLE_STATUS; // the Wifi radio's status

unsigned long lastUpdateDateTime = 0; // Vecchio valore di millis()
unsigned long lastUpdateTempHum = 0;  // Vecchio valore di millis()
unsigned long Last_UPDATE_DataGiorno = 0;
unsigned long Last_UPDATE_TempHumid = 0;

unsigned long lastReadingLDR = 0;

time_t epochTime; // Secondi trascorsi dal 01/01/1970

boolean newsDisplayed = false;

String arrayNotizie[NEWS_ITEMS_VIEWED]; // = {"prima news", "seconda news", "terza news", "quarta news", "quinta news"};
uint8_t indiceArrayNotizie = 0;

RTC_DS3231 rtc;

WiFiEspUDP ntpUDP;
// Crea un client NTP
NTPClient timeClient(ntpUDP, "it.pool.ntp.org", 3600, 60000); // UTC offset e intervallo di aggiornamento

uint32_t dateTimeUpdateInterval = 900000;
uint32_t tempHumUpdateInterval = 30000;
uint32_t ldrUpdateInterval = 60000;

/*
#define POLL_INTERVAL 2 // Time between searches (minutes)
#define MIN_TIME 5      // Skip arrivals sooner than this (minutes)
#define READ_TIMEOUT 15 // Cancel query if no data received (seconds)

uint32_t seconds[2];
*/

// Sostituisce i caratteri accentati
String replaceAccentedCharacters(const String &input)
{
  String output = input;

  output.replace("à", "a'");
  output.replace("è", "e'");
  output.replace("ì", "i'");
  output.replace("ò", "o'");
  output.replace("ù", "u'");

  return output;
}

void fetchRSSFeed()
{
  char dataServer[] = "www.ansa.it";
  char dataPage[] = "/lazio/notizie/lazio_rss.xml";

  Serial.print(">> Connecting to ");
  Serial.print(dataServer);
  Serial.println(dataPage);
  if (!client.connect(dataServer, 80))
  {
    Serial.println(">> Connection Failed !");
    client.stop();
    return;
  }
  else
  {
    Serial.println("Connection established !");
  }

  String url = dataPage;

  Serial.print(">> Requesting URL: ");
  Serial.println(dataPage);

  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "User-Agent: Mozilla/4.0\r\n" +
               "Connection: close\r\n\r\n");
  // unsigned long timeout = millis();

  if (client.connected())
  {

    for (int i = 0; i < NEWS_ITEMS_VIEWED; i++)
    {
      Serial.print("news ");
      Serial.print(i);
      Serial.print(": ");
      finder.find("<title>");
      finder.find("<![CDATA");
      finder.getString("[", "]", buffer_news_titolo, sizeof(buffer_news_titolo));

      /*
      finder.find("<description>");
      finder.find("<![CDATA");
      finder.getString("[", "]", buffer_news_descr, sizeof(buffer_news_descr));
      */
      Serial.println(buffer_news_titolo);
      String replaced = replaceAccentedCharacters(buffer_news_titolo);
      arrayNotizie[i] = replaced;
      // Serial.print("-");
      // Serial.println(buffer_news_descr);
    }

    client.stop();
    client.flush();
    // yield();
    Serial.println();
  }
}

void printWifiStatus()
{
  // print the SSID of the network you're attached to
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength
  long rssi = WiFi.RSSI();
  Serial.print("Signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

String printDateTime(DateTime dt)
{
  char result[50];
  char dateBuffer[] = "DD/MM/YYYY ";
  char timeBuffer[] = "hh:mm:ss";

  Serial.print("Ora da modulo RTC => ");
  Serial.print(dt.toString(dateBuffer));
  Serial.println(dt.toString(timeBuffer));

  strcpy(result, dt.toString(dateBuffer));
  strcat(result, dt.toString(timeBuffer));

  return (result);
}

void playSound()
{
  for (int i = 0; i < 100; i++)
  {                                 // make a sound
    digitalWrite(BUZZER_PIN, HIGH); // send high signal to buzzer
    delay(1);                       // delay 1ms
    digitalWrite(BUZZER_PIN, LOW);  // send low signal to buzzer
    delay(1);
  }
}

void setup()
{
  // initialize serial for debugging
  Serial.begin(9600);
  // initialize serial for ESP module
  Serial1.begin(9600);

  Wire.begin(); // Inizializza il bus I2C

  /*
    if (!sht.begin()) {  // Inizializza il sensore
      Serial.println("Errore Modulo SHT");
      return;
    }
  */

  pinMode(A4, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  matrix.begin();

  // Inizializza il modulo RTC
  Serial.println("Inizializzo modulo RTC");
  if (!rtc.begin())
  {
    Serial.println("Errore Modulo rtc");
    return;
  }
  delay(50);

  Serial.println("Modulo RTC Avviato");

  /*
    if (rtc.lostPower())
    {
      Serial.println("Rilevata perdita di orario del modulo RTC");
      // uso la data e l'ora di compilazione dello sketch
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
      // Puoi usare la rtc.adjust anche per settare manualmente data e ora. I parametri sono AA,MM,GG,HH,MM,SS
      // per il 21 gennaio 2014 alle 3 del mattino puoi usare:
      // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
    }
    */

  // commentare la riga seguente. va eseguita solo una volta
  // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  printDateTime(rtc.now());

  // initialize ESP module
  WiFi.init(&Serial1);

  // check for the presence of the shield
  if (WiFi.status() == WL_NO_SHIELD)
  {
    Serial.println("WiFi shield non presente!");
    // don't continue
    while (true)
      ;
  }

  // attempt to connect to WiFi network
  while (status != WL_CONNECTED)
  {
    Serial.print("In attesa di connettersi a WPA SSID: ");
    Serial.println(WIFI_SSID);
    // Connect to WPA/WPA2 network
    status = WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }

  // you're connected now, so print out the data
  printWifiStatus();

  fetchRSSFeed();
}

void updateRTCmodule(int ora, int minuti, int secondi, int giorno, int mese, int anno)
{
  int rtcYear = rtc.now().year();
  int rtcMonth = rtc.now().month();
  int rtcDay = rtc.now().day();
  if ((rtcYear == anno) and (rtcMonth == mese) and (rtcDay == giorno))
  {
    Serial.println("==> L'ora recuperata dal server NTP è corretta");
    rtc.adjust(DateTime(anno, mese, giorno, ora, minuti, secondi));
    Serial.println("Aggiornamento RTC eseguito con successo !");
  }
  else
  {
    Serial.println("*** L'ora recuperata dal server NTP non è corretta!!");
  }
}

void GetDateTimeFromNTPServer() // Ottiene la data dal server NTP ed aggiorna l'RTC
{
  Serial.println("=> in ottieni data web");
  timeClient.begin();
  // delay(500);

  // while (!timeClient.update()) { timeClient.forceUpdate(); }
  timeClient.update();
  epochTime = timeClient.getEpochTime();
  timeClient.end();

  Serial.println("EpochTime: " + String(epochTime));

  // Imposta il tempo
  setTime(epochTime);

  // Stampa la data e l'ora
  Serial.print("Data e ora da server NTP: ");
  Serial.print(day());
  Serial.print("/");
  Serial.print(month());
  Serial.print("/");
  Serial.print(year());
  Serial.print(" ");
  Serial.print(hour());
  Serial.print(":");
  Serial.print(minute());
  Serial.print(":");
  Serial.println(second());

  updateRTCmodule(hour(), minute(), second(), day(), month(), year());
}

bool isNumber(String str)
{
  // Check if the string is a number
  for (uint16_t i = 0; i < str.length(); i++)
  {
    // Check for digits, allow for a single decimal point and a negative sign
    if (!isdigit(str.charAt(i)) && str.charAt(i) != '.' && str.charAt(i) != '-' && i != 0)
    {
      return false;
    }
  }
  return true;
}

String splitStringByDelimiter(String str, int indice)
{
  String strs[2];
  int StringCount = 0;
  while (str.length() > 0)
  {
    int index = str.indexOf(',');
    if (index == -1) // No space found
    {
      strs[StringCount++] = str;
      break;
    }
    else
    {
      strs[StringCount++] = str.substring(0, index);
      str = str.substring(index + 1);
    }
  }
  return strs[indice];
}

String printTemperatureAndHumidity()
{
  float temperature = sht.getTemperature(); // Legge la temperatura
  float humidity = sht.getHumidity();       // Legge l'umidità

  // Controlla se ci sono errori nella lettura
  if (isnan(temperature) || isnan(humidity))
  {
    Serial.println("Errore nella lettura del sensore!");
    return "";
  }

  // Stampa i valori sul monitor seriale
  /*
  Serial.print("Temperatura: ");
  Serial.print(temperature);
  Serial.println(" °C");

  Serial.print("Umidità: ");
  Serial.print(humidity);
  Serial.println(" %");
  */

  String bufferTemp;

  // Formatta i valori float in una stringa
  // sprintf(bufferTemp, "Temp.: %.2f C, Umid.: %.2f %%", temperature, humidity);
  bufferTemp = "Tmp:" + (String)temperature + "C,Umd:" + (String)humidity + "%";
  // Serial.println(bufferTemp);
  return bufferTemp;
}

int changeBrightness(int value)
{
  float y = 1 - ((0.9 / 1003) * (ldrValue - 20));
  // Serial.print("brightness: ");
  // Serial.println(value * y);
  return int(value * y);
}

String leftPad(int number, int totalLength)
{
  // Convertiamo il numero in stringa
  String str = String(number);

  // Calcoliamo quanto padding è necessario
  int paddingNeeded = totalLength - str.length();

  // Aggiungiamo zeri a sinistra
  for (int i = 0; i < paddingNeeded; i++)
  {
    str = "0" + str; // Aggiungiamo uno zero a sinistra
  }

  return str;
}

// Calcola i pixel a sinistra per centrare una stringa
int centraStringa(String str)
{
  unsigned int stringLength = str.length();
  int leftSpace = (int)((64 - (6 * stringLength)) / 2);
  return leftSpace;
}

void refreshDisplay()
{
  t = rtc.now();
  matrix.setTextSize(1);     // size 1 == 8 pixels high
  matrix.setTextWrap(false); // Don't wrap at end of line - will do ourselves

  // Serial.println("******");
  // Serial.println(splitStringByDelimiter(tempHum,1));
  if (tempHumidState == TEMPERATURA)
  {
    if (millis() > Last_UPDATE_TempHumid + 10000)
    {
      String valore = splitStringByDelimiter(tempHum, 0);
      matrix.fillRect(0, 0, 64, 8, myBLACK);
      int leftSpace = 0; // centraStringa(valore);
      matrix.setCursor(leftSpace, 0);
      matrix.setTextColor(matrix.Color888(changeBrightness(255), changeBrightness(0), changeBrightness(0)));
      matrix.print(valore);
      Last_UPDATE_TempHumid = millis();
      tempHumidState = UMIDITA;
    }
  }
  else
  {
    if (millis() > Last_UPDATE_TempHumid + 10000)
    {
      String valore = splitStringByDelimiter(tempHum, 1);
      matrix.fillRect(0, 0, 64, 8, myBLACK);
      int leftSpace = 0; // centraStringa(valore);
      matrix.setCursor(leftSpace, 0);
      matrix.setTextColor(matrix.Color888(0, changeBrightness(128), changeBrightness(255)));
      matrix.print(valore);
      Last_UPDATE_TempHumid = millis();
      tempHumidState = TEMPERATURA;
    }
  }

  // Alterno la visualizzazione del giorno della settimana e della data odierna
  if (dataGiornoState == DATA)
  {
    if (millis() > Last_UPDATE_DataGiorno + 10000)
    {
      int leftSpace = centraStringa((String)wd[t.dayOfTheWeek()]);
      matrix.fillRect(0, 16, 64, 7, myBLACK);
      matrix.setCursor(leftSpace, 16);
      matrix.setTextColor(matrix.Color333(changeBrightness(7), changeBrightness(7), 0));
      matrix.print(wd[t.dayOfTheWeek()]);
      Last_UPDATE_DataGiorno = millis();
      dataGiornoState = GIORNO;
    }
  }
  else
  {
    if (millis() > Last_UPDATE_DataGiorno + 10000)
    {
      int leftSpace = centraStringa("XXXXXXXXX");
      matrix.fillRect(0, 16, 64, 7, myBLACK);
      matrix.setCursor(leftSpace, 16);
      matrix.setTextColor(matrix.Color333(changeBrightness(7), changeBrightness(7), 0));
      matrix.print(leftPad(t.day(), 2));
      matrix.setCursor(leftSpace + 12, 16);
      matrix.setTextColor(matrix.Color333(changeBrightness(7), 0, 0));
      matrix.print(months[t.month() - 1]);
      matrix.setCursor(leftSpace + 30, 16);
      matrix.setTextColor(matrix.Color333(0, 0, changeBrightness(7)));
      matrix.print(t.year());
      Last_UPDATE_DataGiorno = millis();
      dataGiornoState = DATA;
    }
  }

  // if (t.hour() != h)
  //{
  if (t.second() != s)
  {
    // Avviso acustico ogni ora
    if (t.second() == 0 and t.minute() == 0)
    {
      playSound();
    }
    matrix.fillRect(12, 24, 12, 7, myBLACK);
    matrix.setCursor(12, 24);
    matrix.setTextColor(matrix.Color888(changeBrightness(255), changeBrightness(0), changeBrightness(255)));
    matrix.print(leftPad(t.hour(), 2));
    matrix.setCursor(22, 24);
    matrix.print(":");
    // Serial.println("aggiornato campo ore");
    // h = t.hour();
    //}

    // if (t.minute() != m)
    //{
    matrix.fillRect(26, 24, 12, 7, myBLACK);
    matrix.setCursor(26, 24);
    matrix.setTextColor(matrix.Color333(0, changeBrightness(7), 0));
    matrix.print(leftPad(t.minute(), 2));
    matrix.setCursor(36, 24);
    matrix.print(":");
    // Serial.println("aggiornato campo minuti");
    // m = t.minute();
    //}

    // if (t.second() != s)
    //{
    matrix.fillRect(40, 24, 12, 7, myBLACK);
    matrix.setCursor(40, 24);
    matrix.setTextColor(matrix.Color333(0, changeBrightness(7), changeBrightness(7)));
    matrix.print(leftPad(t.second(), 2));
    s = t.second();
    // Serial.println("aggiornato campo secondi");
  }
}

void scrollingText(String testo)
{
  int limite_inferiore;
  // matrix.begin();
  uint16_t text_length = testo.length();
  matrix.fillRect(0, 8, 64, 8, myBLACK);
  matrix.setTextWrap(false);
  matrix.setCursor(xFirstRow, 8);
  matrix.setTextColor(matrix.Color888(changeBrightness(160), changeBrightness(160), changeBrightness(160)));
  matrix.setTextSize(1);
  matrix.println(testo);
  xFirstRow = xFirstRow - 3;
  limite_inferiore = -(6 * text_length);
  // for (int xpos=64; xpos > limite_inferiore; xpos--) {
  //   matrix.setCursor(xpos, 0);
  //   matrix.fillRect(0, 0, 64, 8, myBLACK);
  //   matrix.println(testo);
  // }
  if (xFirstRow < limite_inferiore)
  {
    xFirstRow = 64;
    newsDisplayed = true;
  }
  // delay(25);
  //  Serial.println(xFirstRow);
  //
  // yield();
}

void loop()
{

  if ((millis() > lastUpdateDateTime + dateTimeUpdateInterval) && (WiFi.status() == WL_CONNECTED))
  {
    GetDateTimeFromNTPServer();
    lastUpdateDateTime = millis();

    fetchRSSFeed();
  }

  if (millis() > lastReadingLDR + ldrUpdateInterval)
  {
    // Leggi il valore dell'LDR
    ldrValue = analogRead(A4);

    // Stampa il valore del LDR sulla Serial Monitor
    Serial.print("LDR value: ");
    Serial.println(ldrValue);
    lastReadingLDR = millis();
  }

  refreshDisplay();

  if (millis() > lastUpdateTempHum + tempHumUpdateInterval)
  {
    tempHum = printTemperatureAndHumidity();
    lastUpdateTempHum = millis();
  }

  if (newsDisplayed == true)
  {
    indiceArrayNotizie = indiceArrayNotizie + 1;
    newsDisplayed = false;
    if (indiceArrayNotizie >= NEWS_ITEMS_VIEWED)
    {
      indiceArrayNotizie = 0;
    }
  }
  scrollingText(arrayNotizie[indiceArrayNotizie]);
}
