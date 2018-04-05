/*
 GOTO7MASTER.h File Written by Igor Ovchinnikov 31/08/2016
*/

boolean bForceIR=false;

struct sDateTime {int SS; int MM; int HH; int DD; int MH; int YY;} sDT;
struct sJDate    {long N; long MN; double H; double MH;} sJD;

int SetRTCPointer()
{
 int i=-1;
 int x=0,y=0;
 boolean bFound=false;
 boolean bExit=false;
 do
 {
  Wire.requestFrom(0x68, 2, false);   
  if(Wire.available()) 
  {
   x = Wire.read();   
   y = Wire.read();
   if (bFound) bExit=true; else i++;
   if (x==136 && y==0) bFound=true;
  }
 } while (!bExit);
 return i;
}

int AskClock(void)
{
  int i=0;
  int c[19];
  SetRTCPointer();
  Wire.requestFrom(0x68, 19, true);    // request 19 bytes from slave device 0x68
  while (Wire.available()&&i<19)       // slave may send less than requested
  {
   c[i] = Wire.read();
   i++;
  }
 sDT.SS=10*(c[0]>>4)+(c[0]&15);
 sDT.MM=10*(c[1]>>4)+(c[1]&15);
 if((c[2]&64)==64) {if((c[2]&32)==32) sDT.HH=12; if((c[2]&16)==16) sDT.HH++;}
 else sDT.HH=10*(c[2]>>4);
 sDT.HH+=(c[2]&15);
 sDT.DD=(c[4]&15)+10*(c[4]>>4);
 sDT.MH=(c[5]&15)+10*(c[5]>>4);
 sDT.YY=(c[6]&15)+10*(c[6]>>4);
 return i;
}

void GJD(void) // Юлианская дата
{
  int A, M;
  long Y;
  AskClock();
  A=int((14.0-sDT.MH)/12.0);
  Y=sDT.YY+2000+4800-A;
  M=sDT.MH+12*A-3;
  sJD.N=sDT.DD+long((153.0*M+2.0)/5.0)+365*Y+Y/4-Y/100+Y/400-32045;
  if((sDT.HH-12)<iZH) sJD.N-=1;
  sJD.H=(sDT.HH+sDT.MM/60.0+sDT.SS/3600.0-iZH)/24.0+0.5;
  if(sJD.H>=1.0) sJD.H-=1.0;
}

void MJD(void) //Модифицированная Юлианская дата // int DAY, int MONTH, int YEAR, double HOUR
{                                                     //JDN - 2400000.5 + HOUR/24
  GJD();
  sJD.MN=sJD.N-2400000;
  sJD.MH=sJD.H-0.5;
  if(sJD.MH<0.0) {sJD.MH+=1.0;}
  if(sJD.MH>=0.5&&sJD.MH<=1.0) {sJD.MN-=1;}
 }

double GST(void)
 {
   double dT, dUT, dGST;
   long lMJD2000;
   MJD();
   lMJD2000=sJD.MN-51545; //Расчет приводится к эпохе 2000
   dUT=sJD.MH*24.0;
   dT= double(lMJD2000)/36525.0;
   dGST=6.697374558+1.0027379093*dUT+(8640184.812866+(0.093104-(6.2E-6)*dT)*dT)*dT/3600.0;
   dGST=dGST-int(dGST/24.0)*24.0;
   return dGST;
 }

double LST(void)
 {
  double dLST;
  dLST=GST();
  dLST=dLST-Longitude/15.0;
  if(dLST<0.0) dLST+=24.0;
  return dLST;
 }

// Функция AskJoy() возвращает при ее вызове следующие значения:

//     0 - когда ничего не надо делать
//   256 - когда надо сделать микрошаг вперед по оси Х
//   512 - когда надо сделать полныйшаг вперед по оси Х
//  1024 - когда надо сделать микрошаг назад по оси Х
//  2048 - когда надо сделать полныйшаг назад по оси Х
//  4096 - когда надо сделать микрошаг вверх по оси У
//  8192 - когда надо сделать полныйшаг вверх по оси У
// 16384 - когда надо сделать микрошаг вниз по оси У
// 32768 - когда надо сделать полныйшаг вниз по оси У
// 65536 - включить/отключить трекинг
// Используются суммы указанных значений

long AskJOY()
{
  int iA1=0, iA2=0, iA3=0;
  long iRetValue=0;

  iA1 = analogRead(X_JOY_SENCE);
  iA2 = analogRead(Y_JOY_SENCE);
  iA3 = analogRead(SW_JOY_SENCE);
    
  if(iA1<25)                { iRetValue=iRetValue | 512;  } // Полный шаг X+
  if(iA1>=25 && iA1 < 490)  { iRetValue=iRetValue | 256;  } // Микрошаг X+
  if(iA1>520 && iA1<=1000)  { iRetValue=iRetValue | 1024; } // Микрошаг X-
  if(iA1>1000)              { iRetValue=iRetValue | 2048; } // Полный шаг X-
  
  if(iA2<25)                { iRetValue=iRetValue | 8192;  } // Полный шаг Y+
  if(iA2>=25  && iA2 < 490) { iRetValue=iRetValue | 4096;  } // Микрошаг Y+
  if(iA2>520  && iA2<=1000) { iRetValue=iRetValue | 16384; } // Микрошаг Y-
  if(iA2>1000)              { iRetValue=iRetValue | 32768; } // Полный шаг Y-
  
  if(iA3<500) {iRetValue=65536; delay(250);}    // Включить/отключить трекинг
  
  return iRetValue;
}

// Функция AskSlaveI2C() возвращает те же значения, что и AskJoy() для стрелок,
// кроме этого, значения цифровых кнопок 1-10
// кроме этого, '*' (11) '#' (12) 'Ok' (15)


long AskSlaveI2C()
{
  long iRetValue=0;
  Wire.requestFrom(10,1); // Считать 1 байт с адреса 10
  if (Wire.available())   // slave may send less than requested
  {
    iRetValue = Wire.read(); // Полученный байт
  }
 switch (iRetValue)
 {
  case  1: {iRetValue= 1; break;} //
  case  2: {iRetValue= 2; break;} //
  case  3: {iRetValue= 3; break;} //
  case  4: {iRetValue= 4; break;} //
  case  5: {iRetValue= 5; break;} //
  case  6: {iRetValue= 6; break;} //
  case  7: {iRetValue= 7; break;} //
  case  8: {iRetValue= 8; break;} //
  case  9: {iRetValue= 9; break;} //
  case 10: {iRetValue=10; break;} // 0
  case 11: {iRetValue=11; break;} // *
  case 12: {iRetValue=12; break;} // #
  case 13: {if(bForceIR) iRetValue= 8192; else iRetValue= 4096; break;} // Вверх
  case 14: {if(bForceIR) iRetValue= 2048; else iRetValue= 1024; break;} // Влево
  case 15: {iRetValue=65536; break;} // Ok
  case 16: {if(bForceIR) iRetValue=  512; else iRetValue=  256; break;} // Вправо
  case 17: {if(bForceIR) iRetValue=32768; else iRetValue=16384; break;} // Вниз
  default: iRetValue=0; // Ничего не надо делать
 }
 return iRetValue; 
}

int Force_X(boolean bForce)
{
 int iXSX=0; 
 if(!bForceX && bForce) //Включаем полношаговый режим
 {
   iXSX = 1; //Кратность шага драйвера X
   digitalWrite(DX_FORCE_PIN, LOW);
   imStepsXPS = iStepsXPS*iXSX; //Шагов в секунду на двигателе X
   ulSPX = iStepsDX*dRDX*iXSX; //Шагов двигателя X на полный оборот оси прямого восхождения
   VRAperSTEP=ulMaxValue/ulSPX;      //Единиц прямого восхождения на 1 шаг двигателя
   dVRAperSTEP=ulMaxValue/StarMSPS*1000/imStepsXPS; //Поправка вращения Земли на 1 шаг ДПВ
   bForceX=true;
 }
 if(bForceX && !bForce) //Включаем микрошаговый режим
 {
   iXSX = iXStepX; //Кратность шага драйвера X
   digitalWrite(DX_FORCE_PIN, HIGH);
   imStepsXPS = 500; //Микрошагов в секунду на двигателе X
   ulSPX = iStepsDX*dRDX*iXSX; //Микрошагов двигателя X на полный оборот оси прямого восхождения
   VRAperSTEP=ulMaxValue/ulSPX;      //Единиц прямого восхождения на 1 шаг двигателя
   dVRAperSTEP=ulMaxValue/StarMSPS*1000/imStepsXPS; //Поправка вращения Земли на 1 шаг ДПВ
   bForceX=false;
  }
}

int Force_Y(boolean bForce)
{
  int iYSX=0;
  if(!bForceY && bForce) //Включаем полношаговый режим
  {
    iYSX = 1; //Кратность шага драйвера Y
    digitalWrite(DY_FORCE_PIN, LOW);
    imStepsYPS = iStepsYPS*iYSX; //Шагов в секунду на двигателе Y
    ulSPY = iStepsDY*dRDY*iYSX; //Шагов двигателя Y на полный оборот оси склонений
    VDEperSTEP=ulMaxValue/ulSPY;      //Единиц склонения на 1 шаг двигателя
    dVDEperSTEP=0;               //Поправка (доворот) ДСК в единицах СК на 1 шаг ДСК
    bForceY=true;
   }
  if(bForceY && !bForce) //Включаем микрошаговый режим
  {
    iYSX = iYStepX; //Кратность шага драйвера Y
    digitalWrite(DY_FORCE_PIN, HIGH);
    imStepsYPS = 500; //Микрошагов в секунду на двигателе Y
    ulSPY = iStepsDY*dRDY*iYSX; //Микрошагов двигателя Y на полный оборот оси склонений
    VDEperSTEP=ulMaxValue/ulSPY;      //Единиц склонения на 1 шаг двигателя
    dVDEperSTEP=7;               //Поправка (доворот) ДСК в единицах СК на 1 шаг ДСК
    bForceY=false;
  }
}

String GetString ()
{
  String STR="";
  char c;
  if (!Serial.available() && ((millis()-iPortTimer) >= 1000)) {iPortTimer=millis(); STR="e"; return STR;}
  while (Serial.available())  //если есть что читать;
  {
    c = Serial.read();       //читаем символ
    if (c!='\n' && c!='\r' ) //если это не спецсимвол прибавляем к строке
    STR += c;
   delay(1);
  }
  return STR;
}

int GetSubStr ()
{
  int i;
  STR1="", STR2="";
  i=STR.indexOf(',');
  if (i>0) {STR1=STR.substring(1,i); STR2=STR.substring(i+1);}
  if (i<0 && STR.length()>1) STR1=STR.substring(1);
}

void HexRaToString(unsigned long ulRaVal, unsigned long ulMaxRaVal)
{
  double udRa=0;
  int iRaH=0, iRaM=0, iRaS=0;
  udRa=double(ulRaVal>>8)/double(ulMaxRaVal>>8);
  iRaH=udRa*24;
  udRa=udRa-double(iRaH)/24;
  iRaM=udRa*24*60;
  udRa=udRa-double(iRaM)/24/60;
  iRaS=round(udRa*24*60*60);
  LCDString1=String("Ra="+String(iRaH)+"h"+String(iRaM)+"m"+String(iRaS)+"s");
  switch (iStDX)
  {
  case -1: {LCDString1=LCDString1+" S"; break;}
  case  1: {LCDString1=LCDString1+" N"; break;}
  }
}

void HexDeToString(unsigned long ulDeVal, unsigned long ulMaxDeVal)
{
  double udDe=0;
  int iDeG=0, iDeM=0, iDeS=0;
  udDe=double(ulDeVal>>8)/double(ulMaxDeVal>>8);
  if(udDe>0.5){udDe=udDe-1;}
  iDeG=udDe*360;
  udDe=udDe-double(iDeG)/360;
  iDeM=udDe*360*60;
  udDe=udDe-double(iDeM)/360/60;
  iDeM=abs(iDeM);
  iDeS=round(abs(udDe*360*60*60));
  LCDString2=String("De="+String(iDeG)+"*"+String(iDeM)+"m"+String(iDeS)+"s");
  switch (iStDY)
  {
  case -1: {LCDString2=LCDString2+" N/E"; break;}
  case  1: {LCDString2=LCDString2+" N/W"; break;}
  }
}

void HexAzToString(unsigned long ulAzVal, unsigned long ulMaxAzVal)
{
  double udDe=0;
  int iDeG=0, iDeM=0, iDeS=0;
  udDe=double(ulAzVal>>8)/double(ulMaxAzVal>>8);
  iDeG=udDe*360;
  udDe=udDe-double(iDeG)/360;
  iDeM=udDe*360*60;
  udDe=udDe-double(iDeM)/360/60;
  iDeM=abs(iDeM);
  iDeS=round(abs(udDe*360*60*60));
  LCDString1=String("Az="+String(iDeG)+"*"+String(iDeM)+"m"+String(iDeS)+"s");
  switch (iStDX)
  {
  case -1: {LCDString1=LCDString1+" N"; break;}
  case  1: {LCDString1=LCDString1+" S"; break;}
  }
}

void HexZeToString(unsigned long ulZeVal, unsigned long ulMaxZeVal)
{
  double udDe=0;
  int iDeG=0, iDeM=0, iDeS=0;
  udDe=0.25-double(ulZeVal>>8)/double(ulMaxZeVal>>8);
  iDeG=udDe*360;
  udDe=udDe-double(iDeG)/360;
  iDeM=udDe*360*60;
  udDe=udDe-double(iDeM)/360/60;
  iDeM=abs(iDeM);
  iDeS=round(abs(udDe*360*60*60));
  LCDString2=String("H="+String(iDeG)+"*"+String(iDeM)+"m"+String(iDeS)+"s");
  switch (iStDY)
  {
  case -1: {LCDString2=LCDString2+" N/W"; break;}
  case  1: {LCDString2=LCDString2+" N/E"; break;}
  }
}

//void SetLCDLight(void)
//{
// analogWrite(LCD_LIGHT_PIN,512-analogRead(LIGHT_SENCE_APIN)/4);
//}

int LCDPrintString (String str, int row, int kol)
{
  int i=0;
  while (i<16 && i<str.length())
  {
    lcd.setCursor(kol-1+i, row-1);
    lcd.print(str.charAt(i));
    i++;
  }
}

int LCDPrintSTR (char* str, int row, int kol)
{
  int i=0;
  while (i<16 && str[i]!='\0')
  {
    lcd.setCursor(kol-1+i, row-1);
    lcd.print(str[i]);
    i++;
  }
}

void LCDPrint()
 {
  if(!bLCD) // Азимутальный режим
  {
   if(bAlignment)
   {
    HexAzToString(iPAZ, ulMaxValue);
    HexZeToString(iPZE, ulMaxValue);
   }
   else
   {
    if (iStDY==-iStDY0) LCDString1="   N/E or S/W   "; //Телескоп слева от вертикальной оси
    if (iStDY== iStDY0) LCDString1="   N/W or S/E   "; //Телескоп справа от вертикальной оси
    if (iStDY== 0) LCDString1=" Azduino GOTO7  "; //Телескоп не сориентирован по склонению
    if (iStDX== 0) LCDString1+=" RAERR"; //Не задано направление ведения телескопа
    if (bRun){LCDString2=" TRACKING"; if(iStDX>0) LCDString2+=" NORTH"; if(iStDX<0) LCDString2+=" SOUTH";}
    else      LCDString2="Fixed to Azimuth";
   }
   lcd.clear();
//  SetLCDLight();
   LCDPrintString(LCDString1,1,1);
   LCDPrintString(LCDString2,2,1);
   bLCD=true;
  }
 }

void LCDCOR (int pKey)
{
 if(true)
 {
  if(pKey>=256 && !bAlignment) LCDPrintSTR (" Correction ", 2, 1);
  if(pKey== 1) LCDPrintSTR (" -v- FOCUS -vv- ", 2, 1);
  if(pKey== 3) LCDPrintSTR (" +^+ FOCUS +^^+ ", 2, 1);
  if(pKey== 8) LCDPrintSTR ("      TIME      ", 1, 1);
  if(pKey==10 &&  bAlignment) {LCDPrintSTR ("   Alighment:   ", 1, 1); LCDPrintSTR ("  - canceled -  ", 2, 1);}
  if(pKey==10 && !bAlignment) {LCDPrintSTR ("   Alighment:   ", 1, 1); LCDPrintSTR ("Use 1-3 or <^v> ", 2, 1);}
  switch (pKey)
  {
   case   256: if(iStDX!=0) LCDPrintSTR ("  > ", 2, 13); else LCDPrintSTR ("N/S Position ERR", 2, 1); break;
   case   512: if(iStDX!=0) LCDPrintSTR ("  >>", 2, 13); else LCDPrintSTR ("N/S Position ERR", 2, 1); break; 
   case  1024: if(iStDX!=0) LCDPrintSTR (" <  ", 2, 13); else LCDPrintSTR ("N/S Position ERR", 2, 1); break;
   case  2048: if(iStDX!=0) LCDPrintSTR ("<<  ", 2, 13); else LCDPrintSTR ("N/S Position ERR", 2, 1); break;
   case  4096: if(iStDY!=0) LCDPrintSTR ("  ^ ", 2, 13); else LCDPrintSTR ("E/W Position ERR", 2, 1); break;
   case  4352: if(iStDX!=0) LCDPrintSTR (" ^> ", 2, 13); else LCDPrintSTR ("N/S Position ERR", 2, 1); break;    
   case  4608: if(iStDY!=0) LCDPrintSTR (" ^>>", 2, 13); else LCDPrintSTR ("E/W Position ERR", 2, 1); break;
   case  5120: if(iStDX!=0) LCDPrintSTR (" <^ ", 2, 13); else LCDPrintSTR ("N/S Position ERR", 2, 1); break; 
   case  6144: if(iStDY!=0) LCDPrintSTR ("<<^ ", 2, 13); else LCDPrintSTR ("E/W Position ERR", 2, 1); break;
   case  8192: if(iStDY!=0) LCDPrintSTR (" ^^ ", 2, 13); else LCDPrintSTR ("E/W Position ERR", 2, 1); break;
   case  8448: if(iStDY!=0) LCDPrintSTR (" ^^>", 2, 13); else LCDPrintSTR ("E/W Position ERR", 2, 1); break;
   case  8704: if(iStDY!=0) LCDPrintSTR ("^^>>", 2, 13); else LCDPrintSTR ("E/W Position ERR", 2, 1); break;
   case  9216: if(iStDY!=0) LCDPrintSTR ("<^^ ", 2, 13); else LCDPrintSTR ("E/W Position ERR", 2, 1); break;
   case 10240: if(iStDX!=0) LCDPrintSTR ("<<^^", 2, 13); else LCDPrintSTR ("N/S Position ERR", 2, 1); break;   
   case 16384: if(iStDY!=0) LCDPrintSTR ("  v ", 2, 13); else LCDPrintSTR ("E/W Position ERR", 2, 1); break;
   case 16640: if(iStDY!=0) LCDPrintSTR (" v> ", 2, 13); else LCDPrintSTR ("E/W Position ERR", 2, 1); break;
   case 16896: if(iStDY!=0) LCDPrintSTR (" v>>", 2, 13); else LCDPrintSTR ("E/W Position ERR", 2, 1); break;
   case 17408: if(iStDY!=0) LCDPrintSTR (" <v ", 2, 13); else LCDPrintSTR ("E/W Position ERR", 2, 1); break;
   case 18432: if(iStDY!=0) LCDPrintSTR ("<<v ", 2, 13); else LCDPrintSTR ("E/W Position ERR", 2, 1); break;
   case 33024: if(iStDY!=0) LCDPrintSTR (" vv>", 2, 13); else LCDPrintSTR ("E/W Position ERR", 2, 1); break;
   case 33280: if(iStDY!=0) LCDPrintSTR ("vv>>", 2, 13); else LCDPrintSTR ("E/W Position ERR", 2, 1); break;
   case 32768: if(iStDY!=0) LCDPrintSTR (" vv ", 2, 13); else LCDPrintSTR ("E/W Position ERR", 2, 1); break;
   case 33792: if(iStDY!=0) LCDPrintSTR ("<vv ", 2, 13); else LCDPrintSTR ("E/W Position ERR", 2, 1); break;
   case 34816: if(iStDY!=0) LCDPrintSTR ("<<vv", 2, 13); else LCDPrintSTR ("E/W Position ERR", 2, 1); break;
  };
 }
}


double TFromAzZeDeFi(double pAz, double pZe, double pDe, double pFi) //Часовой угол
{
  double dh  = (0.25-pZe)*2*PI;
  double dDe = pDe*2*PI;
  double dFi = pFi*2*PI;
  double dT  = 0;
  dT=acos((sin(dh)-sin(dFi)*sin(dDe))/(cos(dFi)*cos(dDe)));
  dT=dT/2.0/PI;
  if(pAz>=0.0 && pAz<0.5) dT+=0.5;
  if(pAz>=0.5 && pAz<=1) dT=0.5-dT;
  return dT;
};

double DeFromZeAzFi(double pZe, double pAz, double pFi)   //Склонение из высоты, азимута и широты
{
  double dh  = (0.25-pZe)*2*PI;
  double dAz = pAz*2*PI;
  double dFi = pFi*2*PI;
  double dDe = 0;
  dDe = asin(sin(dh)*sin(dFi)+cos(dh)*cos(dFi)*cos(dAz));
  return dDe/2.0/PI;
};

double ZeFromDeFiT(double pDe, double pFi, double pT)   //Зенитное расстояние из DE и Az
{
 double dZe = 0;
 double dDe = pDe*2*PI;
 double dFi = pFi*2*PI;
 double dT  = pT*2*PI;
 dZe=acos(sin(dDe)*sin(dFi)-cos(dDe)*cos(dT)*cos(dFi));
 return dZe/2.0/PI;
}

double AzFromZeDeFiT(double pZe, double pDe, double pFi, double pT) //Азимут из склонения и часового угла
{
 double dAz = 0;
 double dZe = pZe*2*PI;
 double dDe = pDe*2*PI;
 double dFi = pFi*2*PI;
 double dT  = pT*2*PI;
 dAz=sin(dDe)*cos(dFi)+cos(dDe)*cos(dT)*sin(dFi);
 dAz=acos(dAz/sin(dZe));
 if(pT<0.5) dAz=2*PI-dAz;
 return dAz/2/PI;
}

unsigned long StrToHEX (String STR)
{
  int  i;
  char c;
  unsigned long ulVal=0;
  for (i=0; i<STR.length(); i++)
  {
   ulVal=ulVal*16;
   c=STR.charAt(i);
   switch (c) 
    {
      case 'f': ;
      case 'F': ulVal++;
      case 'e': ;
      case 'E': ulVal++;
      case 'd': ;
      case 'D': ulVal++;
      case 'c': ;
      case 'C': ulVal++;
      case 'b': ;
      case 'B': ulVal++;
      case 'a': ;
      case 'A': ulVal++;
      case '9': ulVal++;
      case '8': ulVal++;
      case '7': ulVal++;
      case '6': ulVal++;
      case '5': ulVal++;
      case '4': ulVal++;
      case '3': ulVal++;
      case '2': ulVal++;
      case '1': ulVal++;
    };
  };
 return ulVal;
}

String HexTo8D (unsigned long Hex)
{
  String STR0="";
  char c = '0';
  if (Hex<0x10000000) STR0 += c;
  if (Hex<0x1000000)  STR0 += c;
  if (Hex<0x100000)   STR0 += c;
  if (Hex<0x10000)    STR0 += c;
  if (Hex<0x1000)     STR0 += c;
  if (Hex<0x100)      STR0 += c;
  if (Hex<0x10)       STR0 += c;
  return STR0;
};

String HexTo4D (unsigned int Hex)
{
  String STR0="";
  char c = '0';
  if (Hex<0x1000)      STR0 += c;
  if (Hex<0x100)       STR0 += c;
  if (Hex<0x10)        STR0 += c;
  return STR0;
};

long Stepper_step(long ipSteps, unsigned uStepPin, unsigned uDirPin, unsigned uStepsPS)
{
 long iSteps=ipSteps, lRetVal=0;
 if((uStepPin>53)||(uDirPin>53)) return lRetVal;
 
 if(iSteps > 0) digitalWrite(uDirPin,  LOW);
 if(iSteps < 0) digitalWrite(uDirPin,  HIGH);
 iSteps=abs(iSteps);

 while (iSteps>0)
 {
  digitalWrite(uStepPin,  HIGH);
  delay(1000/uStepsPS);
  delayMicroseconds(1000*(1000%uStepsPS));
  digitalWrite(uStepPin,  LOW);
  iSteps--;
  if (ipSteps>0) lRetVal++; else lRetVal--;
 }
 return lRetVal;
}

void Stepper_X_step(int ipSteps)
{
  Stepper_step(ipSteps, DX_STEP_PIN, DX_DIR_PIN, imStepsXPS);
}

void Stepper_Y_step(int ipSteps)
{
  Stepper_step(ipSteps, DY_STEP_PIN, DY_DIR_PIN, imStepsYPS);
}

void Stepper_Z_step(int ipSteps)
{
  Stepper_step(ipSteps, DZ_STEP_PIN, DZ_DIR_PIN, imStepsZPS);
}

int SetDateTime(void)
{
 SetRTCPointer(); 
 sDT.SS=(sDT.SS/10<<4)+sDT.SS%10;
 sDT.MM=(sDT.MM/10<<4)+sDT.MM%10;
 sDT.HH=(sDT.HH/10<<4)+sDT.HH%10;
 sDT.DD=(sDT.DD/10<<4)+sDT.DD%10;
 sDT.MH=(sDT.MH/10<<4)+sDT.MH%10;
 sDT.YY=(sDT.YY/10<<4)+sDT.YY%10;
 Wire.beginTransmission(0x68); 
 Wire.write(0);
 Wire.write(sDT.SS);
 Wire.write(sDT.MM); 
 Wire.write(sDT.HH); 
 Wire.write(0); //iDay
 Wire.write(sDT.DD); 
 Wire.write(sDT.MH); 
 Wire.write(sDT.YY); 
 Wire.endTransmission();
}

void SetValue (int pVal, int pPoz)
{
 int iFirst=0, iLast=0; 
 if(pPoz==1 && pVal>=0 && pVal<=2)
 {
  iFirst=pVal;
  iLast=sDT.HH%10; 
  sDT.HH=(iFirst*10)+iLast;
 }
 if(pPoz==2 && pVal>=0 && pVal<=9)
 {
  iFirst=sDT.HH/10;
  iLast=sDT.HH%10; 
  if(iFirst<2 || (iFirst==2 && pVal<=3)) iLast=pVal;
  sDT.HH=(iFirst*10)+iLast;
 }
 if(pPoz==3 && pVal>=0 && pVal<=5)
 {
  iFirst=pVal;
  iLast=sDT.MM%10; 
  sDT.MM=(iFirst*10)+iLast;
 }
 if(pPoz==4 && pVal>=0 && pVal<=9)
 {
  iFirst=sDT.MM/10;
  iLast=pVal; 
  sDT.MM=(iFirst*10)+iLast;
 }
 if(pPoz==5 && pVal>=0 && pVal<=5)
 {
  iFirst=pVal;
  iLast=sDT.SS%10; 
  sDT.SS=(iFirst*10)+iLast;
 }
 if(pPoz==6 && pVal>=0 && pVal<=9)
 {
  iFirst=sDT.SS/10;
  iLast=pVal; 
  sDT.SS=(iFirst*10)+iLast;
 }
 if(pPoz==7 && pVal>=0 && pVal<=3)
 {
  iFirst=pVal;
  iLast=sDT.DD%10; 
  sDT.DD=(iFirst*10)+iLast;
 }
 if(pPoz==8 && pVal>=0 && pVal<=9)
 {
  iFirst=sDT.DD/10;
  iLast=sDT.DD%10; 
  if(iFirst<3 || (iFirst==3 && pVal<=1)) iLast=pVal;
  if(iFirst==0 && pVal<1) iLast=1;
  sDT.DD=(iFirst*10)+iLast;
 }
 if(pPoz==9 && pVal>=0 && pVal<=1)
 {
  iFirst=pVal;
  iLast=sDT.MH%10; 
  sDT.MH=(iFirst*10)+iLast;
 }
 if(pPoz==10 && pVal>=0 && pVal<=9)
 {
  iFirst=sDT.MH/10;
  iLast=sDT.MH%10; 
  if(iFirst<1 || (iFirst==1 && pVal<=2)) iLast=pVal;
  if(iFirst==0 && pVal<1) iLast=1;
  sDT.MH=(iFirst*10)+iLast;
 }
 if(pPoz==11 && pVal>=0 && pVal<=5)
 {
  iFirst=pVal;
  iLast=sDT.YY%10; 
  sDT.YY=(iFirst*10)+iLast;
 }
 if(pPoz==12 && pVal>=0 && pVal<=9)
 {
  iFirst=sDT.YY/10;
  iLast=pVal; 
  sDT.YY=(iFirst*10)+iLast;
 }
}

void LCDTimePrint()
 {
  LCDString1="";
  LCDString2="";
  if(sDT.HH<10) LCDString1="0";
  LCDString1+=String(sDT.HH); // Часов;
  LCDString1+=":"; 
  if(sDT.MM<10) LCDString1+="0";
  LCDString1+=String(sDT.MM); // Минут
  LCDString1+=":"; 
  if(sDT.SS<10) LCDString1+="0";
  LCDString1+=String(sDT.SS); // Секунд
  if(sDT.DD<10) LCDString2="0";
  LCDString2+=String(sDT.DD); // Часов;
  LCDString2+="/"; 
  if(sDT.MH<10) LCDString2+="0";
  LCDString2+=String(sDT.MH); // Минут
  LCDString2+="/";
  LCDString2+="20";
  if(sDT.YY<10) LCDString2+="0";
  LCDString2+=String(sDT.YY); // Секунд
  lcd.clear();
  LCDPrintString(LCDString1,1,5);
  LCDPrintString(LCDString2,2,4);
//  bLCD=true;
 }

void GetTime()
{
 int iPoz=1;
 long lAskI2C=0; 
 boolean bCH=true;
 AskClock();
 lcd.setCursor(4,0);
 lcd.blink();
 do
 {
  lAskI2C=AskSlaveI2C();
  switch (lAskI2C)
  {
   case  1: {SetValue( 1, iPoz); iPoz++; bCH=true; break;} // 
   case  2: {SetValue( 2, iPoz); iPoz++; bCH=true; break;} // 
   case  3: {SetValue( 3, iPoz); iPoz++; bCH=true; break;} // 
   case  4: {SetValue( 4, iPoz); iPoz++; bCH=true; break;} // 
   case  5: {SetValue( 5, iPoz); iPoz++; bCH=true; break;} // 
   case  6: {SetValue( 6, iPoz); iPoz++; bCH=true; break;} // 
   case  7: {SetValue( 7, iPoz); iPoz++; bCH=true; break;} // 
   case  8: {SetValue( 8, iPoz); iPoz++; bCH=true; break;} // 
   case  9: {SetValue( 9, iPoz); iPoz++; bCH=true; break;} // 
   case 10: {SetValue( 0, iPoz); iPoz++; bCH=true; break;} // 
   case  4096: {iPoz-=6; break;} // Вверх
//   case  8192: 
   case  1024: {iPoz--;  break;} // Влево
//   case  2048: 
   case   256: {iPoz++;  break;} // Вправо
//   case   512: 
   case 16384: {iPoz+=6; break;} // Вниз
//   case 32768:
  }
  if (bCH) {LCDTimePrint(); bCH=false;}
  if(iPoz< 1) iPoz=12;
  if(iPoz>12) iPoz= 1;
  switch (iPoz)
  {
   case  1: {lcd.setCursor(4, 0); break;} // 
   case  2: {lcd.setCursor(5, 0); break;} // 
   case  3: {lcd.setCursor(7, 0); break;} // 
   case  4: {lcd.setCursor(8, 0); break;} // 
   case  5: {lcd.setCursor(10,0); break;} // 
   case  6: {lcd.setCursor(11,0); break;} // 
   case  7: {lcd.setCursor(3, 1); break;} // 
   case  8: {lcd.setCursor(4, 1); break;} // 
   case  9: {lcd.setCursor(6, 1); break;} // 
   case 10: {lcd.setCursor(7, 1); break;} // 
   case 11: {lcd.setCursor(11,1); break;} //
   case 12: {lcd.setCursor(12,1); break;} //
  }
  delay(250);  
 } while((lAskI2C!=65536)&&(lAskI2C!=11)&&(lAskI2C!=12));
 if (lAskI2C==65536) SetDateTime();
 lcd.init();           
}



