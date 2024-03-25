/*
 WizFi360 example: Arduino_Lottery
*/
#include "WizFi360.h"
#include "icon.h"
#include <EEPROM.h>
#include <LittleFS.h>
#include <SD.h>

File myFile;
char file_name[16] = "MarkSix.txt";

#include <Arduino_GFX_Library.h>
Arduino_GFX *tft = create_default_Arduino_GFX();
#include <PNGdec.h>
PNG png;
uint8_t image_x;
uint8_t image_y;

#define debug_msg

#define buttonPin 14
#define ledPin    12
#define backLightPin 22

int frame = 0;
int frame_cnt = 0;
uint16_t x;
uint16_t y;
          
typedef enum 
{
  send_marksix_request = 0,
  get_marksix_content,
  analyze_marksix_content,
  display_marksix_content,
  display_marksix_result,
  check_display_status,
}STATE_;
STATE_ currentState;

/* Wi-Fi info */
char ssid[] = "wiznet";       // your network SSID (name)
char pass[] = "KUvT5sT1Ph";   // your network password
IPAddress ip;
char Lottery_server[] = "kclm.site"; 

struct _marksix_project{
  boolean Exist = false;
  String Issue;
  String DrawResult;
  String DrawTime;
} ;
_marksix_project marksix_project[200];

uint8_t marksix_project_num = 0;
uint8_t marksix_project_cnt = 0;

uint8_t data_now = 0; 
String json_String; 
uint16_t dataStart = 0;
uint16_t dataEnd = 0;
String tf_String; 
uint64_t html_len = 0;

uint16_t Marksix_Round;
uint16_t Marksix_num = 0;

uint8_t ball_num;
uint32_t ball_color;

struct _ball_total {
  uint8_t ball_No;
  uint8_t ball_cnt;
};
_ball_total ball_total[49];

bool buttonState = false;     // variable for reading the pushbutton status
int status = WL_IDLE_STATUS;  // the Wifi radio's status
// Initialize the Ethernet client object
WiFiClient client;
    
void setup() {
  // initialize serial for debugging  
  Serial.begin(115200);
  // initialize the pushbutton pin as an input:
  pinMode(buttonPin, INPUT);
  pinMode(ledPin, OUTPUT); 
  digitalWrite(ledPin, LOW); 

  EEPROM.begin(64);
  sync_with_eeprom(0);

  SD.begin(17);
#ifdef debug_msg
  myFile = SD.open("/"); 
  printDirectory(myFile, 0);
#endif

  tft->begin();
  tft->fillScreen(WHITE);
  pinMode(backLightPin, OUTPUT); 
  digitalWrite(backLightPin, HIGH); 
  
  display_logo(240,80,BLACK);
  image_x = 129;
  image_y = 125;
  display_MarkSix_icon(1);
  display_wifi_status(240,250);

  // initialize serial for WizFi360 module
  Serial2.setFIFOSize(4096);
  Serial2.begin(2000000);
  WiFi.init(&Serial2);
  // attempt to connect to WiFi network
  while ( status != WL_CONNECTED) {
    // Connect to WPA/WPA2 network
    status = WiFi.begin(ssid, pass);
  }
  display_wifi_status(240,250);
  // print your WiFi shield's IP address
  ip = WiFi.localIP();
  currentState = send_marksix_request;
}

void loop(){
  dataStart = 0 ;
  dataEnd = 0 ;
switch(currentState){
    case send_marksix_request:
       {
        // if you get a connection, report back via serial
        if (client.connectSSL(Lottery_server,443)) {
          delay(3000);
          // Make a HTTP request          
          client.println(String("GET /api/trial/drawResult?code=hk6&format=json&rows=300 HTTP/1.1"));          
          client.println(String("Host: ") + String(Lottery_server));
          client.println(String("Content-Type: application/json"));
          client.println("Connection: close");
          client.println();
          json_String= "";
          currentState = get_marksix_content;
        }
        else
        {
          client.stop();
          delay(1000);
        }
       }
      break;
      
    case get_marksix_content:
       {        
          while (client.available()) {
            json_String += (char)client.read();
            data_now =1; 
            frame_cnt ++;
            if(frame_cnt > 256)
            {                
              tft->fillRect(225,230,32,32,WHITE);
              tft->drawBitmap(225, 230, frames[frame], FRAME_WIDTH, FRAME_HEIGHT,BLACK);
              frame = (frame + 1) % FRAME_COUNT;              
              if(frame_cnt > 256)
              {
                frame_cnt = 0;
              }
            }
          }
          if(data_now)
          {
            dataStart = json_String.indexOf("\r\n\r\n") + strlen("\r\n\r\n");
            dataEnd = json_String.indexOf("\r\n{", dataStart); 
            html_len = strtol(json_String.substring(dataStart, dataEnd).c_str(), NULL, 16);
            Serial.println(html_len);
            html_len -= (json_String.length()- dataEnd -2);
            html_len = html_len + 2 + 5;
#ifdef debug_msg
            Serial.println(html_len);
#endif            
            dataStart = json_String.indexOf("\"data\":[") + strlen("\"data\":[");            
            Serial.println(json_String.length() - dataStart);
            json_String = json_String.substring(dataStart, json_String.length());
            while(html_len)
            {
              while (client.available()) {
                json_String += (char)client.read();
                html_len -- ;
                frame_cnt ++;
                if(frame_cnt > 256)
                {                
                  tft->fillRect(225,230,32,32,WHITE);
                  tft->drawBitmap(225, 230, frames[frame], FRAME_WIDTH, FRAME_HEIGHT,BLACK);
                  frame = (frame + 1) % FRAME_COUNT;                    
                  if(frame_cnt > 256)
                  {
                    frame_cnt = 0;
                  }
                }
              }
            }
            currentState = analyze_marksix_content;
          }
       }
      break;
      
    case analyze_marksix_content:
       {
          html_len = json_String.length();
          dataStart = 0 ; 
          dataEnd = 0 ;
          while(marksix_project_num < 200)
          {
            dataStart = json_String.indexOf("issue\":\"",dataEnd) + strlen("issue\":\"");  
            dataEnd = json_String.indexOf("\",\"", dataStart); 
            marksix_project[marksix_project_num].Issue = json_String.substring(dataStart, dataEnd);
#ifdef debug_msg
            Serial.println(marksix_project[marksix_project_num].Issue);
#endif 
            dataStart = json_String.indexOf("drawResult\":\"", dataEnd)+ strlen("drawResult\":\""); 
            dataEnd = json_String.indexOf("\",\"", dataStart); 
            marksix_project[marksix_project_num].DrawResult = json_String.substring(dataStart, dataEnd);
#ifdef debug_msg
            Serial.println(marksix_project[marksix_project_num].DrawResult);
#endif 
            dataStart = json_String.indexOf("drawTime\":\"", dataEnd)+ strlen("drawTime\":\""); 
            dataEnd = json_String.indexOf("\",\"", dataStart); 
            marksix_project[marksix_project_num].DrawTime = json_String.substring(dataStart, dataEnd);
#ifdef debug_msg
            Serial.println(marksix_project[marksix_project_num].DrawTime);
#endif 
            marksix_project_num ++;    
          }
          marksix_project_cnt = marksix_project_num;
#ifdef debug_msg
          for(int i=0;i<marksix_project_cnt;i++)
          {
            Serial.println(i);
            Serial.println(marksix_project[i].Issue);
            Serial.println(marksix_project[i].DrawResult);
            Serial.println(marksix_project[i].DrawTime);            
          }
#endif
          myFile = SD.open(file_name);
          if (myFile) {
//            Serial.println("myFile:");
//            while (myFile.available()) {
//              //Serial.write(myFile.read());
//              tf_String += (char)myFile.read();              
//            }
//            Serial.println(tf_String);
            myFile = SD.open(file_name, FILE_WRITE);
            if (myFile) 
            {
              for(int i=199;i>=0;i--)
              {
                if((marksix_project[i].Issue).toInt() > Marksix_Round)
                {
                  myFile.print("Num:");
                  myFile.print(Marksix_num);
                  myFile.print(" Issue:");
                  myFile.print(marksix_project[i].Issue);
                  myFile.print(" DrawResult:");
                  myFile.print(marksix_project[i].DrawResult);
                  myFile.print(" DrawTime:");
                  myFile.println(marksix_project[i].DrawTime);
                  for(uint8_t m=0; m<7; m++)
                  {
                    dataEnd = marksix_project[i].DrawResult.indexOf(",",dataStart);
                    ball_num = (marksix_project[i].DrawResult.substring(dataStart, dataEnd)).toInt();
                    dataStart = dataEnd+1;
                    ball_total[ball_num-1].ball_cnt++;
                  }
                  Marksix_num ++;
                }
              }              
            }
            myFile.close();        
          }
          else
          {
            Serial.println("Creat new file");
            myFile = SD.open(file_name, FILE_WRITE);
            if (myFile) {
              for(int i=0;i<marksix_project_cnt;i++)
              {
                myFile.print("Num:");
                myFile.print(i);
                myFile.print(" Issue:");
                myFile.print(marksix_project[199-i].Issue);
                myFile.print(" DrawResult:");
                myFile.print(marksix_project[199-i].DrawResult);
                myFile.print(" DrawTime:");
                myFile.println(marksix_project[199-i].DrawTime);
                for(uint8_t m=0; m<7; m++)
                {
                  dataEnd = marksix_project[199-i].DrawResult.indexOf(",",dataStart);
                  ball_num = (marksix_project[199-i].DrawResult.substring(dataStart, dataEnd)).toInt();
                  dataStart = dataEnd+1;
                  ball_total[ball_num-1].ball_cnt++;
                }
              }              
            }
            myFile.close();
            Marksix_num = marksix_project_cnt;
          }
          sync_with_eeprom(1);
          currentState = display_marksix_content;
       }
      break;

    case display_marksix_content:
       {
          x = 241;
          y = 303;
          dataStart = 0;  
          dataEnd = 0; 
          display_dashboard();
          
          tft->setTextColor(WHITE);
          tft->setTextSize(1);
          tft->setCursor(3, y-15);
          tft->print("Round");
          tft->setCursor(72, y-15);
          tft->print("Time");
          tft->setCursor(202, y-15);
          tft->print("Draw");
          tft->setTextSize(2);
          tft->setCursor(3, y-2);
          tft->print(marksix_project[0].Issue);
          tft->setTextSize(2);
          tft->setCursor(72, y-2);
          tft->print(marksix_project[0].DrawTime.substring(0,10));          
          for(uint8_t i; i<7; i++)
          {
            dataEnd = marksix_project[0].DrawResult.indexOf(",",dataStart);
            ball_num = (marksix_project[0].DrawResult.substring(dataStart, dataEnd)).toInt();
            dataStart = dataEnd+1;            
            ball_color = get_ball_color(ball_num);            
               
            tft->fillCircle(x,y,16,ball_color);
            tft->drawCircle(x,y,15,WHITE);
            //tft->fillArc(x,y,15,15,0,360,WHITE); 
            tft->setTextColor(WHITE);
            tft->setTextSize(2);
            if(ball_num < 10)
            {
              tft->setCursor(x-4, y-7);
              tft->print(ball_num);
              Serial.println(ball_num);
            }
            else
            {
              tft->setCursor(x-11, y-7);
              tft->print(ball_num);
              Serial.println(ball_num);
            }
            x += (32+4);
            if(i == 5)
            {
              x += 3;
            }
          }
          for(uint8_t m = 1; m<=49; m++)
          {
            x = (m%10)*48 + 24;
            y = (m/10)*48 + 57;
            ball_color = get_ball_color(m);
            tft->fillCircle(x,y,17,ball_color);
            tft->fillArc(x,y,15,16,0,360,WHITE); 
            tft->setTextColor(WHITE);
            tft->setTextSize(2);
            if(m < 10)
            {
              tft->setCursor(x-4, y-7);
              tft->print(m);
            }
            else
            {
              tft->setCursor(x-11, y-7);
              tft->print(m);
            }
            tft->setTextColor(ball_color);
            tft->setTextSize(1);
            tft->setCursor(x-5, y+21);
            tft->print(ball_total[m-1].ball_cnt);
          }
          bubbleSort(ball_total,49);
        currentState = display_marksix_result;
      }
    break;
    
    case display_marksix_result:
      {
        if(buttonState)
        {
          for (int i = 0; i < 7; i++) {
            x = (ball_total[48-i].ball_No%10)*48 + 24;
            y = (ball_total[48-i].ball_No/10)*48 + 57;
            tft->fillRoundRect(x-13,y+19,28,11,5, WHITE);
            ball_color = get_ball_color(ball_total[48-i].ball_No);
            tft->setTextColor(ball_color);
            tft->setTextSize(1);
            tft->setCursor(x-5, y+21);
            tft->print(ball_total[48-i].ball_cnt);
            
            x = (ball_total[i].ball_No%10)*48 + 24;
            y = (ball_total[i].ball_No/10)*48 + 57;
            tft->fillRoundRect(x-13,y+19,28,11,5, WHITE);
            ball_color = get_ball_color(ball_total[i].ball_No);
                    
            tft->drawRoundRect(x-12,y+19,27,11,5, ball_color);
            tft->fillCircle(x-8,y+24,5,ball_color);
            tft->setTextColor(WHITE);
            tft->setTextSize(1);
            tft->setCursor(x-10, y+21);
            tft->print(i+1);
            
            tft->setTextColor(ball_color);
            tft->setTextSize(1);
            tft->setCursor(x, y+21);
            tft->print(ball_total[i].ball_cnt);
          }
        }
        else
        {
          for (int i = 0; i <7; i++) {            
            x = (ball_total[i].ball_No%10)*48 + 24;
            y = (ball_total[i].ball_No/10)*48 + 57;
            tft->fillRoundRect(x-13,y+19,28,11,5, WHITE);
            ball_color = get_ball_color(ball_total[i].ball_No);  
            tft->setTextColor(ball_color);
            tft->setTextSize(1);
            tft->setCursor(x-5, y+21);
            tft->print(ball_total[i].ball_cnt);
            
            x = (ball_total[48-i].ball_No%10)*48 + 24;
            y = (ball_total[48-i].ball_No/10)*48 + 57;
            tft->fillRoundRect(x-13,y+19,28,11,5, WHITE);
            ball_color = get_ball_color(ball_total[48-i].ball_No);

            tft->drawRoundRect(x-12,y+19,27,11,5, ball_color);
            tft->fillCircle(x-8,y+24,5,ball_color);
            tft->setTextColor(WHITE);
            tft->setCursor(x-10, y+21);
            tft->print(i+1);

            tft->setTextColor(ball_color);
            tft->setCursor(x, y+21);
            tft->print(ball_total[48-i].ball_cnt);
          }
        }
        digitalWrite(ledPin, LOW); 
        delay(500);
        currentState = check_display_status;
      }
      break;
      
    case check_display_status:
      {
        if(digitalRead(buttonPin)== LOW)
        {
          delay(100);
          if(digitalRead(buttonPin)== LOW)
          {
            buttonState = !buttonState;
            digitalWrite(ledPin, HIGH); 
            currentState = display_marksix_result;
          }
        }        
      }
      break;
  }
}

void display_logo(uint16_t x,uint16_t y,uint16_t color)
{
  uint8_t cloud_pixel[5*11]=
  {
    0b00111110,0b01000001,0b01000001,0b01000001,0b00100010, // C
    0b00000000,0b00000000,0b01111111,0b00000000,0b00000000, // l
    0b00001110,0b00010001,0b00010001,0b00010001,0b00001110, // o
    0b00011110,0b00000001,0b00000001,0b00000001,0b00011111, // u
    0b00001110,0b00010001,0b00010001,0b00010001,0b01111111, // d
    0b00000000,0b00000000,0b00000000,0b00000000,0b00000000, // space
    0b01111111,0b01001000,0b01001000,0b01001000,0b00110000, // P
    0b00000000,0b00000000,0b01011111,0b00000000,0b00000000, // i
    0b00010001,0b00001010,0b00000100,0b00001010,0b00010001, // x
    0b00001110,0b00010101,0b00010101,0b00010101,0b00001100, // e
    0b00000000,0b00000000,0b01111111,0b00000000,0b00000000  // l
  };
  uint16_t _x = x - (5*5*5) - 6;
  uint16_t _y = y - 20;
  for(uint8_t i=0;i<11;i++)
  {    
    if(i == 1 || i == 2 || i ==5 || i==6 ||i==7 ||i==8 || i == 10)
    {
       _x = _x -6;
    }
    else
    {
       _x = _x+4;
    }   
    for(uint8_t m=0;m<5;m++)
    {
      _x = _x +5;
      _y = y - 20;
      for(uint8_t n=0;n<8;n++)
      {
        if((cloud_pixel[i*5+m]>>(7-n))&0x01)
        {
          tft->fillRect(_x+1,_y+1,4,4,color);
        }
        _y += 5;
      }
    }
  }
}

// Function to draw pixels to the display
void PNGDraw(PNGDRAW *pDraw)
{
  uint16_t usPixels[240];
  uint8_t usMask[240];
  png.getLineAsRGB565(pDraw, usPixels, PNG_RGB565_LITTLE_ENDIAN, 0x00000000);
  png.getAlphaMask(pDraw, usMask, 1);
  tft->draw16bitRGBBitmap(image_x, image_y + pDraw->y, usPixels, usMask, pDraw->iWidth,1);
}

void display_MarkSix_icon(uint8_t PNGnum)
{
  //char szTemp[256];
  int rc;
  if(PNGnum == 1 )
  {
    rc = png.openFLASH((uint8_t *)MarkSixIcon, sizeof(MarkSixIcon), PNGDraw);
  }
  else if(PNGnum == 2)
  {
    rc = png.openFLASH((uint8_t *)MarkSixIconMini, sizeof(MarkSixIconMini), PNGDraw);
  }
  if (rc == PNG_SUCCESS) {
    rc = png.decode(NULL, 0); // no private structure and skip CRC checking
    png.close();
  }
}

void display_wifi_status(uint8_t x,uint8_t y)
{
  if( status != WL_CONNECTED)
  {
    tft->fillCircle(x,y,3,DARKGREY);
    tft->fillArc(x,y, 5, 7, 225, 315, DARKGREY); 
    tft->fillArc(x,y, 9, 11, 225, 315, DARKGREY); 
    tft->fillArc(x,y, 13, 15, 225, 315, DARKGREY); 
  }
  else
  {
    tft->fillCircle(x,y,3,GREEN);
    tft->fillArc(x,y, 5, 7, 225, 315, GREEN); 
    tft->fillArc(x,y, 9, 11, 225, 315, GREEN); 
    tft->fillArc(x,y, 13, 15, 225, 315, GREEN); 
  }
}

void display_dashboard()
{       
    tft->fillScreen(WHITE);
    tft->fillRect(0,0,480,32,0xFB03);
    image_x = 8;
    image_y = 0;
    display_MarkSix_icon(2);    
    tft->setTextColor(WHITE);//(0xFB03);
    tft->setTextSize(2);
    tft->setCursor(52, 4);
    tft->print(String("HONG KONG Mark Six Lottery Monitor"));
    tft->setTextSize(1);
    tft->setCursor(345, 22);
    tft->print(String("Record Round:")+ (marksix_project_cnt+1));
    
    tft->fillRect(0,286,480,34,0xFB03);
}

uint32_t get_ball_color(uint8_t ball_num)
{
  uint32_t color;
  if((ball_num == 1) || (ball_num == 2)|| (ball_num == 7)|| (ball_num == 8)|| (ball_num == 12)|| (ball_num == 13)|| (ball_num == 18)|| (ball_num == 19)|| (ball_num == 23)|| (ball_num == 24)|| (ball_num == 29)|| (ball_num == 30)|| (ball_num == 34)|| (ball_num == 35)|| (ball_num == 40)|| (ball_num == 45)|| (ball_num == 46))
  {
    color = 0xF909;
  }
  else if((ball_num == 3) || (ball_num == 4)|| (ball_num == 9)|| (ball_num == 10)|| (ball_num == 14)|| (ball_num == 15)|| (ball_num == 20)|| (ball_num == 25)|| (ball_num == 26)|| (ball_num == 31)|| (ball_num == 36)|| (ball_num == 37)|| (ball_num == 41)|| (ball_num == 42)|| (ball_num == 47)|| (ball_num == 48))
  {
    color = 0x47B;
  }
  else if((ball_num == 5) || (ball_num == 6)|| (ball_num == 11)|| (ball_num == 16)|| (ball_num == 17)|| (ball_num == 21)|| (ball_num == 22)|| (ball_num == 27)|| (ball_num == 28)|| (ball_num == 32)|| (ball_num == 33)|| (ball_num == 38)|| (ball_num == 39)|| (ball_num == 43)|| (ball_num == 44)|| (ball_num == 49))
  {
    color = 0x4E73;
  }
  return color;
}

void sync_with_eeprom(uint8_t i)
{ 
  if(i == 0)
  {
    Marksix_Round = (uint16_t)(EEPROM.read(0)<<8 | EEPROM.read(1));
    Marksix_num  = (uint16_t)(EEPROM.read(2)<<8 | EEPROM.read(3));
    for(int n=0;n<49;n++)
    {
      ball_total[n].ball_cnt = EEPROM.read(n+4);
#ifdef debug_msg
      Serial.print(n+1);
      Serial.print(":");
      Serial.println(ball_total[n].ball_cnt);
#endif 
    }
#ifdef debug_msg
    Serial.print("Marksix_Round:"); 
    Serial.println(Marksix_Round);
    Serial.print("Marksix_num:");
    Serial.println(Marksix_num);
#endif    
  }
  else if (i == 1)
  {
    //write the marksix date to the eeprom
    EEPROM.write(0, (marksix_project[0].Issue.toInt() >> 8) & 0xff);
    EEPROM.write(1, (marksix_project[0].Issue.toInt() & 0xff));
    EEPROM.write(2, Marksix_num >> 8 & 0xff);
    EEPROM.write(3, Marksix_num & 0xff);
    for(int n=0;n<49;n++)
    {
      EEPROM.write(n+4, ball_total[n].ball_cnt);
#ifdef debug_msg
      Serial.print(n+1);
      Serial.print(":");
      Serial.println(ball_total[n].ball_cnt);
#endif 
    }
    EEPROM.commit();
#ifdef debug_msg
    Serial.println("write the marksix date to the eeprom");
#endif            
  }
}

void printDirectory(File dir, int numTabs)
{
  while (true)
  { 
    File entry = dir.openNextFile();
    if (!entry)
    {
      // If there is no file, jump out of the loop
      break;
    }
    for (uint8_t i = 0; i < numTabs; i++)
    {
      Serial.print('\t');
    }
    Serial.print(entry.name());
    if (entry.isDirectory())
    {
      Serial.println("/");
      printDirectory(entry, numTabs + 1);
    }
    else
    {
      Serial.print("\t\t");
      Serial.println(entry.size(), DEC);
    }
    entry.close();
  }
}

void bubbleSort(_ball_total arr[],int n) {
  for (int i = 0; i < 49; i++) { 
    arr[i].ball_No = i+1;
  }        
  for (int i = 0; i < n-1; i++) {
    for (int j = 0; j < n-i-1; j++) {
      if (arr[j].ball_cnt < arr[j+1].ball_cnt) {
        _ball_total temp = arr[j];
        arr[j] = arr[j+1];
        arr[j+1] = temp;
      }
    }
  }
}
