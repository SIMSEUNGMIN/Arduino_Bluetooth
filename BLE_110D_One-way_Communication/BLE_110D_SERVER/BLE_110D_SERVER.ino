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
  // put your main code here, to run repeatedly: 

  while(Serial.available()){
    inChar = (char)Serial.read();
    Serial1.write(inChar);
  }
  
  while(Serial1.available()){
    inChar = (char)Serial1.read();
    if(inChar == 0x0D){
      Serial.println();
      break;
    }
    else{
      Serial.write(inChar);
    }
  }

  delay(10);
}

void setCLE110(){
  bool mode = true; // true = Server, false = Client
  delay(10);
  
  digitalWrite(spi, LOW); // PIO17 Low=PWM, High=firmware SPI
  digitalWrite(u_sel, LOW); // PIO4 Low=off, High=on
  
  digitalWrite(reset, LOW);
  delay(100);

  ATCommand("AT\r");
  ATCommand("AT+ROLE?\r");

  if(mode){
    setServer(false); //true = B, false = P
  }
  else{
    setClient(true); //true = O, false = C
  }

  ATCommand("AT+INFO?\r");
  ATCommand("AT+ROLE?\r");
 
}

void setServer(bool mode){
  if(mode){
    ATCommand("AT+SERVER=B\r");  
  }
  else{
    ATCommand("AT+SERVER=P\r");
  }
  delay(5);

//모드 내부의 세부사항 세팅
  ATCommand("AT+MANUF=SIM\r");
  ATCommand("AT+ADVINTERVAL=1000\r");
  ATCommand("AT+COMMAND\r");
  ATCommand("AT+ADVDATA=hello\r");
  ATCommand("AT+ADVDATA?\r");
}

void setClient(bool mode){
//  if(mode){
//    Serial1.print("AT+ROLECHANGE\r");
//    delay(2000);
//    Serial1.print("AT+CLIENT=O\r");
//  }
//  else{
//    ATCommaned("AT+CLIENT=C\r");
//  }
//  delay(5);
//
//  ATCommand("AT+SCANINTERVAL=500\r");
//  ATCommand("AT+SCANINTERVAL?\r");
//  ATCommand("AT+SCAN=1\r");
//  ATCommand("AT+SCAN?")
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
