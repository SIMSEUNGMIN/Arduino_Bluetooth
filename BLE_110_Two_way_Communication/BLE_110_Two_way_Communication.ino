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
    
    int findResult = findData("SIM");  //목록에서 SIM을 찾음
    
    if(findResult != -1){ //찾았으면 연결
      sendConnect(findResult); //반대편 블루투스 모듈(NAME = SIM)이랑 연결

      if(recvRequest()){
        //데이터 전송 요청을 받으면 데이터를 전송
        Serial.println("데이터를 보낼 것");
        Serial1.print("i am data.\r");
        delay(500);
        Serial1.print("finish.\r");
        delay(500);
        //데이터 전송 완료를 보내고 나서 연결이 끊기기를 기다림
        recvDisconnect();

        changeMode(); // 모드 변경
        setModeSetting(curMode); // 모드에 맞는 세팅
        delay(1000);
      }
    }
    else startScanNStore(); // 못 찾았으면 다시 스캔
  }
  else{ //현재 모드가 Server일 경우
    
    //기다리다가 +CONNDECTED명령어를 받으면 연결된 것을 확인
    if(recvConnect()){
      //연결되었으면 데이터 요청
      Serial.println("데이터를 요청");
      sendRequest(); // 데이터 전송을 요청
      Serial.println("데이터 전송 요청 완료");
      //데이터 확인
      checkRecvData();
      Serial.println("데이터 수신 완료");
      //데이터를 다 받았으면 연결 종료
      ATCommand("AT+COMMAND\r", false, false); //커맨드 모드로 변경
      sendDisconnect(); // Disconnect 명령 보냄
      
      changeMode(); //모드 변경
      setModeSetting(curMode); // 모드에 맞는 세팅
      delay(1000);
    }
  }
  
//  BLEWrite();
//  SerialWrite();
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
          return;
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

int findData(String input){ //input이 있으면 해당 배열 칸을 return
  for(int i = 0; i < count; i++){
    String temp = scannedData[i];
    if(temp.indexOf(input) > -1)
      return i;  
  }

  return -1; // 전부 확인 했는데 없을 경우 -1
}

void sendConnect(int index){ // Client
  String addr = scannedData[index].substring(0, 12); //맥 주소 추출
  
  addr = "AT+CONNECT=" + addr + "\r";
  Serial1.print(addr);
  delay(100);

  String s = ""; 
  
  while(1){
    if(Serial1.available() > 0){
      char c = Serial1.read();
       if(c == 0x0D){ // 읽은 데이터가 \r(끝)인지 확인
         if(s.indexOf("OK") > -1){
            s = "";
         }
         else if(s.indexOf("CONNECTED") > -1){ // 연결 성공
            Serial.println(s);
            delay(1000);
            return;
         }
         else{
            Serial1.print(addr); // 다시 연결 시도
            delay(100);
         }
       }
       else s += c; // 읽은 데이터가 끝이 아닌 경우 String에 합침
    }
    else delay(10);
  } 
}

bool recvConnect(){ //Server
  String s = ""; 
  
  while(1){
    if(Serial1.available() > 0){
      char c = Serial1.read();
       if(c == 0x0D){ // 읽은 데이터가 \r(끝)인지 확인
         if(s.indexOf("CONNECTED") > -1){ // 연결 성공
            Serial.println(s);
            delay(1000);
            return true;
         }
         else{
           Serial.print("이상한 데이터1 : ");
           Serial.println(s);
           s = "";
         }
       }
       else s += c; // 읽은 데이터가 끝이 아닌 경우 String에 합침
    }
    else delay(10);
  } 
}

void sendRequest(){ //Server, 데이터 전송 요청을 보내는 함수
  String s = "";

  Serial1.print("send me data.\r");
  delay(100);

  int count = 0;

//  while(1){
//    Serial1.print(count++);
//    Serial1.print('\r');
//    Serial1.print("send me data.\r");
//    delay(100);
//  }
  
  while(1){
    if(Serial1.available() > 0){
      char c = Serial1.read();
      if(c == '.'){ // 읽은 데이터가 끝인지 확인
        if(s.indexOf("ok") > -1){ // 데이터를 보낼 준비 완료라는 뜻
          Serial.println(s);
          return; // 데이터를 받는 함수 종료
        }
        else{
          s = "";
          Serial1.print("send me data.\r");
          Serial.println("hi");
          delay(500);
        } 
      }
      else s += c;
    }
    else delay(10);
  }
}

bool recvRequest(){ // 데이터 전송을 요청 받는 함수, Client
  String s = ""; 
  
  while(1){
    if(Serial1.available() > 0){
      char c = Serial1.read();
       if(c == '.'){ // 읽은 데이터가 (끝)인지 확인
         if(s.indexOf("send") > -1){ // 데이터 전송 요청 받음
            Serial.println("드디어 도착했다!!!!!!!!!!!!!!");
            Serial.println(s);
            Serial1.print("ok.\r");
            delay(100);
            return true;
         }
         else{
           Serial.print("이상한 데이터2 : ");
           Serial.println(s);
           s = "";
         }
       }
       else s += c; // 읽은 데이터가 끝이 아닌 경우 String에 합침
    }
    else delay(10);
  } 
}

void checkRecvData(){ // finish가 들어오기 전까지 들어오는 데이터 받음, Server
  String s = "";
  
  while(1){
    if(Serial1.available() > 0){
      char c = Serial1.read();
      if(c == '.'){ // 읽은 데이터가 끝인지 확인
        if(s.indexOf("finish") > -1){ // 데이터 전송이 끝났음
          Serial.println(s);
          return; // 데이터를 받는 함수 종료
        }
        else{
          Serial.println(s); //전송된 데이터 출력
          s = "";
        } 
      }
      else s += c;
    }
    else delay(10);
  }
}

void sendDisconnect(){ //Server
  String s = "";
  
  Serial1.print("AT+DISCONNECT\r");
  delay(100);

  while(1){
    if(Serial1.available() > 0){ // 버퍼에 쌓인 데이터가 있는지 확인
      char c = Serial1.read();
      if(c == 0x0D){ // 읽은 데이터가 \r(끝)인지 확인
        if(s.indexOf("OK") > -1){ // 한 문장이 들어온 경우 OK 확인
          Serial.println(s);
          s = "";
        }
        else if(s.indexOf("DISCONNECT") > -1){
          Serial.println(s);
          return;
        }
        else{ // ERROR인 경우
          s = "";
          Serial1.print("AT+DISCONNECT\r");
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

void recvDisconnect(){ //Client
  String s = "";
  
  while(1){
    if(Serial1.available() > 0){ // 버퍼에 쌓인 데이터가 있는지 확인
      char c = Serial1.read();
      if(c == 0x0D){ // 읽은 데이터가 \r(끝)인지 확인
        if(s.indexOf("DISCONNECT") > -1){
          Serial.println(s);
          return;
        }
        else{ // ERROR인 경우
          Serial.print("이상한 데이터3 : ");
          Serial.println(s);
          s = "";
        }
      }
      else s += c; // 읽은 데이터가 끝이 아닌 경우 String에 합침
    }
    else{ //버퍼에 쌓인 데이터가 없으면 다시 delay
      delay(10);
    }
  }
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

        delay(1000); //모드가 변경되면서 설정들이 조절될 시간
        SerialWrite();
        
        if(s.indexOf("SERVER") > -1){ // s에 SERVER이 있는 경우
//          delay(1000); //모드가 변경되면서 설정들이 조절될 시간
//          SerialWrite();
          return true; // curmode = true (Server)
        }
        else{
//           delay(1000); //모드가 변경되면서 설정들이 조절될 시간
//           SerialWrite();
          return false; // curmode = false (Client)
        }
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
