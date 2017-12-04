//Coding by HJK-.


 
#include<Time.h>
#include<Servo.h>
#include<SoftwareSerial.h>
#include<OneWire.h>

const int rxpin = 2; const int txpin = 3;
SoftwareSerial bluetooth(rxpin,txpin);
//블루투스 송수신핀 설정 및 오브젝트 생성

Servo Servo1; Servo Servo2;
const int servoPin1 = 9; const int servoPin2 = 10;
//서보모터 핀 설정 및 오브젝트 생성
//Servo1=온수, Servo2=냉수 각각 tap에 연결

const int DS18 = 8; //온도센서
OneWire ds(DS18); //온도센서의 오브젝트 생성
const int led1 =12; //수도1 작동확인용 LED
const int led2 =13; //수도2 작동확인용 LED




int state_op = 0;
/* state_op 상태표
 * 0 = 셋업상태
 * 1 = 작업가능 (현재 동작 안함)
 * 2 = 작업시작 (현재 타이머 상태)
 * 3 = 작업중   (현재 동작 상태)
 */

const float hot_w=80; //상수도 더운물 (단위도씨)
const float cold_w=5; //상수도 찬물 (단위도씨)

float t1; //더운물이 틀어질 시간을 계산한 값 (단위ms)
float t2; //찬물이 틀어질 시간을 계산한 값 (단위ms)

float my_temperature=0; //수조의 물이 가지고있는 온도 (단위도씨)
float my_water_level=0; //수조가 가지고있는 부피 (단위%)
float max_level=5800; //수조가 가질수있는 최대 부피 (단위ml)

long timermin=0; //앱에서 받아온 설정타이머값 (단위min)
long timersec=0; //앱에서 받아온 설정타이머값 (단위sec)
long sendtime=0;

float temperature; //앱에서 받아온 설정온도값 (단위도씨)
float water_level; //앱에서 받아온 설정부피값 (단위%)

long startTime=0; //설정타이머의 시작기준 (단위ms)
long duration=0; //타이머의 기간 (단위ms)

long t1time=0; //t1의 계산값의 시작기준 (단위ms)
long t2time=0; //t2의 계산값의 시작기준 (단위ms)
long tduration=0; //수도 작동시간의 기간 (단위ms)



int error=0;
/*앱으로 보내질 정보코드
 * : 0 = 이상무
 * : 1 = 설정 온도 이상 (t1,t2값이 음수로 계산됨)
 * : 2 = 설정 수위 이상 (설정한 수위가 현재 수위보다 낮음)
 * : 3 = 수신오류 (재전송요망)
 * : 4 = 작업완료
 */

//////////////////////////////////////////////////////////////////////////////////////////////
void tapAngle (Servo motor, int angle, int led) {
  Serial.print("+MOVE+");
  Serial.print("angle:");
  Serial.print(angle);
  Serial.println(">>OPERATING");
  if(angle<=180) {
    if(angle>=0) {
      motor.write(angle);
      digitalWrite(led,HIGH); //작동시 ledPin1작동
    }
    else {
      Serial.println("motor-not move-");
    }
  }
  else {
    Serial.println("motor-much angle-");
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////
void setup() {
  Serial.begin(9600);
  bluetooth.begin(9600);
  Servo1.attach(servoPin1);
  Servo2.attach(servoPin2);
  pinMode(led1,OUTPUT);
  pinMode(led2,OUTPUT);
  
  tapAngle(Servo1, 180, led1);
  tapAngle(Servo2, 180, led2);
  //초기 동작시 수도꼭지를 잠긴상태로 만든다.
  my_temperature = getTemp();
  Serial.print("measure current temperature:");Serial.print(my_temperature);Serial.println("'C");
  
  state_op = 1;
  Serial.println("ready");
  //시리얼부의 셋업이 정상적으로 끝났음을 프린트.
}
//////////////////////////////////////////////////////////////////////////////////////////////
void loop() {
  

  
  //앱에서 받아온 계산된 설정타이머값 (단위min+sec)
  delay(100);
  my_temperature=getTemp();
  bluetooth.write(error);
  bluetooth.write(my_temperature);
  bluetooth.write(my_water_level);
  bluetooth.write(state_op);
  bluetooth.write((sendtime/1000)/60);
  bluetooth.write((sendtime/1000)%60);
  byte readop = btread();
  error=0;
  if (my_water_level>=100) {
    tapAngle(Servo1, 180, led1);
    tapAngle(Servo2, 180, led2);
    duration=0;
    timermin=0;
    timersec=0;
    state_op=1;
  }
//==================================================|||||
  
  if (state_op==1) {
    //Serial.println(state_op);
    if (readop=='e') {
      error=3;
    }
    else if (readop=='a') {
      Serial.println("select = wr");
      my_water_level=0;
    }
    else if (readop=='o') {
    //========================||
      Serial.println("select = op");
      
      t2 = calculate_t2();
      t1 = calculate_t1(t2);
      t1 = t1*1000;
      t2 = t2*1000;
        
      Serial.print("t1,t2 st:");Serial.print(t1);Serial.print("/");Serial.print(t2);Serial.print("/");Serial.println(sendtime);
      if((t1<0)|(t2<0)) {
        error=1;
      }
      else if(water_level<my_water_level) {
        error=2;
      }
      else {
        //-->*1
        startTime=millis();
        state_op=2;
      }
    }
    //========================||
  }
//==================================================|||||
  else if (state_op==2) {
    Serial.println(state_op);
    if (readop=='c') {
      tapAngle(Servo1, 180, led1);
      tapAngle(Servo2, 180, led2);
      duration=0;
      startTime=0;
      timermin=0;
      timersec=0;
      sendtime=0;
      state_op=1;
    }
    else {
  //========================||
      duration=millis();
      sendtime = ( ((timermin*60)+timersec)*1000 ) - (duration-startTime);
      if((duration-startTime)>=(((timermin*60)+timersec)*1000)) {
        tapAngle(Servo1, 0, led1);
        t1time=millis();
        tapAngle(Servo2, 0, led2);
        t2time=millis();
        duration=0;
        startTime=0;
        timermin=0;
        timersec=0;
        state_op=3;
        sendtime=0;
      }
      else {
        Serial.print((duration-startTime));
        Serial.print("/");
        Serial.println(((timermin*60)+timersec)*1000);
      }
    }
    
  }
//==================================================|||||
  else if (state_op==3) {
    if (readop=='c') {
      tapAngle(Servo1, 180, led1);
      tapAngle(Servo2, 180, led2);
      duration=0;
      startTime=0;
      timermin=0;
      timersec=0;
      state_op=1;
      sendtime=0;
    }
    else {
    //========================||
      tduration=millis();
      if (t1!=1) {
        if ((tduration-t1time)>=(t1)) {
        tapAngle(Servo1, 180, led1);
        digitalWrite(led1,LOW);
        t1=1;
        }
        else {
          my_water_level=my_water_level+((8.27/max_level)*100);
        }
      }
      if (t2!=1) {
        if ((tduration-t2time)>=(t2)) {
        tapAngle(Servo2, 180, led2);
        digitalWrite(led2,LOW);
        t2=1;
        }
        else {
          my_water_level=my_water_level+((8.27/max_level)*100);
        }
      }
      
      Serial.print("t1 timer:");Serial.print((tduration-t1time));
      Serial.print("/");Serial.print(t1);
      Serial.print("  t2 timer:");Serial.print((tduration-t2time));
      Serial.print("/");Serial.print(t2);Serial.print(" / MWL=");Serial.println(my_water_level);  
        
      if((t1+t2)==2) {
        t1=0;t2=0;
        Serial.println("DOEN");     
        error = 4;
        duration=0;
        startTime=0;
        timermin=0;
        timersec=0;
        state_op=1;
        sendtime=0;
        
        //done
      }
    }
    //========================||
  }
  readop=0;
}
//////////////////////////////////////////////////////////////////////////////////////////////

float calculate_t2() {
  float WL = water_level/100*max_level; //앱에서 설정한 수위(단위%)를 단위ml수위로 바꾼다.
  float MWL = my_water_level/100*max_level; //앱에서 설정한 수위(단위%)를 단위ml수위로 바꾼다.
  float target_temp = ((my_temperature*MWL)-(temperature*WL))/(MWL-WL);
  Serial.print("tt, MWL, t, my_temperature :");
  Serial.print(target_temp);Serial.print(" / ");Serial.print(MWL);Serial.print(" / ");Serial.print(temperature);Serial.print(" / ");Serial.println(my_temperature);
  Serial.print("water_level,WL,my_water_level:");
  Serial.print(water_level);Serial.print(" / ");Serial.print((water_level/100*max_level));Serial.print(" / ");Serial.println(my_water_level);
  float get_t2 = (((hot_w-target_temp)*(WL-MWL))/60.4)/(hot_w-cold_w);
  return get_t2;
}
float calculate_t1(float calculate) {
  float WL = water_level/100*max_level; //앱에서 설정한 수위(단위%)를 단위ml수위로 바꾼다.
  float MWL = my_water_level/100*max_level; //앱에서 설정한 수위(단위%)를 단위ml수위로 바꾼다.
  float get_t1 = ((WL-MWL)/60.4)-t2;
  return get_t1;
}



byte btread() {
  int opstart=0;
  byte point;
  point = bluetooth.read();
  if(point==0) {
    point = bluetooth.read();
    if(point!=255) {
      timermin=(int)point;
      Serial.println(timermin);
      opstart++;
    }
    point = bluetooth.read();
    if(point!=255) {
      timersec=(int)point;
      Serial.println(timersec);
      opstart++;
    }
    point = bluetooth.read();
    if(point!=255) {
      temperature=(int)point;
      Serial.println(temperature);
      opstart++;
    }
    point = bluetooth.read();
    if(point!=255) {
      water_level=(int)point;
      Serial.println(water_level);
      opstart++;
    }
  }
  else if (point==2) {
    Serial.println("signal = water-level is reset");
    return 'a';
  }
  else if (point==3) {
    Serial.println("signal = stop");
    return 'c';
  }
  if (opstart==4) {
    Serial.println("signal = OP");
    return 'o';
  }
  else if ((opstart<4)&(opstart>0)) {
    Serial.println("error");
    return 'e';
  }
  return 0;
}

float getTemp() {
  byte data[12];
  byte addr[8];
  if ( !ds.search(addr)) {
      ds.reset_search();
      return -1000;
  }

  if ( OneWire::crc8( addr, 7) != addr[7]) {
      Serial.println("CRC is not valid!");
      return -1000;
  }
  if ( addr[0] != 0x10 && addr[0] != 0x28) {
      Serial.print("Device is not recognized");
      return -1000;
  }
  ds.reset();
  ds.select(addr);
  ds.write(0x44,1); 

  byte present = ds.reset();
  ds.select(addr);    
  ds.write(0xBE);
  
  for (int i = 0; i < 9; i++) { 
    data[i] = ds.read();
  }
  ds.reset_search();
  
  byte MSB = data[1];
  byte LSB = data[0];
  float tempRead = ((MSB << 8) | LSB); 
  float TemperatureSum = tempRead / 16;
  
  return TemperatureSum; 
}

