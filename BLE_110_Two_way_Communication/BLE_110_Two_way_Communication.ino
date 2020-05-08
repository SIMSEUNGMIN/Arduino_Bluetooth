int reset = 5;
int pio10 = 52;
int pio11 = 53;

int u_sel = 23;
int spi = 22;

bool curMode = true; //현재 모드 Server = true, Client = false
bool mode = false; // 시작하길 원하는 모드 Server = true, Client = false

String scannedData[10] = ""; //스캔 저장된 데이터
int count = 0; //데이터가 저장될 칸(데이터 개수)

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  Serial1.begin(9600);
  
  setCLE110();
  setMode(mode); //처음 시작시 설정되어있는 모드 확인
  setModeSetting(mode); //원하는 모드의 세부 세팅
  delay(1000);
}

void loop() {
  // put your main code here, to run repeatedly:

  if(!curMode){ //현재 모드가 Client일 경우
    //스캔 시작 및 데이터(ADDR주소랑 NAME이 반드시 존재하는 데이터) 저장
    //스캔한 데이터 목록 출력
    startScanNStore();
    //반대편 블루투스 모듈(NAME = SIM)이랑 연결
//    connection();
    
  }
  else{ //현재 모드가 Server일 경우
  
  }
  
  BLEWrite();

  SerialWrite();
}

void setCLE110(){
  digitalWrite(spi, LOW); // PIO17 Low=PWM, High=firmware SPI
  digitalWrite(u_sel, LOW); // PIO4 Low=off, High=on
  digitalWrite(reset, LOW);
}

/*
* 종류
* 1. curMode : Server(true), mode : Server(true) -> 문제 없음
* 2. curMode : Server(true), mode : Client(false) -> rolchange 필요
* 3. curMode : Client(false), mode : Server(true) -> rolechange 필요
* 4. curMode : Client(false), mode : Client(false) -> 문제 없음
*/
void setMode(bool mode){ //모드 설정(초기모드가 Server인지 Client인지 확인X)
  Serial.print("초기 모드 : ");
  ATCommand("AT+ROLE?\r", false, true);

   //rolechange 필요한 조건만 확인
   if(curMode && !mode){ // 2번
     changeMode();
   }

   if(!curMode && mode){ // 3번
     changeMode();
   }
}

void setModeSetting(bool mode){ //모드 내에서 세팅 설정
  if(mode){ //Server
    ATCommand("AT\r", false, false);
    ATCommand("AT+MANUF=SIM\r", false, false);
    ATCommand("AT+ADVDATA=I'm SIM\r", false, false);
    ATCommand("AT+ADVINTERVAL=1000\r", false, false);
  }
  else{ //Client
    ATCommand("AT+SCANINTERVAL=500\r", false, false);
//    ATCommand("AT+SCAN\r", false, false); //기본 스캔 = 15초
  }
}

void startScanNStore(){
  String s = "";

  Serial1.print("AT+SCAN=5\r"); // 데이터 전송
  delay(100);

  while(1){
   if(Serial1.available() > 0){ // 버퍼에 쌓인 데이터가 있는지 확인
      char c = Serial1.read();
      if(c == 0x0D){ // 읽은 데이터가 \r(끝)인지 확인
        if(s.indexOf("OK") > -1){ // 스캔 시작을 알리는 OK
          Serial.println(s);
          //다음은 Scanning이 들어온 다음 
          //데이터 목록이 들어옴
          storeData();
          break;
        }
        else{// ERROR인 경우
          s = "";
          Serial1.print("AT+SCAN=5\r"); // 다시 재전송
          delay(100);
        }
      }
      else s += c; // 읽은 데이터가 끝이 아닌 경우 String에 합침
   }
   else delay(10); //버퍼에 쌓인 데이터가 없으면 delay 
  }
}

void storeData(){
  String s = "";
  count = 0;

  while(1){
    if(Serial1.available() > 0){ // 버퍼에 쌓인 데이터가 있는지 확인
      char c = Serial1.read();
      if(c == 0x0D){
        if(s.indexOf("SCANNING") > -1){ // 스캔 시작 SCANNING
          Serial.println("스캔 시작");
//          Serial.println(s);
          s = ""; //데이터 비움
        }
        else if(s.indexOf("OK") > -1){ // 스캔을 종료하는 OK
          Serial.println("스캔 종료");
//          Serial.println(s);
          break;
        }
        else{ // 스캔한 데이터가 한 줄씩 들어오는 경우
//          Serial.println(s);
          if(checkData(s)){ //데이터 확인
            splitData(s); //제대로 된 데이터일 경우 분리해서 저장
          }
          
          s = ""; //데이터 비움  
        }
      }
      else s += c; // 읽은 데이터가 끝이 아닌 경우 String에 합침
    }
    else delay(10); //버퍼에 쌓인 데이터가 없으면 delay 
  }
}

bool checkData(String data){
  //데이터가 제대로된 데이터인지 확인 (ADDR, NAME)
  if((data.indexOf("BT_ADDR") > -1) 
                    && (data.indexOf("NAME") > -1)){
    return true;  
  }
  else return false;
}

void splitData(String data){
  String s = "";
  
  int btStart = data.indexOf("BT_ADDR");

  for(int i = btStart+8; i < data.length(); i++){
    char temp = data.charAt(i);
    if(temp == ']') break; // 끝
    else{ //끝이 아닐 때
      if(temp != ':') s += temp; // ADDR 저장
    }
  }

  s += " ";

  int nameStart = data.indexOf("NAME");

  for(int i = nameStart+6; i < data.length(); i++){
    char temp = data.charAt(i);
    if(temp == ']') break; // 끝
    else s += temp;
  }

  scannedData[count] = s;
  count++; //다음 칸을 사용할 수 있도록 증가

  Serial.println(s);
}

void ATCommand(String input, bool role, bool mode){
  String s = "";
  
  Serial1.print(input); // 먼저 한 번 전송
  delay(100);
  
  while(1){
    if(Serial1.available() > 0){ // 버퍼에 쌓인 데이터가 있는지 확인
      char c = Serial1.read();
      if(c == 0x0D){ // 읽은 데이터가 \r(끝)인지 확인
        if(s.indexOf("OK") > -1){ // 한 문장이 들어온 경우 OK 확인
          
          if(role){ //rolechange일 경우
            Serial.println(s); // OK면 출력
            delay(3000);
            SerialWrite(); //초기화 후 ready출력
          }
          else if(mode){
            curMode = checkMode(); // Server = true, Client = false
          }
          else{ // 남은 데이터 읽어옴
            Serial.println(s); // OK면 출력
            SerialWrite(); 
          }
          
          break; 
        }
        else{ // ERROR인 경우
          s = "";
          Serial1.print(input); // 다시 재전송
          delay(100);
        }
      }
      else s += c; // 읽은 데이터가 끝이 아닌 경우 String에 합침
    }
    else{ //버퍼에 쌓인 데이터가 없으면 다시 delay
      delay(10);
    }
  }
}

boolean checkMode(){ //버퍼에 있는 데이터 들고와서 모드 확인
  String s = "";
  
  while(1){
    if(Serial1.available() > 0){
      char c = Serial1.read();
      if(c == 0x0D){ // 한 줄 다 읽어왔을 때
        Serial.println(s);
        
        if(s.indexOf("SERVER") > -1){ // s에 SERVER이 있는 경우
          return true; // initmode = true (Server)
        }
        else return false; // initmode = false (Client)
        
      }
      else s += c; // 다 읽어오지 않았으면 Stirng에 붙임
      
    }
    else{
      delay(10);
    }  
  }
}

void changeMode(){
  Serial.print("변환 전 모드 : ");
  ATCommand("AT+ROLE?\r", false, true);
  ATCommand("AT+ROLECHANGE\r", true, false);
  Serial.print("변환 후 모드 : ");
  ATCommand("AT+ROLE?\r", false, true);
}

void BLEWrite(){
  if(Serial.available()){
    char c = (char)Serial.read();
    Serial1.write(c);
  }
}

void SerialWrite(){
  while(Serial1.available() > 0){
    char c = (char)Serial1.read();
    if(c == 0x0D){
      Serial.println();
      delay(10);
    }
    else{
      Serial.write(c);
    }
  }
}
