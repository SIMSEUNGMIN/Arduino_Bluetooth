unsigned long start = 0;

int reset = 5;
int pio10 = 52;
int pio11 = 53;

int u_sel = 23;
int spi = 22;

char inChar = -1;

boolean mode = true; // 원하는 모드 Server = true, Client = false

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  Serial1.begin(9600);

  setCLE110();
  setMode(mode);

  start = millis();
}

void loop() {
  // put your main code here, to run repeatedly:
  unsigned long cur = millis();

  if(cur - start >= 5000){ //5초가 지난 경우
    if(mode) mode = false;
    else mode = true;
    
    changeMode();

    Serial.print(mode); //확인용

    if(!mode){ //현재 Client 모드일 경우
      Serial1.print("AT+SCAN\r"); //스캔 시작
    }

    start = cur;
  }
  else{ //5초가 지나지 않은 경우
    BLEWrite();
    SerialWrite();
  }
}

void setCLE110(){
  digitalWrite(spi, LOW); // PIO17 Low=PWM, High=firmware SPI
  digitalWrite(u_sel, LOW); // PIO4 Low=off, High=on
  digitalWrite(reset, LOW);
}

void setMode(bool mode){
  if(mode){ //Server, Server -> Client -> Server
    //Server setting
    Serial1.print("AT\r"); //첫 번째 AT Command는 가끔 안 먹히는 경우 발생, 대비책
    Serial1.print("AT+MANUF=SIM\r");
//    Serial1.print("AT+ADVDATA=hello\r");
    Serial1.print("AT+ADVINTERVAL=1000\r");
    delay(100);
    SerialWrite();

    changeMode(); // Client

    //Client setting
    Serial1.print("AT+SCANINTERVAL=500\r");
    delay(100);
    SerialWrite();

    changeMode(); //Server
  }
  else{ //Client, Server -> Client -> delay (Changemode 횟수가 한 번 적기 때문에 시간 조절 필요)
    delay(100);

    //Server setting
    Serial1.print("AT\r"); //첫 번째 AT Command는 가끔 안 먹히는 경우 발생, 대비책
    Serial1.print("AT+MANUF=SIM\r");
//    Serial1.print("AT+ADVDATA=hello\r");
    Serial1.print("AT+ADVINTERVAL=1000\r");
    delay(100);
    SerialWrite();

    changeMode(); // Client

    //Client setting
    Serial1.print("AT+SCANINTERVAL=500\r");
    delay(100);
    SerialWrite();

//    delay(3000); //Server모드와 동작 시간을 맞추기 위한 delay

    Serial1.print("AT+SCAN\r"); //스캔 시작
  }
}

void changeMode(){
   Serial1.print("AT+ROLECHANGE\r");
   Serial1.print("AT+ROLE?\r");
   
   SerialWrite();
}

void BLEWrite(){
  if(Serial.available()){
    inChar = (char)Serial.read();
    Serial1.write(inChar);
    }
}

//Serial.available 체크 한 다음 뒤에 더 쌓일 수 있음
//따라서 Serial.available에서 0이라 while문 빠져나갔는데 
//그 사이에 원래 들어와야 하는 데이터가 또 쌓였을 수 있음
void SerialWrite(){ 
  while(Serial1.available() > 0){
      inChar = (char)Serial1.read();
      if(inChar == 0x0D){
        Serial.println();
        delay(10); //데이터가 들어올 틈
      }
      else{
        Serial.write(inChar);
      }
    }
}
