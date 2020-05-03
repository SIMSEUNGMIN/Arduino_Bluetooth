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

  setCLE110();
}

void loop() {
   if(Serial.available()){ //블루투스에 쓰기
    inChar = (char)Serial.read();
    Serial1.write(inChar);
   }
  
   while(Serial1.available()){//시리얼 모니터에 쓰기
     inChar = (char)Serial1.read();
     if(inChar == 0x0D){
        Serial.println();
        break;
     }
     else{
        Serial.write(inChar);
     }
   }
}

void setCLE110(){
  digitalWrite(spi, LOW); // PIO17 Low=PWM, High=firmware SPI
  digitalWrite(u_sel, LOW); // PIO4 Low=off, High=on
  digitalWrite(reset, LOW);
}
