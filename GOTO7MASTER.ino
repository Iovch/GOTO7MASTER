/*
 GOTO7MASTER.ino File Written by Igor Ovchinnikov 31/08/2016
 скетч управления альт-азимутальной монтировкой
*/

#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27,16,2);

#include "GOTO7Config.h"

const unsigned long StarMSPS=86164091; // Милисекунд в Звездных сутках

int imStepsXPS = iStepsXPS*iXStepX; //Микрошагов в секунду на двигателе X
int imStepsYPS = iStepsYPS*iYStepX; //Микрошагов в секунду на двигателе Y

unsigned long ulSPX = iStepsDX*dRDX*iXStepX; //Микрошагов двигателя на полный оборот оси X
unsigned long ulSPY = iStepsDY*dRDY*iYStepX; //Микрошагов двигателя на полный оборот оси Y

const unsigned long ulMaxValue=0xFFFFFFFF; //Максимальное значение unsigned long

unsigned long VRAperSTEP=ulMaxValue/ulSPX;   //Единиц прямого восхождения на 1 шаг двигателя
unsigned long VDEperSTEP=ulMaxValue/ulSPY;   //Единиц склонения на 1 шаг двигателя
unsigned long VTperMS=ulMaxValue/StarMSPS; //Единиц звездного времени на 1 милисекунду
unsigned long ulLST=0;                //Местное звездное время

const int DefaultDelay=int(StarMSPS/ulSPX); //Периодичность шага ДПВ для компенсации вращения Земли

unsigned long dVRAperSTEP=ulMaxValue/StarMSPS*1000/imStepsXPS; //Поправка вращения Земли на 1 шаг ДПВ
unsigned long dVDEperSTEP=0; //Поправка (доворот) ДСК в единицах СК на 1 шаг ДСК

unsigned long ulMilisec=millis();   // Время предыдущего шага
unsigned long iPortTimer=millis();  // Время предыдущей посылки данных в порт
unsigned long iPultTimer=0;         // Время ожидания пульта

unsigned long iPRA=0;   //Текущее (исходное) значение прямого восхождения
unsigned long iPDE=0;   //Текущее (исходное) значение склонения
unsigned long iPAZ=0x0; //Текущее (исходное) значение азимута 0 Север (Юг 0x7FFFFFFF)
unsigned long iPZE=0x3FFFFFFF; //Текущее (исходное) значение зенитного растояния 90 (H=0)
unsigned long iPT= 0x0; //Текущее (исходное) значение часового угла 0 (Север)
unsigned long iToPRA=0; //Целевое значение прямого восхождения
unsigned long iToPDE=0; //Целевое значение склонения
unsigned long iToPAZ=0; //Целевое значение азимута
unsigned long iToPZE=0; //Целевое значение зенитного растояния
unsigned long iToPT=0;  //Целевое значение часового угла

int iLastMovement = 0;    // Направление перемещения монтировки в предыдущем цикле
int iMovement = 0;        // Направление перемещения монтировки в следующем цикле
int iSteps = 0;           // Подсчет количества шагов
int iStDY  = 0;           // Направление шага двигателя Y

boolean bAlignment=false;  // Выравнивание не выполнено
boolean bRun=false;        // Ведение выключено изначально
boolean bLCD=false;        // LCD врёт
boolean bForceX  = false;  //Ускоренный режим Х
boolean bForceY  = false;  //Ускоренный режим Y

String STR= "", STR1="", STR2="";
String LCDString1=" Azduino GOTO7  ";
String LCDString2="  Ready to Use  ";

#include "GOTO7MASTER.h"

void action(String STRA)
{
  char cAction;
  cAction=STRA.charAt(0);
  switch (cAction)
  {
    case 'e': {Serial.print(HexTo8D(iPRA)); Serial.print(iPRA,HEX); Serial.print(",");
               Serial.print(HexTo8D(iPDE)); Serial.print(iPDE,HEX); Serial.print("#");
               break;}
    case 'z': {
               Serial.print(360.0*double(iPAZ>>8)/double(ulMaxValue>>8));      Serial.print(",");
               Serial.print(90.0-360.0*double(iPZE>>8)/double(ulMaxValue>>8)); Serial.println("#");
               break;
               }
    case 'b': {
               GetSubStr ();
               if(STR1!="") iToPAZ=STR1.toFloat()/360.0*ulMaxValue; else iToPAZ=iPAZ; //Это только для ручного ввода азимута в градусах (0-360)
               if(STR2!="") iToPZE=(90.0-STR2.toFloat())/360.0*ulMaxValue; else iToPZE=iPZE; //Это только для ручного ввода высоты в градусах (±90)
//               iToPAZ=StrToHEX(STR1); //Это для компьютера
//               iToPZE=StrToHEX(STR2); //Это для компьютера
               ToAZaH(true);
               Serial.println("#");
               break; 
              }
    case 'r': {
               GetSubStr ();
               iToPRA=StrToHEX(STR1);
               iToPDE=StrToHEX(STR2);
               iToPT = iToPRA-ulLST+ulMaxValue/2;         //Целевой часовой угол
               iToPZE=ZeFromDeT(iToPDE, iToPT);           //Целевое значение зенитного растояния
               iToPAZ=AzFromDeZeT(iToPDE, iToPZE, iToPT); //Целевое значение азимута
               ToAZaH (true);
               Serial.print("#");
               break;}
  };
}

long AskControl()
{
  long iRetValue=0;
  iRetValue=iRetValue|AskJOY();
  iRetValue=iRetValue|AskSlaveI2C();
  return iRetValue;
 }

void reaction () //Обработка команд ПУ
  {
   long iKey=0, iLastKey=0;
       
// Здесь мы договариваемся, что функция int AskControl(),
// к чему бы она ни была привязана, возвращает при ее вызове следующие значения:

//   0 - когда ничего не надо делать
//   1 - Fokus-
//   2 - включить ускоренное управление монтировкой
//   3 - Fokus+
//   5 - включить замедленное управление монтировкой
//  11 - Телескоп слева от полярной оси
//  12 - Телескоп справа от полярной оси
//  10 - Сброс выравнивания монтировки
//  51 - включить трекинг
//  52 - отключить трекинг
// 65536 - включить/отключить трекинг

  do 
   {
    iMovement=0;
    iKey=AskControl();
    if (iKey == 1) {Stepper_Z_step(-1);} //Фокусировка (-)
    if (iKey == 3) {Stepper_Z_step( 1);} //Фокусировка (+)
    if (iKey == 2) bForceIR = true;  //Ускоренный режим управления с пульта ИК
    if (iKey == 5) bForceIR = false; //Замедленный режим управления с пульта ИК
    if ((iKey &   256)==  256 && iStDX!=0) {Force_X(false); Stepper_X_step( iStDX); bLCD=false; iMovement=iMovement |   256;} // Микрошаг вперед
    if ((iKey &   512)==  512 && iStDX!=0) {Force_X(true);  Stepper_X_step( iStDX); iPAZ+=VRAperSTEP; iMovement=iMovement |   512;} // Полный шаг вперед
    if ((iKey &  1024)== 1024 && iStDX!=0) {Force_X(false); Stepper_X_step(-iStDX); bLCD=false; iMovement=iMovement |  1024;} // Микрошаг назад
    if ((iKey &  2048)== 2048 && iStDX!=0) {Force_X(true);  Stepper_X_step(-iStDX); iPAZ-=VRAperSTEP; iMovement=iMovement |  2048;} // Полный шаг назад
    if ((iKey &  4096)== 4096 && iStDY!=0) {Force_Y(false); Stepper_Y_step(-iStDY); bLCD=false; iMovement=iMovement |  4096;} // Микрошаг вверх
    if ((iKey &  8192)== 8192 && iStDY!=0) {Force_Y(true);  Stepper_Y_step(-iStDY); iPZE-=VDEperSTEP; iMovement=iMovement |  8192;} // Полный шаг вверх   
    if ((iKey & 16384)==16384 && iStDY!=0) {Force_Y(false); Stepper_Y_step( iStDY); bLCD=false; iMovement=iMovement | 16384;} // Микрошаг вниз
    if ((iKey & 32768)==32768 && iStDY!=0) {Force_Y(true);  Stepper_Y_step( iStDY); iPZE+=VDEperSTEP; iMovement=iMovement | 32768;} // Полный шаг вниз
  
    if ((iMovement&512)==512||(iMovement&2048)==2048||(iMovement&8192)==8192||(iMovement&32768)==32768) // Если монтировка сдвинулась
     {
       iPDE=DeFromZeAz(iPZE,iPAZ);   // Cклонение
       iPT =TFromDeZeAz(iPDE, iPZE, iPAZ);  // Часовой угол
       iLastMovement = iMovement;
       bLCD=false; 
     };

    if ((iKey&65536)==65536) 
     {
      if(!bRun) {iKey=51;} //Включить  ведение (Tracking ON)
      else      {iKey=52;} //Отключить ведение (Tracking OFF)
      delay(250);
     }
      
    if (iKey==51 && !bRun && bAlignment) //Включить ведение  (Tracking ON)
     {
      bRun=true;
      bLCD=false;
      iPDE=DeFromZeAz(iPZE,iPAZ); // Cклонение
      iPT =TFromDeZeAz(iPDE, iPZE, iPAZ); // Часовой угол
      ulMilisec=millis();
     }
     
   if (iKey==52 && bRun) {bRun=false; bLCD=false;} //Отключить ведение (Tracking OFF)

   if (iKey==8 && !bRun)
    {
     iPultTimer=millis(); 
     do
      {
       AskClock(); 
       LCDTimePrint();
       delay(250);
       iKey=AskControl();
       if(iKey==8) {bForceIR = false; GetTime();}
      } while (iKey!=65536 && iKey!=11 && iKey!=12 );
     bLCD=false; 
    }
   
   if (iKey==11) if(iStDY!= iStDY0) {iStDY= iStDY0; bLCD=false;} //Телескоп слева от полярной оси
   if (iKey==12) if(iStDY!=-iStDY0) {iStDY=-iStDY0; bLCD=false;} //Телескоп справа от полярной оси
 
   if (iKey==10) //Установка/Сброс выравнивания монтировки
    {
     LCDCOR(iKey); 
     if(!bAlignment)
      {
       iPultTimer=millis();
       do
       {
        iKey=AskControl();
        switch (iKey)
         {
         case     1: {iPAZ=iAzAli1/360.0*ulMaxValue; iPZE=(90.0-iHAli1)/360.0*ulMaxValue; bAlignment=true; break;} //Первая пользовательская точка выравнивания монтировки
         case     2: {iPAZ=iAzAli2/360.0*ulMaxValue; iPZE=(90.0-iHAli2)/360.0*ulMaxValue; bAlignment=true; break;} //Вторая пользовательская точка выравнивания монтировки
         case     3: {iPAZ=iAzAli3/360.0*ulMaxValue; iPZE=(90.0-iHAli3)/360.0*ulMaxValue; bAlignment=true; break;} //Третья пользовательская точка выравнивания монтировки
         case  4096:
         case  8192: {iPAZ=0x0;        iPZE=0x3FFFFFFF; bAlignment=true; break;}  //Азимут   0 (Север)  Зенитное расстояние 90 (мат.горизонт)
         case   256:
         case   512: {iPAZ=0x3FFFFFFF; iPZE=0x3FFFFFFF; bAlignment=true; break;}  //Азимут  90 (Восток) Зенитное расстояние 90 (мат.горизонт)
         case 16384:
         case 32768: {iPAZ=0x7FFFFFFF; iPZE=0x3FFFFFFF; bAlignment=true; break;}  //Азимут 180 (Юг)     Зенитное расстояние 90 (мат.горизонт)
         case  1024:
         case  2048: {iPAZ=0xBFFFFFFD; iPZE=0x3FFFFFFF; bAlignment=true; break;}  //Азимут 270 (Запад)  Зенитное расстояние 90 (мат.горизонт)
         }
       } while (millis()-iPultTimer < 2000 && !bAlignment);
      ulLST=ulMaxValue/24*LST();
      iPDE=DeFromZeAz(iPZE,iPAZ);
      iPRA=TFromDeZeAz(iPDE,iPZE,iPAZ)+ulLST-ulMaxValue/2;
      iPT = iPRA-ulLST+ulMaxValue/2;
      }
      else {bAlignment=false; delay(500);}
      bLCD=false;
      ulMilisec=millis();
    }
   if (iKey != iLastKey){LCDCOR(iKey); iLastKey=iKey; bLCD=false;}
   } while (iKey!=0);
  LCDPrint();
 } 

void ToAZaH (boolean bForceit)
{
  int iDIR1=0, iDIR2=0; //Знак изменения значения координат 1,2
  int iStepD1=-iStDX;   //Направление шага ШД1
  int iStepD2=-iStDY;   //Направление шага ШД2
  unsigned long ulStartMilis=millis();      //Время начала выполнения перемещения
  unsigned long ulD1=0, ulD2=0;             //Разница координат
  unsigned long ulMax1=ulMaxValue, ulMax2=ulMaxValue;   //Максимальные значения координат
  unsigned long ulTo1=iToPAZ, ulTo2=iToPZE; //Целевые значения координат
  unsigned long ulFrom1=iPAZ, ulFrom2=iPZE; //Текущие координаты
  unsigned long ulCor1PS=0, ulCor2PS=0;     //Корректировка движения за шаг

  Force_X(bForceit);
  Force_Y(bForceit);
  
  if (ulTo1 > ulFrom1) {ulD1 = (ulTo1-ulFrom1); iDIR1=  1;}
  if (ulTo1 < ulFrom1) {ulD1 = (ulFrom1-ulTo1); iDIR1= -1;}
  if (ulD1 > ulMax1/2) {ulD1 = ulMax1-ulD1; iDIR1 = -(iDIR1);}

  if (ulTo2 > ulFrom2) {ulD2 = (ulTo2-ulFrom2); iDIR2=  1;}
  if (ulTo2 < ulFrom2) {ulD2 = (ulFrom2-ulTo2); iDIR2= -1;}
  if (ulD2 > ulMax2/2) {ulD2 = ulMax2-ulD2; iDIR2 = -(iDIR2);}

  if (ulD1 > ulMax1/2) return; //Ошибка в расчете шагов по первой координате
  if (ulD2 > ulMax2/2) return; //Ошибка в расчете шагов по второй координате

  if (bAlignment)
  {
   while (((ulD1 > VRAperSTEP) && iStepD1!=0) || ((ulD2 > VDEperSTEP) && iStepD2!= 0))
   {
    if (ulD1 > VRAperSTEP)
     {
      if (iDIR1 > 0) {Stepper_X_step(-iStepD1); ulD1-=(VRAperSTEP+ulCor1PS); ulFrom1+=(VRAperSTEP+ulCor1PS);}
      if (iDIR1 < 0) {Stepper_X_step( iStepD1); ulD1-=(VRAperSTEP-ulCor1PS); ulFrom1-=(VRAperSTEP-ulCor1PS);}
      if (ulD1 > ulMax1/2) ulD1 = 0;
      if ((ulD1 < VRAperSTEP) && bForceX) Force_X(false);
     }
    if (ulD2 > VDEperSTEP)
     {
      if (iDIR2 > 0) {Stepper_Y_step(-iStepD2); ulD2-=(VDEperSTEP+ulCor2PS); ulFrom2+=(VDEperSTEP+ulCor2PS);}
      if (iDIR2 < 0) {Stepper_Y_step( iStepD2); ulD2-=(VDEperSTEP+ulCor2PS); ulFrom2-=(VDEperSTEP+ulCor2PS);}
      if (ulD2 > ulMax2/2) ulD2 = 0;
      if ((ulD2 < VDEperSTEP) && bForceY) Force_Y(false);
     }
    if ((millis()-ulStartMilis)>1000)
     {
      iPAZ=ulFrom1;
      iPZE=ulFrom2;
      iPDE=DeFromZeAz(iPZE,iPAZ);
      iPRA=TFromDeZeAz(iPDE,iPZE,iPAZ)+ulLST-ulMaxValue/2;
      ulStartMilis=millis();
      action("e");
      bLCD=false; 
      LCDPrint();
     }
   }
  }
   else  //Первая команда GOTO задает координаты наведения телескопа, без его реального перемещения
   {
    ulFrom1=iToPAZ;
    ulFrom2=iToPZE;
    bAlignment=true;
   }
  iPAZ=ulFrom1;
  iPZE=ulFrom2;
  if (bForceit)
  {
  iPDE=DeFromZeAz(iPZE,iPAZ);
  iPRA=TFromDeZeAz(iPDE,iPZE,iPAZ)+ulLST-ulMaxValue/2;
  iPT = iPRA-ulLST+ulMaxValue/2;
  }
  ulMilisec=millis();
  bLCD=false;
 }

unsigned long DeFromZeAz(unsigned long pZE, unsigned long pAZ)   //Склонение из ZE и AZ
{
 double dDe =0;
 double dFi =Latitude/360.0;
 double dZe=double(pZE>>8)/double(ulMaxValue>>8);
 double dAz=double(pAZ>>8)/double(ulMaxValue>>8);
 dDe=DeFromZeAzFi(dZe,dAz,dFi);
 return dDe*double(ulMaxValue>>8)*256.0;
}

unsigned long TFromDeZeAz(unsigned long pDE, unsigned long pZE, unsigned long pAZ)
{
 double dZe=double(pZE>>8)/double(ulMaxValue>>8);
 double dDe=double(pDE>>8)/double(ulMaxValue>>8);
 double dAz=double(pAZ>>8)/double(ulMaxValue>>8);
 double dFi=Latitude/360.0;
 double dT=0; 
 dT=TFromAzZeDeFi(dAz,dZe,dDe,dFi);
 return dT*double(ulMaxValue>>8)*256.0;
}

unsigned long ZeFromDeT(unsigned long pDE, unsigned long pH)   //Зенитное расстояние из DE и H из проверенного XLS
{
 double dZe=0;
 double dFi=Latitude/360.0;
 double dDe=double(pDE>>8)/double(ulMaxValue>>8);
 double dT =double(pH>>8)/double(ulMaxValue>>8);
 dZe=ZeFromDeFiT(dDe,dFi,dT);
 return dZe*double(ulMaxValue>>8)*256.0;
}

unsigned long AzFromDeZeT(unsigned long pDE, unsigned long pZE, unsigned long pH) //Азимут из DE и H
{
  double dAz=0;
  double dFi=Latitude/360.0;
  double dZe=double(pZE>>8)/double(ulMaxValue>>8);
  double dDe=double(pDE>>8)/double(ulMaxValue>>8);
  double dT =double(pH>>8)/double(ulMaxValue>>8);
  dAz=AzFromZeDeFiT(dZe,dDe,dFi,dT);
  return dAz*double(ulMaxValue>>8)*256.0;
}

void setup()
{
  lcd.init();           
  lcd.backlight();
//  pinMode(LIGHT_SENCE_APIN, INPUT); // Вход датчика освещенности
//  pinMode(LCD_LIGHT_PIN, OUTPUT); //Вывод на управление подсветкой LCD
//  SetLCDLight(); //Устанавливаем яркость LCD
  pinMode(ENABLE_XYZ_PIN,  OUTPUT);    // ENABLE XYZ PIN
  digitalWrite(ENABLE_XYZ_PIN, LOW);   // LOW
  pinMode(DX_STEP_PIN,  OUTPUT);       // DX STEP PIN
  digitalWrite(DX_STEP_PIN, LOW);      // LOW
  pinMode(DX_DIR_PIN,  OUTPUT);        // DX DIR PIN
  digitalWrite(DX_DIR_PIN, LOW);       // LOW
  pinMode(DX_FORCE_PIN,  OUTPUT);      // DX FORCE PIN
  digitalWrite(DX_FORCE_PIN, HIGH);    // HIGH
  pinMode(DY_STEP_PIN,  OUTPUT);       // DY STEP PIN
  digitalWrite(DY_STEP_PIN, LOW);      // LOW
  pinMode(DY_DIR_PIN,  OUTPUT);        // DY DIR PIN
  digitalWrite(DY_DIR_PIN, LOW);       // LOW
  pinMode(DY_FORCE_PIN,  OUTPUT);      // DY FORCE PIN
  digitalWrite(DY_FORCE_PIN, HIGH);    // HIGH
  pinMode(DZ_STEP_PIN,  OUTPUT);       // DZ STEP PIN
  digitalWrite(DZ_STEP_PIN, LOW);      // LOW
  pinMode(DZ_DIR_PIN,  OUTPUT);        // DZ DIR PIN
  digitalWrite(DZ_DIR_PIN, LOW);       // LOW
  pinMode(SW_JOY_SENCE, INPUT_PULLUP); // Сенсор кнопки джойстика
  pinMode(X_JOY_SENCE, INPUT);         // Сенсор оси X джойстика
  pinMode(Y_JOY_SENCE, INPUT);         // Сенсор оси Y джойстика
  pinMode(13,  OUTPUT);  // Запасной индикатор
  digitalWrite(13, LOW); // Выключен
  LCDPrint();
  Serial.begin(9600);
  Serial.flush();
  ulLST=ulMaxValue/24*LST();
  ulMilisec=millis();
  bAlignment=false;
  iStDY=iStDY0; 
 }

void loop()
{
 STR=GetString();  // Чтение порта
 action(STR);      // Обработка команд порта
 reaction();       // Обработка команд пульта
 Force_X(false);
 Force_Y(false);
 iPT=iPT-(millis()-ulMilisec)*VTperMS;     //Часовой угол уменьшается
 ulLST=ulLST+(millis()-ulMilisec)*VTperMS; //Звездное время увеличивается
 ulMilisec=millis();
 if (bAlignment) //Если монтировка выровнена
  {
   if (bRun) //Ведение включено: изменяются зимут и зенитное расстояние 
    {
     iToPZE=ZeFromDeT(iPDE, iPT);           //Целевое значение зенитного расстояния
     iToPAZ=AzFromDeZeT(iPDE, iToPZE, iPT); //Целевое значение азимута
     if(abs(iPAZ-iToPAZ)>VRAperSTEP || abs(iPZE-iToPZE)>VDEperSTEP) {ToAZaH(false);}
     if(abs(iPAZ-iToPAZ)>ulMaxValue/360/60/12 || abs(iPZE-iToPZE)>ulMaxValue/360/60/12) bLCD=false;
    }
   else //Ведение выключено: изменяются склонение и прямое восхождение прицела
    {
     iPDE=DeFromZeAz(iPZE,iPAZ);
     iPRA=TFromDeZeAz(iPDE,iPZE,iPAZ)+ulLST-ulMaxValue/2;
    }
  }
 LCDPrint();
}
