#include <Wire.h> // FINAL 9/2016 - 30/4/2017  update rainsensor fix 31/10/17
#include <LiquidCrystal_I2C.h>
#include <LCDMenuLib.h>
#include <avr/pgmspace.h>
#include "Sodaq_DS3231.h"
#include <EEPROM.h>
char weekDay[][4] =
{
  " ", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"
}; // day from the clock

// 1 select/menu
// 2 back
// 3 down
// 4 up
// EEPROM 0-3 valves.flag
// EEPROM 0-4 valves.cycle
// EEPROM 9-40 days of the week programs
// EEPROM 41-45 duration of irrigation
// EEPROM 46-49 status of valve Enable/Disable
// EEPROM 50-61 Start minute
// EEPROM 62-73 Start Hour
// menuflag = 1 root
// menuflag = 2 program
// menuflag = 3 manuel control
// menuflag = 4 vavles programs
// menuflag = 5 program type
// menuflag = 6 starts times
// menuflag = 7 duration of irrigation
// menuflag = 8 choose time screen
// menuflag = 9 time and date
// menuflag = 10 change rct time
// menuflag = 11 change rct date
// menuflag = 12 vavles status
// menuflag = 13 Hard reset
// inmenu flag = 1 cycle
#define I2C_ADDR 0x3F // <<----- Add your address here.  Find it from I2C Scanner
#define BACKLIGHT_PIN 3
#define En_pin 2
#define Rw_pin 1
#define Rs_pin 0
#define D4_pin 4
#define D5_pin 5
#define D6_pin 6
#define D7_pin 7
typedef struct
{
  // stats time valve1: 1-4, valve2: 5-8, valve3: 9-12, valve4: 13-16
  int hour;
  int minute; // time to start operate
}

programstime;
typedef struct
{
  // valvas
  int duration; // how much time to work
  int cycle; // how many days in cycle mode
  int flag; // flag=0 is piriod, flag=1 is days of the week
  int status; // able=1 disable=0
  programstime programs[3];
}

valve;
// programstime programs[12]={0};  // stats time valve1: 0-2, valve2: 3-5, valve3: 6-8, valve4: 9-11
valve valves[3] =
{
  0
};

programstime finnishv1, finnishv2, finnishv3, finnishv4, last;
int valve1days[8], valve2days[8], valve3days[8], valve4days[8] =
{
  0, 0, 0, 0, 0, 0, 0, 0
};

char digiflag, h, m, mo, da, d, activef, dayf1, dayf2, dayf3, dayf4, cc1, cc2, cc3, cc4, last_day, day, second_counter, last_sec, backlight_counter, fblink, lmonth, ldate;
unsigned long pm = 0; // will store last time LED was updated
unsigned long cm = 0; // currentMillis
LiquidCrystal_I2C lcd(I2C_ADDR, En_pin, Rw_pin, Rs_pin, D4_pin, D5_pin, D6_pin, D7_pin);
void mainmenuprt(); // ROOT
void dispmain(); // main screen
void programprt(); // program
void manuelprt(); // manuel control
void disptemp(); // the temperature
void valvesproprt(); // valve programs
void programtype(); // program type
void start(); // starts times
void duration(); // duration of irrigation
void choosetime(); // choose the time screen
void choosedate(); // choose date screen
void timedate(); // time and date
void rcttime(); // change the rtc time
void rctdate(); // change the rtc date
void valvestatus(); // make status enable or disable
void hardreset(); // delete all system EEPROM
void timematch(); // this will be resposibe to activate the valves whenever there is a time match
void stoptimematch(); // this will stop the irrigation when time is over
void blinkon(); // show the valve that is open by blink
int v1flag, v2flag, v3flag, v4flag, tempdig, y, rain;
int menuflag, inmenuflag, pointer, pointer2, valve1, valve2, valve3, valve4, a, temp, temp_old, i, temporary, weeksign = 0;
DateTime now; // the full date type
// constants won't change :
const long interval = 250; // interval at which to blink (milliseconds)
void setup()
{
  digitalWrite(2, HIGH);
  digitalWrite(3, HIGH);
  digitalWrite(4, HIGH);
  digitalWrite(5, HIGH);
  Serial.begin(9600);
  // valve1days[4]=1;
  lcd.begin(20, 4); // initialize the lcd
  Wire.begin(); // initialize the clock
  rtc.begin();
  now = rtc.now(); // get the current date-time
  pinMode(2, OUTPUT); // valve 1
  pinMode(3, OUTPUT); // va;ve 2
  pinMode(4, OUTPUT); // valve 3
  pinMode(5, OUTPUT); // valve 4
  // Switch on the backlight
  lcd.setBacklightPin(BACKLIGHT_PIN, POSITIVE);
  lcd.setBacklight(HIGH); // this turn ON/OFF backlight
  lcd.home(); // go home
  pinMode(A0, INPUT_PULLUP); // sets analog pin for input for the bottuns
  dispmain(); // display main screen system
  for (int j = 0; j < 4; j++)
  {
    valves[j].duration = 0;
    for (int i = 0; i < 3; i++)
    {
      valves[j].programs[i].hour = 24;
      valves[j].programs[i].minute = 0;
    }
  }
  for (int i = 0; i < 4; i++)
    //read the kind of program cycle/weekdays 0-3
  valves[i].flag = EEPROM.read(i);
  for (int i = 0; i < 4; i++)
    //read the cycle programs 3-7
  valves[i].cycle = EEPROM.read(i + 4);
  for (int i = 1; i < 8; i++)
    // read the days of week 9-40
  valve1days[i] = EEPROM.read(i + 8);
  for (int i = 1; i < 8; i++)
    valve2days[i] = EEPROM.read(i + 16);
  for (int i = 1; i < 8; i++)
    valve3days[i] = EEPROM.read(i + 24);
  for (int i = 1; i < 8; i++)
    valve4days[i] = EEPROM.read(i + 32);
  for (int i = 0; i < 4; i++)
    //read the duration programs 41-45
  valves[i].duration = EEPROM.read(i + 41);
  for (int i = 0; i < 4; i++)
    //read the status Enable/Disable 46-49
  valves[i].status = EEPROM.read(i + 46);
  valves[0].programs[0].minute = EEPROM.read(50); // start time minute
  valves[0].programs[1].minute = EEPROM.read(51);
  valves[0].programs[2].minute = EEPROM.read(52);
  valves[1].programs[0].minute = EEPROM.read(53);
  valves[1].programs[1].minute = EEPROM.read(54);
  valves[1].programs[2].minute = EEPROM.read(55);
  valves[2].programs[0].minute = EEPROM.read(56);
  valves[2].programs[1].minute = EEPROM.read(57);
  valves[2].programs[2].minute = EEPROM.read(58);
  valves[3].programs[0].minute = EEPROM.read(59);
  valves[3].programs[1].minute = EEPROM.read(60);
  valves[3].programs[2].minute = EEPROM.read(61);
  valves[0].programs[0].hour = EEPROM.read(62); // start time hour
  valves[0].programs[1].hour = EEPROM.read(63);
  valves[0].programs[2].hour = EEPROM.read(64);
  valves[1].programs[0].hour = EEPROM.read(65);
  valves[1].programs[1].hour = EEPROM.read(66);
  valves[1].programs[2].hour = EEPROM.read(67);
  valves[2].programs[0].hour = EEPROM.read(68);
  valves[2].programs[1].hour = EEPROM.read(69);
  valves[2].programs[2].hour = EEPROM.read(70);
  valves[3].programs[0].hour = EEPROM.read(71);
  valves[3].programs[1].hour = EEPROM.read(72);
  valves[3].programs[2].hour = EEPROM.read(73);
  activef = 0;
  last_day = now.dayOfWeek();
  last_sec = now.second();
  backlight_counter = 0;
  last.hour = 0;
  last.minute = 0;
  lmonth = 0;
  ldate = 0;
  pinMode(10, INPUT); // rain
}

// end setup
int readButtons(int pin)
// returns the button number pressed, or zero for none pressed
// int pin is the analog pin number to read
{
  int b, c = 0;
  c = analogRead(pin); // get the analog value
  Serial.print(c);
  Serial.print('\n');
  // delay(50);
  if (c > 900)
  {
    b = 0; // buttons have not been pressed
  }
  else
    if (c > 10 && c < 35)
    {
      b = 1; // button 1 pressed
    }
  else
    if (c > 55 && c < 85)
    {
      b = 2; // button 2 pressed
    }
  else
    if (c > 105 && c < 135)
    {
      b = 3; // button 3 pressed
    }
  else
    if (c > 150 && c < 200)
    {
      b = 4; // button 4 pressed
    }
  return b;
}

// end read buttons
uint32_t old_ts;
void loop()
{
  a = readButtons(0);
  Serial.print(a);
  Serial.print('\n');
  if (a == 0)
    // no buttons pressed
  {
  }
  else
    if (a > 0)
      // someone pressed a button!
  {
    last_sec = now.second();
    backlight_counter = 0;
    lcd.setBacklight(HIGH);
  }
  if (a == 1)
  {
    lcd.clear();
    pointer = 1;
    mainmenuprt();
    menuflag = 1;
    a = 0;
    while (readButtons(0) > 0)
    {
      delay(100); // wait untill end of pressing
    }
  }
  // if a = 1
  if (a == 2)
  {
    if ((v1flag == 1) || (v2flag == 1) || (v3flag == 1) || (v4flag == 1))
    {
      digitalWrite(2, HIGH);
      digitalWrite(3, HIGH);
      digitalWrite(4, HIGH);
      digitalWrite(5, HIGH);
      v1flag = 0;
      v2flag = 0;
      v3flag = 0;
      v4flag = 0;
      lcd.setCursor(0, 1);
      lcd.write("System is OFF");
      lcd.setCursor(0, 2);
      lcd.write("                ");
    }
    a = 0;
    while (readButtons(0) > 0)
    {
      delay(100); // wait untill end of pressing
    }
  }
  // if a = 2
  while (menuflag != 0)
  {
    delay(100);
    lcd.setBacklight(HIGH);
    switch (menuflag)
    {
      case 1://menuflag = 1 ROOT
      a = readButtons(0);
      switch (a)
      {
        case 3:
        {
          // down
          if (pointer < 5)
          {
            pointer++;
            mainmenuprt();
          }
          while (readButtons(0) > 0)
          {
            delay(100); // wait untill end of pressing
          }
        }
        break;
        case 4:
        {
          // up
          if (pointer > 1)
          {
            pointer--;
            mainmenuprt();
          }
          while (readButtons(0) > 0)
          {
            delay(100); // wait untill end of pressing
          }
        }
        break;
        case 1:
        {
          // enter
          switch (pointer)
          {
            case 1:
            {
              // enter program
              lcd.clear();
              pointer = 1;
              programprt();
              menuflag = 2;
              while (readButtons(0) > 0)
              {
                delay(100); // wait untill end of pressing
              }
            }
            break;
            case 2:
            {
              // enter manuel control
              lcd.clear();
              pointer = 1;
              manuelprt();
              menuflag = 3;
              while (readButtons(0) > 0)
              {
                delay(100); // wait untill end of pressing
              }
              Serial.write("manuel control");
            }
            break;
            case 4:
            {
              // enter time and date
              lcd.clear();
              h = now.hour();
              m = now.minute();
              d = now.date();
              mo = now.month();
              da = now.dayOfWeek();
              y = now.year();
              pointer = 1;
              timedate();
              menuflag = 9;
              while (readButtons(0) > 0)
              {
                delay(100); // wait untill end of pressing
              }
              Serial.write("time and date");
            }
            break;
            case 3:
            {
              // enter valves status
              lcd.clear();
              pointer = 1;
              valvestatus();
              menuflag = 12;
              while (readButtons(0) > 0)
              {
                delay(100); // wait untill end of pressing
              }
              Serial.write("valves status");
            }
            break;
            case 5:
            {
              // enter hard reset
              lcd.clear();
              hardreset();
              menuflag = 13;
              while (readButtons(0) > 0)
              {
                delay(100); // wait untill end of pressing
              }
              Serial.write("hard reset");
            }
            break;
          }
          // switch '1' enter
        }
        break;
        case 2:
        {
          // quit to main screen system
          pointer = 1;
          dispmain();
          menuflag = 0;
          while (readButtons(0) > 0)
          {
            delay(100); // wait untill end of pressing
          }
          Serial.write("system");
        }
        break;
      }; // switch
      a = 0;
      break; // while menuflag = 1 root
      case 2://menuflag = 2 programs
      a = readButtons(0);
      switch (a)
      {
        case 3:
        {
          // down
          if (pointer < 4)
          {
            pointer++;
            programprt();
          }
          while (readButtons(0) > 0)
          {
            delay(100); // wait untill end of pressing
          }
        }
        break;
        case 4:
        {
          // up
          if (pointer > 1)
          {
            pointer--;
            programprt();
          }
          while (readButtons(0) > 0)
          {
            delay(100); // wait untill end of pressing
          }
        }
        break;
        case 1:
        {
          // enter
          lcd.clear();
          pointer2 = 1;
          valvesproprt();
          menuflag = 4;
          while (readButtons(0) > 0)
          {
            delay(100); // wait untill end of pressing
          }
        }
        break;
        case 2:
        {
          // quit to main menu
          lcd.clear();
          pointer = 1;
          mainmenuprt();
          menuflag = 1;
          Serial.write("main menu");
          while (readButtons(0) > 0)
          {
            delay(100); // wait untill end of pressing
          }
        }
        break;
      }; // switch
      a = 0;
      break; // while menuflag = 2 program
      case 3://menuflag = 3 manuel
      a = readButtons(0);
      switch (a)
      {
        case 3:
        {
          // down
          if (pointer < 4)
          {
            pointer++;
            manuelprt();
          }
          while (readButtons(0) > 0)
          {
            delay(100); // wait untill end of pressing
          }
        }
        break;
        case 4:
        {
          // up
          if (pointer > 1)
          {
            pointer--;
            manuelprt();
          }
          while (readButtons(0) > 0)
          {
            delay(100); // wait untill end of pressing
          }
        }
        break;
        case 2:
        {
          // quit to main menu
          lcd.clear();
          pointer = 1;
          mainmenuprt();
          menuflag = 1;
          valve1 = 0;
          valve2 = 0;
          valve3 = 0;
          valve4 = 0;
          digitalWrite(2, HIGH);
          digitalWrite(3, HIGH);
          digitalWrite(4, HIGH);
          digitalWrite(5, HIGH);
          Serial.write("main menu");
          while (readButtons(0) > 0)
          {
          }
          // wait untill end of pressing
        }
        break;
        case 1:
        {
          // enter
          switch (pointer)
          {
            case 1:
            {
              // turn on manuel valve 1
              if (valve1 == 0)
              {
                valve1 = 1;
                lcd.setCursor(10, 1);
                digitalWrite(2, LOW);
                lcd.write("Opening...");
                // delay(3000);
                lcd.setCursor(10, 1);
                lcd.write("ON        ");
                while (readButtons(0) > 0)
                {
                  delay(100); // wait untill end of pressing
                }
              }
              // if manuelvalve1=0
              else
              {
                valve1 = 0;
                lcd.setCursor(10, 1);
                digitalWrite(2, HIGH);
                lcd.write("Closing...");
                lcd.setCursor(10, 1);
                // delay(3000);
                lcd.write("OFF        ");
              }
              // else manuelvalve1=0
              while (readButtons(0) > 0)
              {
                delay(100); // wait untill end of pressing
              }
            }
            break;
            case 2:
            {
              // turn on manuel valve 2
              if (valve2 == 0)
              {
                valve2 = 1;
                lcd.setCursor(10, 2);
                digitalWrite(3, LOW);
                lcd.write("Opening...");
                // delay(3000);
                lcd.setCursor(10, 2);
                lcd.write("ON        ");
                while (readButtons(0) > 0)
                {
                  delay(100); // wait untill end of pressing
                }
              }
              // if manuelvalve2=0
              else
              {
                valve2 = 0;
                lcd.setCursor(10, 2);
                digitalWrite(3, HIGH);
                lcd.write("Closing...");
                lcd.setCursor(10, 2);
                // delay(3000);
                lcd.write("OFF        ");
              }
              // else manuelvalve2=0
              while (readButtons(0) > 0)
              {
                delay(100); // wait untill end of pressing
              }
            }
            break;
            case 3:
            {
              // turn on manuel valve 3
              if (valve3 == 0)
              {
                valve3 = 1;
                lcd.setCursor(10, 3);
                digitalWrite(4, LOW);
                lcd.write("Opening...");
                // delay(3000);
                lcd.setCursor(10, 3);
                lcd.write("ON        ");
              }
              // if manuelvalve2=0
              else
              {
                valve3 = 0;
                lcd.setCursor(10, 3);
                digitalWrite(4, HIGH);
                lcd.write("Closing...");
                lcd.setCursor(10, 3);
                // delay(3000);
                lcd.write("OFF        ");
              }
              // else manuelvalve3=0
              while (readButtons(0) > 0)
              {
                delay(100); // wait untill end of pressing
              }
            }
            break;
            case 4:
            {
              // turn on manuel valve 4
              if (valve4 == 0)
              {
                valve4 = 1;
                lcd.setCursor(10, 1);
                digitalWrite(5, LOW);
                lcd.write("Opening...");
                // delay(3000);
                lcd.setCursor(10, 1);
                lcd.write("ON        ");
              }
              // if manuelvalve4=0
              else
              {
                valve4 = 0;
                lcd.setCursor(10, 1);
                digitalWrite(5, HIGH);
                lcd.write("Closing...");
                lcd.setCursor(10, 1);
                // delay(3000);
                lcd.write("OFF        ");
              }
              // else manuelvalve4=0
              while (readButtons(0) > 0)
              {
              }
              // wait untill end of pressing
            }
            break;
            while (readButtons(0) > 0)
            {
              delay(100); // wait untill end of pressing
            }
          }
          // switch '1'
        }
        break;
      }; // switch
      a = 0;
      break; // menuflag = 3 manuel
      case 4://menuflag = 4 valves programs
      a = readButtons(0);
      switch (a)
      {
        case 3:
        {
          // down
          if (pointer2 < 4)
          {
            pointer2++;
            valvesproprt();
          }
          while (readButtons(0) > 0)
          {
            delay(100); // wait untill end of pressing
          }
        }
        break;
        case 4:
        {
          // up
          if (pointer2 > 1)
          {
            pointer2--;
            valvesproprt();
          }
          while (readButtons(0) > 0)
          {
            delay(100); // wait untill end of pressing
          }
        }
        break;
        case 1:
        {
          // enter
          switch (pointer2)
          {
            case 1:
            {
              // enter program type
              lcd.clear();
              pointer2 = 1;
              programtype();
              menuflag = 5;
              while (readButtons(0) > 0)
              {
                delay(100); // wait untill end of pressing
              }
            }
            break;
            case 2:
            {
              // start time
              lcd.clear();
              pointer2 = 1;
              start();
              menuflag = 6;
              while (readButtons(0) > 0)
              {
                delay(100); // wait untill end of pressing
              }
            }
            break;
            case 3:
            {
              // duration
              lcd.clear();
              pointer2 = 1;
              duration();
              menuflag = 7;
              while (readButtons(0) > 0)
              {
                delay(100); // wait untill end of pressing
              }
            }
            break;
          }
          while (readButtons(0) > 0)
          {
            delay(100); // wait untill end of pressing
          }
        }
        break;
        case 2:
        {
          // quit to main menu
          lcd.clear();
          pointer2 = 1;
          pointer = 1;
          programprt();
          menuflag = 2;
          Serial.write("programs");
          while (readButtons(0) > 0)
          {
            delay(100); // wait untill end of pressing
          }
        }
        break;
      }; // switch
      a = 0;
      break; // while menuflag = 4 vavles programs
      case 5://menuflag = 5 program type
      a = readButtons(0);
      switch (a)
      {
        case 3:
        {
          // down
          if (pointer2 < 4)
          {
            pointer2++;
            programtype();
          }
          while (readButtons(0) > 0)
          {
            delay(100); // wait untill end of pressing
          }
        }
        break;
        case 4:
        {
          // up
          if (pointer2 > 1)
          {
            pointer2--;
            programtype();
          }
          while (readButtons(0) > 0)
          {
            delay(100); // wait untill end of pressing
          }
        }
        break;
        case 2:
        {
          // quit to valve programs
          lcd.clear();
          EEPROM.write(pointer - 1, valves[pointer - 1].flag);
          EEPROM.write(pointer + 3, valves[pointer - 1].cycle);
          switch (pointer)
          {
            case 1:
            {
              for (int i = 1; i < 8; i++)
                EEPROM.write(i + 8, valve1days[i]);
            }
            // case 1
            break;
            case 2:
            {
              for (int i = 1; i < 8; i++)
                EEPROM.write(i + 16, valve2days[i]);
            }
            // case 2
            break;
            case 3:
            {
              for (int i = 1; i < 8; i++)
                EEPROM.write(i + 24, valve3days[i]);
            }
            // case 3
            break;
            case 4:
            {
              for (int i = 1; i < 8; i++)
                EEPROM.write(i + 32, valve4days[i]);
            }
            // case 4
            break;
          }
          // end switch pointer for saving to EEROM weekdays
          pointer2 = 1;
          valvesproprt();
          menuflag = 4;
          Serial.write("vlave programs");
          while (readButtons(0) > 0)
          {
            delay(100); // wait untill end of pressing
          }
        }
        break;
        case 1:
        {
          // enter
          switch (pointer2)
          {
            case 1:
            {
              if (valves[pointer - 1].flag == 0)
              {
                valves[pointer - 1].flag = 1;
                lcd.setCursor(7, 1);
                lcd.write("Week days    ");
                switch (pointer)
                {
                  case 1:
                  {
                    for (i = 1; i < 8; i++)
                    {
                      lcd.setCursor(6 + i, 2);
                      if (valve1days[i] == 1)
                      {
                        lcd.print(i);
                      }
                      // if
                      else
                      {
                        lcd.write("X");
                      }
                      // else
                    }
                    // for loop
                    break;
                  }
                  case 2:
                  {
                    for (i = 1; i < 8; i++)
                    {
                      lcd.setCursor(6 + i, 2);
                      if (valve2days[i] == 1)
                      {
                        lcd.print(i);
                      }
                      // if
                      else
                      {
                        lcd.write("X");
                      }
                      // else
                    }
                    // for loop
                    break;
                  }
                  // case 2
                  case 3:
                  {
                    for (i = 1; i < 8; i++)
                    {
                      lcd.setCursor(6 + i, 2);
                      if (valve3days[i] == 1)
                      {
                        lcd.print(i);
                      }
                      // if
                      else
                      {
                        lcd.write("X");
                      }
                      // else
                    }
                    // for loop
                    break;
                  }
                  // case 3
                  case 4:
                  {
                    for (i = 1; i < 8; i++)
                    {
                      lcd.setCursor(6 + i, 2);
                      if (valve4days[i] == 1)
                      {
                        lcd.print(i);
                      }
                      // if
                      else
                      {
                        lcd.write("X");
                      }
                      // else
                    }
                    // for loop
                    break;
                  }
                  // case 4
                }
                // switch
                while (readButtons(0) > 0)
                {
                  delay(100); // wait untill end of pressing
                }
              }
              // 
              else
              {
                valves[pointer - 1].flag = 0;
                lcd.setCursor(7, 1);
                lcd.write("Cycle       ");
                inmenuflag = 1;
                lcd.setCursor(7, 2);
                lcd.print(valves[pointer - 1].cycle);
                lcd.write("       ");
              }
              // else
              while (readButtons(0) > 0)
              {
                delay(100); // wait untill end of pressing
              }
            }
            break;
            case 2:
            {
              lcd.setCursor(1, 3);
              lcd.write(" ");
              lcd.setCursor(7, 4);
              lcd.write("^");
              inmenuflag = 1;
              temporary = valves[pointer - 1].cycle;
              weeksign = 7;
              while (readButtons(0) > 0)
              {
                delay(100); // wait untill end of pressing
              }
              while (inmenuflag == 1)
              {
                // inmenuflag = 1 cycle
                a = readButtons(0);
                if (valves[pointer - 1].flag == 0)
                {
                  // if its cycle
                  switch (a)
                  {
                    case 3:
                    {
                      // down
                      if (temporary > 0)
                      {
                        temporary--;
                        lcd.setCursor(7, 2);
                        lcd.print(temporary);
                      }
                      while (readButtons(0) > 0)
                      {
                        delay(100); // wait untill end of pressing
                      }
                    }
                    break;
                    case 4:
                    {
                      // up
                      if (temporary < 9)
                      {
                        temporary++;
                        lcd.setCursor(7, 2);
                        lcd.print(temporary);
                      }
                      while (readButtons(0) > 0)
                      {
                        delay(100); // wait untill end of pressing
                      }
                    }
                    break;
                    case 1:
                    {
                      // enter
                      valves[pointer - 1].cycle = temporary; // save data here
                      inmenuflag = 0;
                      lcd.setCursor(7, 4);
                      lcd.write(" ");
                      cc1 = 0;
                      cc2 = 0;
                      cc3 = 0;
                      cc4 = 0;
                      while (readButtons(0) > 0)
                      {
                        delay(100); // wait untill end of pressing
                      }
                      a = 0;
                    }
                    break;
                    case 2:
                    {
                      // quit
                      inmenuflag = 0;
                      temporary = 0;
                      weeksign = 7;
                      lcd.setCursor(7, 4);
                      lcd.write(" ");
                      lcd.setCursor(7, 2);
                      lcd.print(valves[pointer - 1].cycle);
                      while (readButtons(0) > 0)
                      {
                        delay(100); // wait untill end of pressing
                      }
                      a = 0;
                    }
                    break;
                  }; // switch
                  a = 0;
                }
                // if cycle
                else
                {
                  // weekdays
                  switch (a)
                  {
                    case 3:
                    {
                      // down
                      if (weeksign > 7)
                      {
                        lcd.setCursor(weeksign, 3);
                        lcd.write(" ");
                        weeksign--;
                        lcd.setCursor(weeksign, 3);
                        lcd.write("^");
                      }
                      while (readButtons(0) > 0)
                      {
                        delay(100); // wait untill end of pressing
                      }
                    }
                    break;
                    case 4:
                    {
                      // up
                      if (weeksign < 13)
                      {
                        lcd.setCursor(weeksign, 3);
                        lcd.write(" ");
                        weeksign++;
                        lcd.setCursor(weeksign, 3);
                        lcd.write("^");
                      }
                      while (readButtons(0) > 0)
                      {
                        delay(100); // wait untill end of pressing
                      }
                    }
                    break;
                    case 1:
                    {
                      // enter
                      switch (pointer)
                      {
                        case 1:
                        {
                          lcd.setCursor(weeksign, 2);
                          if (valve1days[weeksign - 6] == 1)
                          {
                            valve1days[weeksign - 6] = 0;
                            lcd.write("X");
                          }
                          // if
                          else
                          {
                            valve1days[weeksign - 6] = 1;
                            lcd.print(weeksign - 6);
                          }
                          // else
                          break;
                        }
                        case 2:
                        {
                          lcd.setCursor(weeksign, 2);
                          if (valve2days[weeksign - 6] == 1)
                          {
                            valve2days[weeksign - 6] = 0;
                            lcd.write("X");
                          }
                          // if
                          else
                          {
                            valve2days[weeksign - 6] = 1;
                            lcd.print(weeksign - 6);
                          }
                          // else
                          break;
                        }
                        case 3:
                        {
                          lcd.setCursor(weeksign, 2);
                          if (valve3days[weeksign - 6] == 1)
                          {
                            valve3days[weeksign - 6] = 0;
                            lcd.write("X");
                          }
                          // if
                          else
                          {
                            valve3days[weeksign - 6] = 1;
                            lcd.print(weeksign - 6);
                          }
                          // else
                          break;
                        }
                        case 4:
                        {
                          lcd.setCursor(weeksign, 2);
                          if (valve4days[weeksign - 6] == 1)
                          {
                            valve4days[weeksign - 6] = 0;
                            lcd.write("X");
                          }
                          // if
                          else
                          {
                            valve4days[weeksign - 6] = 1;
                            lcd.print(weeksign - 6);
                          }
                          // else
                          break;
                        }
                      }
                      // switch
                      while (readButtons(0) > 0)
                      {
                        delay(100); // wait untill end of pressing
                      }
                    }
                    break;
                    case 2:
                    {
                      // quit save data here
                      inmenuflag = 0;
                      temporary = 0;
                      lcd.setCursor(weeksign, 4);
                      lcd.write(" ");
                      weeksign = 7;
                      while (readButtons(0) > 0)
                      {
                        delay(100); // wait untill end of pressing
                      }
                      a = 0;
                    }
                    break;
                  }; // switch
                }
                // else weekdays
              }
              // while inmenuflag = 1 cycle weekdays */
            }
            break;
          }
          // switch
        }
        // end enter
      }
      a = 0;
      break; // menuflag5 program type
      case 7://menuflag = 7 duration
      {
        a = readButtons(0);
        switch (a)
        {
          case 3:
          {
            // down
            if (valves[pointer - 1].duration > 1)
            {
              valves[pointer - 1].duration--;
              lcd.setCursor(9, 2);
              lcd.write("  ");
              lcd.setCursor(9, 2);
              lcd.print(valves[pointer - 1].duration);
              delay(100);
            }
          }
          break;
          case 4:
          {
            // up
            if (valves[pointer - 1].duration < 60)
            {
              valves[pointer - 1].duration++;
              lcd.setCursor(9, 2);
              lcd.write("  ");
              lcd.setCursor(9, 2);
              lcd.print(valves[pointer - 1].duration);
              delay(100);
            }
          }
          break;
          case 1:
          {
            // enter
            lcd.clear();
            valvesproprt();
            menuflag = 4;
            Serial.write("vlave programs");
            while (readButtons(0) > 0)
            {
              delay(100); // wait untill end of pressing
            }
          }
          break;
          case 2:
          {
            // quit
            EEPROM.write(pointer + 40, valves[pointer - 1].duration);
            lcd.clear();
            valvesproprt();
            menuflag = 4;
            Serial.write("vlave programs");
            while (readButtons(0) > 0)
            {
              delay(100); // wait untill end of pressing
            }
          }
          break;
        }; // switch
        a = 0;
        break;
      }
      // menuflag 7 duration
      case 6://menuflag = 6 start
      {
        a = readButtons(0);
        switch (a)
        {
          case 3:
          {
            // down
            if (pointer2 < 4)
            {
              pointer2++;
              start();
            }
            while (readButtons(0) > 0)
            {
              delay(100); // wait untill end of pressing
            }
          }
          break;
          case 4:
          {
            // up
            if (pointer2 > 1)
            {
              pointer2--;
              start();
            }
            while (readButtons(0) > 0)
            {
              delay(100); // wait untill end of pressing
            }
          }
          break;
          case 1:
          {
            // enter
            lcd.clear();
            choosetime();
            menuflag = 8;
            digiflag = 1;
            Serial.write("choose time");
            while (readButtons(0) > 0)
            {
              delay(100); // wait untill end of pressing
            }
          }
          break; // case 1 enter
          case 2:
          {
            // quit
            lcd.clear();
            valvesproprt();
            menuflag = 4;
            Serial.write("vlave programs");
            while (readButtons(0) > 0)
            {
              delay(100); // wait untill end of pressing
            }
          }
          break;
        }; // switch
        a = 0;
        break;
      }
      // menuflag 6 start
      case 8://menuflag =8 choose time
      {
        a = readButtons(0);
        switch (a)
        {
          case 3:
          {
            // down
            if (digiflag == 1)
            {
              if (valves[pointer - 1].programs[pointer2 - 1].hour > 0)
                valves[pointer - 1].programs[pointer2 - 1].hour--;
            }
            else
            {
              if (valves[pointer - 1].programs[pointer2 - 1].minute > 0)
                valves[pointer - 1].programs[pointer2 - 1].minute--;
            }
            delay(100);
            // while (readButtons(0) >0){ delay(100);}  //wait untill end of pressing
            choosetime();
          }
          break;
          case 4:
          {
            // up
            if (digiflag == 1)
            {
              if (valves[pointer - 1].programs[pointer2 - 1].hour < 24)
                valves[pointer - 1].programs[pointer2 - 1].hour++;
            }
            else
            {
              if (valves[pointer - 1].programs[pointer2 - 1].minute < 60)
                valves[pointer - 1].programs[pointer2 - 1].minute++;
            }
            delay(100);
            // while (readButtons(0) >0){ delay(100);}  //wait untill end of pressing
            choosetime();
          }
          break;
          case 1:
          {
            // enter
            if (digiflag == 1)
            {
              digiflag = 2;
              lcd.setCursor(8, 1);
              lcd.write("^");
              lcd.setCursor(5, 1);
              lcd.write(" ");
            }
            else
            {
              digiflag = 1;
              lcd.setCursor(8, 1);
              lcd.write(" ");
              lcd.setCursor(5, 1);
              lcd.write("^");
            }
            while (readButtons(0) > 0)
            {
              delay(100); // wait untill end of pressing
            }
          }
          break;
          case 2:
          {
            // quit
            switch (pointer)
            {
              case 1:
              {
                EEPROM.write(49 + pointer2, valves[pointer - 1].programs[pointer2 - 1].minute);
                EEPROM.write(61 + pointer2, valves[pointer - 1].programs[pointer2 - 1].hour);
              }
              // case 1
              break;
              case 2:
              {
                EEPROM.write(52 + pointer2, valves[pointer - 1].programs[pointer2 - 1].minute);
                EEPROM.write(64 + pointer2, valves[pointer - 1].programs[pointer2 - 1].hour);
              }
              // case 2
              break;
              case 3:
              {
                EEPROM.write(55 + pointer2, valves[pointer - 1].programs[pointer2 - 1].minute);
                EEPROM.write(67 + pointer2, valves[pointer - 1].programs[pointer2 - 1].hour);
              }
              // case 3
              break;
              case 4:
              {
                EEPROM.write(58 + pointer2, valves[pointer - 1].programs[pointer2 - 1].minute);
                EEPROM.write(70 + pointer2, valves[pointer - 1].programs[pointer2 - 1].hour);
              }
              // case 4
              break;
            }
            // end switch pointer for saving to EEPROM starts minutes
            lcd.clear();
            pointer2 = 1;
            start();
            menuflag = 6;
            digiflag = 1;
            while (readButtons(0) > 0)
            {
              delay(100); // wait untill end of pressing
            }
          }
          break;
        }; // switch
        a = 0;
        break;
      }
      // menuflag = 8
      case 9:
      {
        // menuflag = 9 time and date
        a = readButtons(0);
        switch (a)
        {
          case 3:
          {
            // down
            if (pointer < 2)
            {
              pointer++;
              timedate();
            }
            while (readButtons(0) > 0)
            {
              delay(100); // wait untill end of pressing
            }
          }
          break;
          case 4:
          {
            // up
            if (pointer > 1)
            {
              pointer--;
              timedate();
            }
            while (readButtons(0) > 0)
            {
              delay(100); // wait untill end of pressing
            }
          }
          break;
          case 1:
          {
            // enter
            switch (pointer)
            {
              case 1:
              {
                // enter date
                lcd.clear();
                pointer = 1;
                digiflag = 1;
                rctdate();
                menuflag = 11;
                while (readButtons(0) > 0)
                {
                  delay(100); // wait untill end of pressing
                }
              }
              break;
              case 2:
              {
                // enter rcttime
                lcd.clear();
                pointer = 1;
                digiflag = 1;
                rcttime();
                menuflag = 10;
                while (readButtons(0) > 0)
                {
                  delay(100); // wait untill end of pressing
                }
                Serial.write("rct time");
              }
              break;
            }
            // switch '1'
          }
          break;
          case 2:
          {
            // quit
            lcd.clear();
            pointer = 1;
            DateTime dt(y, mo, d, h, m, 0, da);
            rtc.setDateTime(dt);
            mainmenuprt();
            menuflag = 1;
            while (readButtons(0) > 0)
            {
              delay(100); // wait untill end of pressing
            }
          }
          break;
        }; // switch
        a = 0;
        // menuflag =  9 time and date
        break;
      }
      case 10://menuflag =10 choose rct time
      {
        a = readButtons(0);
        switch (a)
        {
          case 3:
          {
            // down
            if (digiflag == 1)
            {
              if (h > 0)
                h--;
            }
            else
            {
              if (m > 0)
                m--;
            }
            delay(100);
            // while (readButtons(0) >0){ delay(100);}  //wait untill end of pressing
            rcttime();
          }
          break;
          case 4:
          {
            // up
            if (digiflag == 1)
            {
              if (h < 23)
                h++;
            }
            else
            {
              if (m < 59)
                m++;
            }
            delay(100);
            // while (readButtons(0) >0){ delay(100);}  //wait untill end of pressing
            rcttime();
          }
          break;
          case 1:
          {
            // enter
            if (digiflag == 1)
            {
              digiflag = 2;
              lcd.setCursor(10, 1);
              lcd.write("^");
              lcd.setCursor(7, 1);
              lcd.write(" ");
            }
            else
            {
              digiflag = 1;
              lcd.setCursor(10, 1);
              lcd.write(" ");
              lcd.setCursor(7, 1);
              lcd.write("^");
            }
            while (readButtons(0) > 0)
            {
              delay(100); // wait untill end of pressing
            }
          }
          break;
          case 2:
          {
            // quit
            lcd.clear();
            pointer = 1;
            DateTime dt(y, mo, d, h, m, 0, da);
            rtc.setDateTime(dt);
            timedate();
            menuflag = 9;
            while (readButtons(0) > 0)
            {
              delay(100); // wait untill end of pressing
            }
          }
          break;
        }; // switch
        a = 0;
        break;
      }
      // menuflag = 10
      case 11://menuflag =11 choose rct date
      {
        a = readButtons(0);
        switch (a)
        {
          case 3:
          {
            // down
            switch (digiflag)
            {
              case 1:
              {
                d--;
              }
              break; // 1
              case 2:
              {
                mo--;
              }
              break; // 2
              case 3:
              {
                y--;
              }
              break; // 3
              case 4:
              {
                da--;
              }
              break; // 4
            }
            // switch
            delay(100);
            // while (readButtons(0) >0){ delay(100);}  //wait untill end of pressing
            rctdate();
          }
          break;
          case 4:
          {
            // up
            switch (digiflag)
            {
              case 1:
              {
                d++;
              }
              break; // 1
              case 2:
              {
                mo++;
              }
              break; // 2
              case 3:
              {
                y++;
              }
              break; // 3
              case 4:
              {
                da++;
              }
              break; // 4
            }
            // switch
            delay(100);
            // while (readButtons(0) >0){ delay(100);}  //wait untill end of pressing
            rctdate();
          }
          break;
          case 1:
          {
            // enter
            if (digiflag < 4)
              digiflag++;
            else
              digiflag = 1;
            rctdate();
            while (readButtons(0) > 0)
            {
              delay(100); // wait untill end of pressing
            }
          }
          break;
          case 2:
          {
            // quit
            lcd.clear();
            pointer = 1;
            DateTime dt(y, mo, d, h, m, 0, da);
            rtc.setDateTime(dt);
            timedate();
            menuflag = 9;
            while (readButtons(0) > 0)
            {
              delay(100); // wait untill end of pressing
            }
          }
          break;
        }; // switch
        a = 0;
        break;
      }
      // menuflag =11
      case 12://  menuflag =12 valve status
      {
        a = readButtons(0);
        switch (a)
        {
          case 3:
          {
            // down
            if (pointer < 4)
            {
              pointer++;
              valvestatus();
            }
            while (readButtons(0) > 0)
            {
              delay(100); // wait untill end of pressing
            }
          }
          break;
          case 4:
          {
            // up
            if (pointer > 1)
            {
              pointer--;
              valvestatus();
            }
            while (readButtons(0) > 0)
            {
              delay(100); // wait untill end of pressing
            }
          }
          break;
          case 2:
          {
            // quit to main menu
            lcd.clear();
            EEPROM.write(46, valves[0].status);
            EEPROM.write(47, valves[1].status);
            EEPROM.write(48, valves[2].status);
            EEPROM.write(49, valves[3].status);
            pointer = 1;
            mainmenuprt();
            menuflag = 1;
            Serial.write("main menu");
            while (readButtons(0) > 0)
            {
            }
            // wait untill end of pressing
          }
          break;
          case 1:
          {
            // enter
            switch (pointer)
            {
              case 1:
              {
                // turn on manuel valve 1
                if (valves[pointer - 1].status == 0)
                {
                  valves[pointer - 1].status = 1;
                  lcd.setCursor(10, 1);
                  lcd.write("Enable ");
                  while (readButtons(0) > 0)
                  {
                    delay(100); // wait untill end of pressing
                  }
                }
                // if valves[pointer-1].status=0
                else
                {
                  valves[pointer - 1].status = 0;
                  lcd.setCursor(10, 1);
                  lcd.write("Disable");
                }
                // else valves[pointer-1].status=0
                while (readButtons(0) > 0)
                {
                  delay(100); // wait untill end of pressing
                }
              }
              break;
              case 2:
              {
                // turn on valves[pointer-1].status
                if (valves[pointer - 1].status == 0)
                {
                  valves[pointer - 1].status = 1;
                  lcd.setCursor(10, 2);
                  lcd.write("Enable ");
                  while (readButtons(0) > 0)
                  {
                    delay(100); // wait untill end of pressing
                  }
                }
                // if valves[pointer-1].status=0
                else
                {
                  valves[pointer - 1].status = 0;
                  lcd.setCursor(10, 2);
                  lcd.write("Disable");
                }
                // else valves[pointer-1].status=0
                while (readButtons(0) > 0)
                {
                  delay(100); // wait untill end of pressing
                }
              }
              break;
              case 3:
              {
                // turn on valves[pointer-1].status
                if (valves[pointer - 1].status == 0)
                {
                  valves[pointer - 1].status = 1;
                  lcd.setCursor(10, 3);
                  lcd.write("Enable ");
                }
                // if valves[pointer-1].status=0
                else
                {
                  valves[pointer - 1].status = 0;
                  lcd.setCursor(10, 3);
                  lcd.write("Disable");
                }
                // else valves[pointer-1].status=0
                while (readButtons(0) > 0)
                {
                  delay(100); // wait untill end of pressing
                }
              }
              break;
              case 4:
              {
                // turn on valves[pointer-1].statuse 4
                if (valves[pointer - 1].status == 0)
                {
                  valves[pointer - 1].status = 1;
                  lcd.setCursor(10, 1);
                  ;
                  lcd.write("Enable ");
                }
                // if valves[pointer-1].status=0
                else
                {
                  valves[pointer - 1].status = 0;
                  lcd.setCursor(10, 1);
                  lcd.write("Disable");
                }
                // else valves[pointer-1].status=0
                while (readButtons(0) > 0)
                {
                }
                // wait untill end of pressing
              }
              break;
              while (readButtons(0) > 0)
              {
                delay(100); // wait untill end of pressing
              }
            }
            // switch '1'
          }
          break;
        }; // switch
        a = 0;
        break;
      }
      // menuflag 12 valves status
      case 13://  menuflag = 13 hard reset
      {
        a = readButtons(0);
        switch (a)
        {
          case 3:
          {
            // down
            while (readButtons(0) > 0)
            {
              delay(100); // wait untill end of pressing
            }
          }
          break;
          case 4:
          {
            // up
            while (readButtons(0) > 0)
            {
              delay(100); // wait untill end of pressing
            }
          }
          break;
          case 1:
          {
            // enter
            delay(2000);
            if ((readButtons(0)) == 1)
            {
              for (int i = 0; i < 4; i++)
                //read the kind of program cycle/weekdays 0-3
              EEPROM.write(i, 0);
              for (int i = 0; i < 4; i++)
                //read the cycle programs 3-7
              EEPROM.write(i + 4, 0);
              for (int i = 1; i < 8; i++)
                // read the days of week 9-40
              EEPROM.write(i + 8, 0);
              for (int i = 1; i < 8; i++)
                EEPROM.write(i + 16, 0);
              for (int i = 1; i < 8; i++)
                EEPROM.write(i + 24, 0);
              for (int i = 1; i < 8; i++)
                EEPROM.write(i + 32, 0);
              for (int i = 0; i < 4; i++)
                //read the duration programs 41-45
              EEPROM.write(i + 41, 1);
              for (int i = 0; i < 4; i++)
                //read the status Enable/Disable 46-49
              EEPROM.write(i + 46, 0);
              for (int i = 50; i < 61; i++)
                // start time minute
              EEPROM.write(i, 0);
              for (int i = 62; i < 73; i++)
                // start time hour
              EEPROM.write(i, 24);
              lcd.clear();
              lcd.write("All Data has been");
              lcd.setCursor(0, 1);
              lcd.write("Deleted! Please Reset");
            }
          }
          break;
          case 2:
          {
            // quit
            lcd.clear();
            pointer = 1;
            mainmenuprt();
            menuflag = 1;
            Serial.write("main menu");
            while (readButtons(0) > 0)
            {
              delay(100); // wait untill end of pressing
            }
          }
          break;
        }; // switch
        a = 0;
        break; // menuflag =13 hard reset
      }
    }
    // switch all menu
  }
  // while all menu
  // MAIN SYSTEM CODE HERE
  now = rtc.now(); // get the current date-time
  uint32_t ts = now.minute();
  if (old_ts != ts)
  {
    old_ts = ts;
    timematch();
    dispmain();
    lcd.setCursor(11, 4);
    // lcd.write(0);
    display_position(temp);
    lcd.print((char) 223);
    lcd.write("C");
    Serial.print(now.month(), DEC);
    Serial.print(now.date(), DEC);
    Serial.print(' ');
    Serial.print(now.hour(), DEC);
    Serial.print(':');
    Serial.print(now.minute(), DEC);
    Serial.print(':');
    Serial.print(weekDay[now.dayOfWeek()]);
    if ((v1flag == 1) || (v2flag == 1) || (v3flag == 1) || (v4flag == 1))
      stoptimematch();
  }
  rtc.convertTemperature();
  temp = rtc.getTemperature();
  if (temp_old != temp)
  {
    temp_old = temp;
    lcd.setCursor(11, 4);
    display_position(temp);
    lcd.print((char) 223);
    lcd.write("C");
  }
  day = now.dayOfWeek();
  if (day != last_day)
    //do the cycle
  {
    last_day = day;
    if (valves[0].flag == 0)
      cc1--;
    if (valves[1].flag == 0)
      cc2--;
    if (valves[2].flag == 0)
      cc3--;
    if (valves[3].flag == 0)
      cc4--;
    if (cc1 < 0)
      cc1 = valves[0].cycle;
    if (cc2 < 0)
      cc2 = valves[1].cycle;
    if (cc3 < 0)
      cc3 = valves[2].cycle;
    if (cc4 < 0)
      cc4 = valves[3].cycle;
  }
  // end if day
  if (backlight_counter < 60)
    // 1# here is the backlight second delay
  second_counter = now.second();
  if (last_sec != second_counter)
    //do the cycle
  {
    backlight_counter++;
    last_sec = second_counter;
    if (backlight_counter == 60)
      // 2# here is the backlight second delay
    {
      lcd.setBacklight(LOW);
    }
    // end if second backlight
  }
  // end if second backlight
  if ((v1flag == 1) || (v2flag == 1) || (v3flag == 1) || (v4flag == 1))
  {
    cm = millis();
    blinkon();
  }
  // if system is on
  if (digitalRead(10) == LOW)
  {
    lcd.setCursor(0, 3);
    lcd.write("Raining");
  }
  else
  {
    lcd.setCursor(0, 3);
    lcd.write("        ");
  }
}

// end loop END MAIN CODE
void mainmenuprt()
{
  // print the main menu
  switch (pointer)
  {
    case 1:
    {
      lcd.setCursor(3, 0);
      lcd.write("- Main Menu -");
      lcd.setCursor(0, 1);
      lcd.write("~");
      lcd.setCursor(1, 1);
      lcd.write("Program             ");
      lcd.setCursor(1, 2);
      lcd.write("Manuel Control");
      lcd.setCursor(1, 3);
      lcd.write("Valves Status");
      lcd.setCursor(0, 3);
      lcd.write(" ");
      lcd.setCursor(0, 2);
      lcd.write(" ");
      break;
    }
    case 2:
    {
      lcd.setCursor(0, 1);
      lcd.write(" ");
      lcd.setCursor(0, 2);
      lcd.write("~");
      lcd.setCursor(0, 3);
      lcd.write(" ");
      break;
    }
    // case 2
    case 3:
    {
      lcd.setCursor(1, 1);
      lcd.write("Program            ");
      lcd.setCursor(1, 2);
      lcd.write("Manuel Control");
      lcd.setCursor(1, 3);
      lcd.write("Valves Status");
      lcd.setCursor(0, 1);
      lcd.write(" ");
      lcd.setCursor(0, 2);
      lcd.write(" ");
      lcd.setCursor(0, 3);
      lcd.write("~");
      break;
    }
    // case 3
    case 4:
    {
      lcd.setCursor(0, 1);
      lcd.write("~");
      lcd.setCursor(1, 1);
      lcd.write("Time and Date      ");
      lcd.setCursor(0, 2);
      lcd.write(" Hard Reset        ");
      lcd.setCursor(0, 3);
      lcd.write("                     ");
      break;
    }
    // case 4
    case 5:
    {
      lcd.setCursor(0, 1);
      lcd.write(" ");
      lcd.setCursor(0, 2);
      lcd.write("~");
      lcd.setCursor(0, 3);
      lcd.write(" ");
      break;
    }
    // case 2
  }
  // switch
}

// mainmenuprt
void dispmain()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.write(weekDay[now.dayOfWeek()]);
  lcd.setCursor(4, 0);
  tempdig = now.date();
  display_position(tempdig);
  lcd.write("/");
  tempdig = now.month();
  display_position(tempdig);
  lcd.setCursor(10, 0);
  tempdig = now.hour();
  display_position(tempdig);
  lcd.write(":");
  tempdig = now.minute();
  display_position(tempdig);
  lcd.setCursor(11, 4);
  display_position(temp);
  lcd.print((char) 223);
  lcd.write("C");
  if (valves[0].status == 1)
  {
    // what valves are on
    lcd.setCursor(18, 0);
    lcd.write("V1");
  }
  if (valves[1].status == 1)
  {
    lcd.setCursor(18, 1);
    lcd.write("V2");
  }
  if (valves[2].status == 1)
  {
    lcd.setCursor(18, 2);
    lcd.write("V3");
  }
  if (valves[3].status == 1)
  {
    lcd.setCursor(18, 3);
    lcd.write("V4");
  }
  if ((v1flag == 1) || (v2flag == 1) || (v3flag == 1) || (v4flag == 1))
  {
    lcd.setCursor(0, 1);
    lcd.write("System is ON");
    lcd.setCursor(0, 2);
    lcd.write("untill:");
    if (v1flag == 1)
    {
      lcd.setCursor(8, 2);
      display_position(finnishv1.hour);
      lcd.write(":");
      display_position(finnishv1.minute);
    }
    if (v2flag == 1)
    {
      lcd.setCursor(8, 2);
      display_position(finnishv2.hour);
      lcd.write(":");
      display_position(finnishv2.minute);
    }
    if (v3flag == 1)
    {
      lcd.setCursor(8, 2);
      display_position(finnishv3.hour);
      lcd.write(":");
      display_position(finnishv3.minute);
    }
    if (v4flag == 1)
    {
      lcd.setCursor(8, 2);
      display_position(finnishv4.hour);
      lcd.write(":");
      display_position(finnishv4.minute);
    }
  }
  // if system is on
  else
  {
    lcd.setCursor(0, 1);
    lcd.write("Last: ");
    display_position(last.hour);
    lcd.write(":");
    display_position(last.minute);
    lcd.write(" ");
    tempdig = ldate;
    display_position(tempdig);
    lcd.write("/");
    tempdig = lmonth;
    display_position(tempdig);
  }
}

// dispmain
void display_position(int digits)
{
  if (digits < 10)
    lcd.print("0");
  lcd.print(digits);
}

void programprt()
{
  // program menu
  switch (pointer)
  {
    case 1:
    {
      lcd.setCursor(3, 0);
      lcd.write("- Program -");
      lcd.setCursor(0, 1);
      lcd.write("~");
      lcd.setCursor(1, 1);
      lcd.write("Valve 1            ");
      lcd.setCursor(1, 2);
      lcd.write("Valve 2            ");
      lcd.setCursor(1, 3);
      lcd.write("Valve 3            ");
      lcd.setCursor(0, 3);
      lcd.write(" ");
      lcd.setCursor(0, 2);
      lcd.write(" ");
      break;
    }
    case 2:
    {
      lcd.setCursor(0, 1);
      lcd.write(" ");
      lcd.setCursor(0, 2);
      lcd.write("~");
      lcd.setCursor(0, 3);
      lcd.write(" ");
      break;
    }
    // case 2
    case 3:
    {
      lcd.setCursor(1, 1);
      lcd.write("Valve 1            ");
      lcd.setCursor(1, 2);
      lcd.write("Valve 2            ");
      lcd.setCursor(1, 3);
      lcd.write("Valve 3            ");
      lcd.setCursor(0, 1);
      lcd.write(" ");
      lcd.setCursor(0, 2);
      lcd.write(" ");
      lcd.setCursor(0, 3);
      lcd.write("~");
      break;
    }
    // case 3
    case 4:
    {
      lcd.setCursor(0, 1);
      lcd.write("~");
      lcd.setCursor(1, 1);
      lcd.write("Valve 4            ");
      lcd.setCursor(0, 2);
      lcd.write("                    ");
      lcd.setCursor(0, 3);
      lcd.write("                     ");
      break;
    }
    // case 4
  }
  // switch
}

// programsprt
void manuelprt()
{
  // manuel menu
  switch (pointer)
  {
    case 1:
    {
      lcd.setCursor(1, 0);
      lcd.write("- Manuel Control -");
      lcd.setCursor(0, 1);
      lcd.write("~");
      lcd.setCursor(1, 1);
      lcd.write("Valve 1:");
      if (valve1 == 1)
      {
        lcd.setCursor(10, 1);
        lcd.write("ON ");
      }
      else
      {
        lcd.setCursor(10, 1);
        lcd.write("OFF");
      }
      lcd.setCursor(1, 2);
      lcd.write("Valve 2:");
      if (valve2 == 1)
      {
        lcd.setCursor(10, 2);
        lcd.write("ON ");
      }
      else
      {
        lcd.setCursor(10, 2);
        lcd.write("OFF");
      }
      lcd.setCursor(1, 3);
      lcd.write("Valve 3:");
      if (valve3 == 1)
      {
        lcd.setCursor(10, 3);
        lcd.write("ON ");
      }
      else
      {
        lcd.setCursor(10, 3);
        lcd.write("OFF");
      }
      lcd.setCursor(0, 3);
      lcd.write(" ");
      lcd.setCursor(0, 2);
      lcd.write(" ");
      break;
    }
    case 2:
    {
      lcd.setCursor(0, 1);
      lcd.write(" ");
      lcd.setCursor(0, 2);
      lcd.write("~");
      lcd.setCursor(0, 3);
      lcd.write(" ");
      break;
    }
    // case 2
    case 3:
    {
      lcd.setCursor(1, 1);
      lcd.write("Valve 1:           ");
      if (valve1 == 1)
      {
        lcd.setCursor(10, 1);
        lcd.write("ON ");
      }
      else
      {
        lcd.setCursor(10, 1);
        lcd.write("OFF");
      }
      lcd.setCursor(1, 2);
      lcd.write("Valve 2:");
      if (valve2 == 1)
      {
        lcd.setCursor(10, 2);
        lcd.write("ON ");
      }
      else
      {
        lcd.setCursor(10, 2);
        lcd.write("OFF");
      }
      lcd.setCursor(1, 3);
      lcd.write("Valve 3:");
      if (valve3 == 1)
      {
        lcd.setCursor(10, 3);
        lcd.write("ON ");
      }
      else
      {
        lcd.setCursor(10, 3);
        lcd.write("OFF");
      }
      lcd.setCursor(0, 3);
      lcd.setCursor(0, 1);
      lcd.write(" ");
      lcd.setCursor(0, 2);
      lcd.write(" ");
      lcd.setCursor(0, 3);
      lcd.write("~");
      break;
    }
    // case 3
    case 4:
    {
      lcd.setCursor(0, 1);
      lcd.write("~");
      lcd.setCursor(1, 1);
      lcd.write("Valve 4:");
      if (valve4 == 1)
      {
        lcd.setCursor(10, 1);
        lcd.write("ON ");
      }
      else
      {
        lcd.setCursor(10, 1);
        lcd.write("OFF");
      }
      lcd.setCursor(0, 2);
      lcd.write("                    ");
      lcd.setCursor(0, 3);
      lcd.write("                     ");
      break;
    }
    // case 4
  }
  // switch
}

// manuelprt
void valvesproprt()
{
  // valves program screen
  switch (pointer2)
  {
    case 1:
    {
      lcd.setCursor(0, 1);
      lcd.write("~");
      lcd.setCursor(6, 0);
      lcd.write("Valve: ");
      lcd.print(pointer);
      lcd.setCursor(1, 1);
      lcd.write("Program type ");
      lcd.setCursor(1, 2);
      lcd.write("Start: ");
      lcd.setCursor(1, 3);
      lcd.write("Duration: ");
      lcd.setCursor(0, 2);
      lcd.write(" ");
      break;
    }
    case 2:
    {
      lcd.setCursor(0, 1);
      lcd.write(" ");
      lcd.setCursor(0, 2);
      lcd.write("~");
      lcd.setCursor(0, 3);
      lcd.write(" ");
      break;
    }
    // case 2
    case 3:
    {
      lcd.setCursor(0, 1);
      lcd.write(" ");
      lcd.setCursor(0, 2);
      lcd.write(" ");
      lcd.setCursor(0, 3);
      lcd.write("~");
      break;
    }
    // case 3
  }
  // switch
}

// valvesproprt
void programtype()
{
  switch (pointer2)
  {
    case 1:
    {
      lcd.setCursor(0, 1);
      lcd.write("~");
      lcd.setCursor(4, 0);
      lcd.write("Program Type");
      lcd.setCursor(1, 1);
      lcd.write("Type: ");
      lcd.setCursor(1, 2);
      lcd.write("Days:");
      lcd.setCursor(0, 2);
      lcd.write(" ");
      if (valves[pointer - 1].flag == 0)
      {
        lcd.setCursor(7, 1);
        lcd.write("Cycle        ");
        lcd.setCursor(7, 2);
        lcd.print(valves[pointer - 1].cycle);
      }
      else
      {
        lcd.setCursor(7, 1);
        lcd.write("Week days    ");
        switch (pointer)
        {
          case 1:
          {
            for (i = 1; i < 8; i++)
            {
              lcd.setCursor(6 + i, 2);
              if (valve1days[i] == 1)
              {
                lcd.print(i);
              }
              // if
              else
              {
                lcd.write("X");
              }
              // else
            }
            // for loop
            break;
          }
          case 2:
          {
            for (i = 1; i < 8; i++)
            {
              lcd.setCursor(6 + i, 2);
              if (valve2days[i] == 1)
              {
                lcd.print(i);
              }
              // if
              else
              {
                lcd.write("X");
              }
              // else
            }
            // for loop
            break;
          }
          // case 2
          case 3:
          {
            for (i = 1; i < 8; i++)
            {
              lcd.setCursor(6 + i, 2);
              if (valve3days[i] == 1)
              {
                lcd.print(i);
              }
              // if
              else
              {
                lcd.write("X");
              }
              // else
            }
            // for loop
            break;
          }
          // case 3
          case 4:
          {
            for (i = 1; i < 8; i++)
            {
              lcd.setCursor(6 + i, 2);
              if (valve4days[i] == 1)
              {
                lcd.print(i);
              }
              // if
              else
              {
                lcd.write("X");
              }
              // else
            }
            // for loop
            break;
          }
          // case 4
        }
        // switch
      }
      // else
      break;
    }
    case 2:
    {
      lcd.setCursor(0, 1);
      lcd.write(" ");
      lcd.setCursor(0, 2);
      lcd.write("~");
      lcd.setCursor(0, 3);
      lcd.write(" ");
      break;
    }
    // case 2
  }
}

// program type
void start()
{
  lcd.setCursor(3, 0);
  lcd.write("Start: Valve ");
  lcd.print(pointer);
  switch (pointer2)
  {
    case 1:
    {
      lcd.setCursor(0, 1);
      lcd.write("~");
      lcd.setCursor(0, 2);
      lcd.write(" ");
      for (int i = 0; i < 3; i++)
      {
        lcd.setCursor(1,(i + 1));
        if (valves[pointer - 1].programs[pointer2 - 1 + i].hour == 24)
        {
          lcd.write("XX:XX");
        }
        else
        {
          lcd.print(valves[pointer - 1].programs[pointer2 - 1 + i].hour);
          lcd.write(":");
          lcd.print(valves[pointer - 1].programs[pointer2 - 1 + i].minute);
        }
      }
      break;
    }
    case 2:
    {
      lcd.setCursor(0, 1);
      lcd.write(" ");
      lcd.setCursor(0, 2);
      lcd.write("~");
      lcd.setCursor(0, 3);
      lcd.write(" ");
      break;
    }
    // case 2
    case 3:
    {
      lcd.setCursor(0, 1);
      lcd.write(" ");
      lcd.setCursor(0, 2);
      lcd.write(" ");
      lcd.setCursor(0, 3);
      lcd.write("~");
      break;
    }
    // case 3
  }
  // switch
}

void duration()
{
  lcd.setCursor(3, 0);
  lcd.write("- Duration -");
  lcd.setCursor(6, 1);
  lcd.write("Valve ");
  lcd.print(pointer);
  lcd.setCursor(9, 2);
  lcd.print(valves[pointer - 1].duration);
}

void choosetime()
{
  lcd.clear();
  lcd.setCursor(5, 0);
  if (valves[pointer - 1].programs[pointer2 - 1].hour == 24)
  {
    lcd.write("XX:XX");
  }
  else
  {
    lcd.print(valves[pointer - 1].programs[pointer2 - 1].hour);
    lcd.write(":");
    lcd.print(valves[pointer - 1].programs[pointer2 - 1].minute);
  }
  if (digiflag == 2)
  {
    lcd.setCursor(8, 1);
    lcd.write("^");
    lcd.setCursor(5, 1);
    lcd.write(" ");
  }
  else
  {
    lcd.setCursor(8, 1);
    lcd.write(" ");
    lcd.setCursor(5, 1);
    lcd.write("^");
  }
}

void timedate()
{
  lcd.setCursor(0, 0);
  lcd.write("- Time and date -");
  switch (pointer)
  {
    case 1:
    {
      lcd.setCursor(0, 1);
      lcd.write("~");
      lcd.setCursor(0, 2);
      lcd.write(" ");
      lcd.setCursor(1, 1);
      display_position(d);
      lcd.write("/");
      display_position(mo);
      lcd.write("/");
      lcd.print(y);
      lcd.write(" ");
      lcd.print(weekDay[da]);
      lcd.setCursor(1, 2);
      display_position(h);
      lcd.write(":");
      display_position(m);
      break;
    }
    case 2:
    {
      lcd.setCursor(0, 1);
      lcd.write(" ");
      lcd.setCursor(0, 2);
      lcd.write("~");
      break;
    }
    // case 2
  }
}

void rcttime()
{
  lcd.clear();
  lcd.setCursor(7, 0);
  display_position(h);
  lcd.write(":");
  display_position(m);
  if (digiflag == 2)
  {
    lcd.setCursor(10, 1);
    lcd.write("^");
    lcd.setCursor(7, 1);
    lcd.write(" ");
  }
  else
  {
    lcd.setCursor(10, 1);
    lcd.write(" ");
    lcd.setCursor(7, 1);
    lcd.write("^");
  }
}

void rctdate()
{
  lcd.clear();
  lcd.setCursor(2, 0);
  display_position(d);
  lcd.write("/");
  display_position(mo);
  lcd.write("/");
  lcd.print(y);
  lcd.write(" ");
  lcd.print(weekDay[da]);
  switch (digiflag)
  {
    case 1:
    {
      lcd.setCursor(2, 1);
      lcd.write("^");
      lcd.setCursor(13, 1);
      lcd.write(" ");
    }
    break; // 1
    case 2:
    {
      lcd.setCursor(5, 1);
      lcd.write("^");
      lcd.setCursor(2, 1);
      lcd.write(" ");
    }
    break; // 2
    case 3:
    {
      lcd.setCursor(8, 1);
      lcd.write("^");
      lcd.setCursor(5, 1);
      lcd.write(" ");
    }
    break; // 3
    case 4:
    {
      lcd.setCursor(13, 1);
      lcd.write("^");
      lcd.setCursor(8, 1);
      lcd.write(" ");
    }
    break; // 4
  }
  // switch
}

void valvestatus()
{
  switch (pointer)
  {
    case 1:
    {
      lcd.setCursor(1, 0);
      lcd.write("- Valve status -");
      lcd.setCursor(0, 1);
      lcd.write("~");
      lcd.setCursor(1, 1);
      lcd.write("Valve 1:");
      if (valves[0].status == 1)
      {
        lcd.setCursor(10, 1);
        lcd.write("Enable ");
      }
      else
      {
        lcd.setCursor(10, 1);
        lcd.write("Disable");
      }
      lcd.setCursor(1, 2);
      lcd.write("Valve 2:");
      if (valves[1].status == 1)
      {
        lcd.setCursor(10, 2);
        lcd.write("Enable ");
      }
      else
      {
        lcd.setCursor(10, 2);
        lcd.write("Disable");
      }
      lcd.setCursor(1, 3);
      lcd.write("Valve 3:");
      if (valves[2].status == 1)
      {
        lcd.setCursor(10, 3);
        lcd.write("Enable ");
      }
      else
      {
        lcd.setCursor(10, 3);
        lcd.write("Disable");
      }
      lcd.setCursor(0, 3);
      lcd.write(" ");
      lcd.setCursor(0, 2);
      lcd.write(" ");
      break;
    }
    case 2:
    {
      lcd.setCursor(0, 1);
      lcd.write(" ");
      lcd.setCursor(0, 2);
      lcd.write("~");
      lcd.setCursor(0, 3);
      lcd.write(" ");
      break;
    }
    // case 2
    case 3:
    {
      lcd.setCursor(1, 1);
      lcd.write("Valve 1:           ");
      if (valves[0].status == 1)
      {
        lcd.setCursor(10, 1);
        lcd.write("Enable ");
      }
      else
      {
        lcd.setCursor(10, 1);
        lcd.write("Disable");
      }
      lcd.setCursor(1, 2);
      lcd.write("Valve 2:");
      if (valves[1].status == 1)
      {
        lcd.setCursor(10, 2);
        lcd.write("Enable ");
      }
      else
      {
        lcd.setCursor(10, 2);
        lcd.write("Disable");
      }
      lcd.setCursor(1, 3);
      lcd.write("Valve 3:");
      if (valves[2].status == 1)
      {
        lcd.setCursor(10, 3);
        lcd.write("Enable ");
      }
      else
      {
        lcd.setCursor(10, 3);
        lcd.write("Disable");
      }
      lcd.setCursor(0, 3);
      lcd.setCursor(0, 1);
      lcd.write(" ");
      lcd.setCursor(0, 2);
      lcd.write(" ");
      lcd.setCursor(0, 3);
      lcd.write("~");
      break;
    }
    // case 3
    case 4:
    {
      lcd.setCursor(0, 1);
      lcd.write("~");
      lcd.setCursor(1, 1);
      lcd.write("Valve 4:");
      if (valves[3].status == 1)
      {
        lcd.setCursor(10, 1);
        lcd.write("Enable ");
      }
      else
      {
        lcd.setCursor(10, 1);
        lcd.write("Disable");
      }
      lcd.setCursor(0, 2);
      lcd.write("                    ");
      lcd.setCursor(0, 3);
      lcd.write("                     ");
      break;
    }
    // case 4
  }
  // switch
}

// vavle status
void hardreset()
{
  lcd.setCursor(3, 0);
  lcd.write("- Hard Reset -");
  lcd.setCursor(0, 1);
  lcd.write("To delete press for");
  lcd.setCursor(0, 2);
  lcd.write("5 seconds.");
}

void timematch()
{
  if ((valves[0].flag == 1) && (valves[0].status == 1))
  {
    if (valve1days[now.dayOfWeek()] == 1)
      dayf1 = 1;
    else
      dayf1 = 0;
  }
  // check day of week for v1
  if ((valves[1].flag == 1) && (valves[1].status == 1))
  {
    if (valve2days[now.dayOfWeek()] == 1)
      dayf2 = 1;
    else
      dayf2 = 0;
  }
  // check day of week for v2
  if ((valves[2].flag == 1) && (valves[2].status == 1))
  {
    if (valve3days[now.dayOfWeek()] == 1)
      dayf3 = 1;
    else
      dayf3 = 0;
  }
  // check day of week for v3
  if ((valves[3].flag == 1) && (valves[3].status == 1))
  {
    if (valve4days[now.dayOfWeek()] == 1)
      dayf4 = 1;
    else
      dayf4 = 0;
  }
  // check day of week for v4
  if ((valves[0].flag == 0) && (valves[0].status == 1))
    if (cc1 == 0)
      dayf1 = 1;
  else
    dayf1 = 0;
  // check cycle of week for v1
  if ((valves[1].flag == 0) && (valves[1].status == 1))
    if (cc2 == 0)
      dayf2 = 1;
  else
    dayf2 = 0;
  // check cycle of week for v2
  if ((valves[2].flag == 0) && (valves[2].status == 1))
    if (cc3 == 0)
      dayf3 = 1;
  else
    dayf3 = 0;
  // check cycle of week for v3
  if ((valves[3].flag == 0) && (valves[3].status == 1))
    if (cc4 == 0)
      dayf4 = 1;
  else
    dayf4 = 0;
  // check cycle of week for v4
  if ((valves[0].status == 1) && (dayf1 == 1) && (v1flag == 0) && (digitalRead(10) == HIGH))
    // check for v1
  for (int i = 0; i < 3; i++)
    if ((valves[0].programs[i].minute == now.minute()) && (valves[0].programs[i].hour == now.hour()))
    {
      digitalWrite(2, LOW);
      v1flag = 1;
      activef = 1;
      if ((valves[0].programs[i].minute + valves[0].duration) < 60)
      {
        finnishv1.minute = valves[0].programs[i].minute + valves[0].duration;
        finnishv1.hour = valves[0].programs[i].hour;
      }
      else
      {
        if (valves[0].programs[i].hour == 23)
          finnishv1.hour = 0;
        else
          finnishv1.hour = valves[0].programs[i].hour + 1;
        finnishv1.minute = (valves[0].programs[i].minute + valves[0].duration) - 60;
      }
      Serial.print(finnishv1.hour);
      Serial.print(finnishv1.minute);
    }
  // if v1
  if ((valves[1].status == 1) && (dayf2 == 1) && (v2flag == 0) && (digitalRead(10) == HIGH))
    // check for v2
  for (int i = 0; i < 3; i++)
    if ((valves[1].programs[i].minute == now.minute()) && (valves[1].programs[i].hour == now.hour()))
    {
      digitalWrite(3, LOW);
      v2flag = 1;
      activef = 1;
      if ((valves[1].programs[i].minute + valves[1].duration) < 60)
      {
        finnishv2.minute = valves[1].programs[i].minute + valves[1].duration;
        finnishv2.hour = valves[1].programs[i].hour;
      }
      else
      {
        if (valves[1].programs[i].hour == 23)
          finnishv2.hour = 0;
        else
          finnishv2.hour = valves[1].programs[i].hour + 1;
        finnishv2.minute = (valves[1].programs[i].minute + valves[1].duration) - 60;
      }
      Serial.print(finnishv2.hour);
      Serial.print(finnishv2.minute);
    }
  // if v2
  if ((valves[2].status == 1) && (dayf3 == 1) && (v3flag == 0) && (digitalRead(10) == HIGH))
    // check for v3
  for (int i = 0; i < 3; i++)
    if ((valves[2].programs[i].minute == now.minute()) && (valves[2].programs[i].hour == now.hour()))
    {
      digitalWrite(4, LOW);
      v3flag = 1;
      activef = 1;
      if ((valves[2].programs[i].minute + valves[2].duration) < 60)
      {
        finnishv3.minute = valves[2].programs[i].minute + valves[2].duration;
        finnishv3.hour = valves[2].programs[i].hour;
      }
      else
      {
        if (valves[2].programs[i].hour == 23)
          finnishv3.hour = 0;
        else
          finnishv3.hour = valves[2].programs[i].hour + 1;
        finnishv3.minute = (valves[2].programs[i].minute + valves[2].duration) - 60;
      }
      Serial.print(finnishv3.hour);
      Serial.print(finnishv3.minute);
    }
  // if v3
  if ((valves[3].status == 1) && (dayf4 == 1) && (v4flag == 0) && (digitalRead(10) == HIGH))
    // check for v4
  for (int i = 0; i < 3; i++)
    if ((valves[3].programs[i].minute == now.minute()) && (valves[3].programs[i].hour == now.hour()))
    {
      digitalWrite(5, LOW);
      v4flag = 1;
      activef = 1;
      if ((valves[3].programs[i].minute + valves[3].duration) < 60)
      {
        finnishv4.minute = valves[3].programs[i].minute + valves[3].duration;
        finnishv4.hour = valves[3].programs[i].hour;
      }
      else
      {
        if (valves[3].programs[i].hour == 23)
          finnishv4.hour = 0;
        else
          finnishv4.hour = valves[3].programs[i].hour + 1;
        finnishv4.minute = (valves[3].programs[i].minute + valves[3].duration) - 60;
      }
      Serial.print(finnishv4.hour);
      Serial.print(finnishv4.minute);
    }
  // if v4
}

// time match
void stoptimematch()
{
  if (v1flag == 1)
    // check for v1
  if ((finnishv1.minute == now.minute()) && (finnishv1.hour == now.hour()))
  {
    digitalWrite(2, HIGH);
    v1flag = 0;
    dayf1 = 0;
    last.minute = now.minute();
    last.hour = now.hour();
    lmonth = now.month();
    ldate = now.date();
  }
  // if v1
  if (v2flag == 1)
    // check for v2
  if ((finnishv2.minute == now.minute()) && (finnishv2.hour == now.hour()))
  {
    digitalWrite(3, HIGH);
    v2flag = 0;
    dayf2 = 0;
    last.minute = now.minute();
    last.hour = now.hour();
    lmonth = now.month();
    ldate = now.date();
  }
  // if v2
  if (v3flag == 1)
    // check for v3
  if ((finnishv3.minute == now.minute()) && (finnishv3.hour == now.hour()))
  {
    digitalWrite(4, HIGH);
    v3flag = 0;
    dayf3 = 0;
    last.minute = now.minute();
    last.hour = now.hour();
    lmonth = now.month();
    ldate = now.date();
  }
  // if v3
  if (v4flag == 1)
    // check for v4
  if ((finnishv4.minute == now.minute()) && (finnishv4.hour == now.hour()))
  {
    digitalWrite(5, HIGH);
    v4flag = 0;
    dayf4 = 0;
    last.minute = now.minute();
    last.hour = now.hour();
    lmonth = now.month();
    ldate = now.date();
  }
  // if v4
}

// stop time match
void blinkon()
{
  if ((cm - pm) > 500)
  {
    pm = cm;
    if (v1flag == 1)
    {
      if (fblink == 1)
      {
        lcd.setCursor(18, 0);
        lcd.write("V1");
        fblink = 0;
      }
      // if fblink
      else
      {
        lcd.setCursor(18, 0);
        lcd.write("  ");
        fblink = 1;
      }
      // else
    }
    if (v2flag == 1)
    {
      if (fblink == 1)
      {
        lcd.setCursor(18, 1);
        lcd.write("V2");
        fblink = 0;
      }
      // if fblink
      else
      {
        lcd.setCursor(18, 1);
        lcd.write("  ");
        fblink = 1;
      }
      // else
    }
    if (v3flag == 1)
    {
      if (fblink == 1)
      {
        lcd.setCursor(18, 2);
        lcd.write("V3");
        fblink = 0;
      }
      // if fblink
      else
      {
        lcd.setCursor(18, 3);
        lcd.write("  ");
        fblink = 1;
      }
      // else
    }
    if (v4flag == 1)
    {
      if (fblink == 1)
      {
        lcd.setCursor(18, 3);
        lcd.write("V4");
        fblink = 0;
      }
      // if fblink
      else
      {
        lcd.setCursor(18, 4);
        lcd.write("  ");
        fblink = 1;
      }
      // else
    }
  }
  // if
}

// end blink ok
/*while (menuflag == 0){ //menuflag =
a=readButtons(0);
switch (a)
{
case 3: { //down
while (readButtons(0) >0){ delay(100);}  //wait untill end of pressing
}break;
case 4: {  //up
while (readButtons(0) >0){ delay(100);}  //wait untill end of pressing
}break;
case 1: {  //enter
}break;
case 2: {  //quit
while (readButtons(0) >0){ delay(100);}  //wait untill end of pressing
}break;
};// switch
a = 0;
} // while menuflag =  */
/*   if (cm - pm >= interval) {
// save the last time you blinked the LED
pm = cm;
if (digitglag == LOW) {
digitglag = HIGH;
} else {
digitglag = LOW;
}
}// timer*/