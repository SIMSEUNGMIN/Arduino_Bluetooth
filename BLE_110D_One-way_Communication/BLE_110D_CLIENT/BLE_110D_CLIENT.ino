const long interval = 20;

int reset = 5;
int pio10 = 52;
int pio11 = 53;

int u_sel = 23;
int spi = 22;

char inChar = -1;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  Serial1.begin(9600);

  Serial.println("START");

  setCLE110();
  
  delay(1000);
}

void loop() {
  startScan();

  while(Serial.available()){
   inChar = (char)Serial.read();
   Serial1.write(inChar);
  }

  unsigned long cur = millis();

  while(true){ // 5초동안 시리얼 버퍼에 있는 데이터 출력
    if(millis() - cur <= 5000){
    while(Serial1.available()){
      inChar = (char)Serial1.read();
      if(inChar == 0x0D){
        Serial.println();
      }
      else Serial.write(inChar);
    }
   }
   else break;
  }

  delay(500);
}

void setCLE110(){
  bool mode = false; // true = Server, false = Client
  delay(100);
  
  digitalWrite(spi, LOW); // PIO17 Low=PWM, High=firmware SPI
  digitalWrite(u_sel, LOW); // PIO4 Low=off, High=on

  digitalWrite(reset, LOW);
  delay(100);
  
  ATCommand("AT\r");
  ATCommand("AT+ROLE?\r");

  if(mode){
    setServer(true); //true = B, false = P
  }
  else{
    setRoleChange();
    setClient(false); //true = O, false = C
  }
  
  ATCommand("AT+INFO?\r");
  ATCommand("AT+ROLE?\r"); 
}

void setServer(bool mode){ //수정
//  if(mode){
//    ATCommand("AT+SERVER=B\r");  
//  }
//  else{
//    ATCommand("AT+SERVER=P\r");
//  }
//  delay(5);
//
//  ATCommand("AT+MANUF=SIM\r");
//  ATCommand("AT+ADVINTERVAL=1000\r");
}

void setClient(bool mode){
  if(mode){
    ATCommand("AT+CLIENT=O\r");
  }
  else{
    ATCommand("AT+CLIENT=C\r");
  }
  delay(5);
}

//the function that checks the role and if you need to change role,
//insert AT+ROLECHANGE 

void setRoleChange(){ //모드 변경
  Serial1.print("AT+ROLECHANGE\r");

  unsigned long cur = millis();

  while(true){ // 3초 대기 후에 다시 동작
    if(millis() - cur >= 3000){ 
    while(Serial1.available()){
      inChar = (char)Serial1.read();
      if(inChar == 0x0D){
        Serial.println(); break;
      }
      else Serial.write(inChar);
    }
    break;
   }
  } 
}

void startScan(){ //스캔 시작 명령
  ATCommand("AT+SCAN=5\r");
}

void ATCommand(String input){ //AT커맨드 전송시 20ms 대기

  Serial1.print(input);
  
  unsigned long cur = millis();

  while(true){
    if(millis() - cur >= interval){
    while(Serial1.available()){
      inChar = (char)Serial1.read();
      if(inChar == 0x0D){
        Serial.println(); break;
      }
      else Serial.write(inChar);
    }
    break;
   }
  }
}
