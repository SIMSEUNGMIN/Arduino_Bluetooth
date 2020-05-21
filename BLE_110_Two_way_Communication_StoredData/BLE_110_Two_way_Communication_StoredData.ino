int reset = 5;
int pio10 = 52;
int pio11 = 53;

int u_sel = 23;
int spi = 22;

bool curMode = true; //현재 모드 Server = true, Client = false
bool mode = false; //시작하길 원하는 모드 Server = true, Client = false

int connectedNode = -1; //현재 연결된 노드

//각 노드에서 받아온 데이터를 저장
String storedData[9] = "";

//필요한 노드들의 데이터를 저장할 리스트(9개 A~I)
//현재는 한 번 스캔할 때마다 초기화된다
String scannedData[9] = "";

void setup() {
  Serial.begin(9600);
  Serial1.begin(9600);

  scannedData[0] = "74F07DC9CF7E";
  
  setCLE110();
  setMode(mode); //처음 시작시 설정되어있는 모드 확인
  setModeSetting(mode); //원하는 모드의 세부 세팅
  delay(1000);
}

void loop() {
  if(!curMode){ //현재 모드가 Client일 경우
    //스캔 시작 및 데이터(필요한 노드의 ADDR랑 NAME) 저장
    //저장된 데이터 목록 출력
    startScanNStore();

    //연결할만한 노드가 있는지 확인
    //(순차적으로 확인, 여러 개 있을 시 알파벳 순으로 가장 처음 노드랑 연결)
    int findResult = findData();
    Serial.print("연결할 만한 노드 : ");
    Serial.println(findResult);
    
    if(findResult != -1){ //연결될만한 노드가 있다면
      sendConnect(findResult); //상대 블루투스 모듈이랑 연결

      //연결되고 나서 내가 누구인지 전송

      if(recvRequest()){ //데이터 전송 요청을 받았을 경우
        //연결된 노드에 따른 데이터를 전송
        Serial.println("데이터를 보냅니다.");

        //데이터를 보내는 함수 (!!!!!!!!!!!!!!!!만들어야함)
        sendData(findResult);
      }

      //데이터 전송이 끝난 경우 or 데이터 전송 요청을 받지 못한 경우(blacklist)

      //연결이 끊기기를 기다렸다가 연결이 끊기고 나면 모드 변경 및 모드 세팅
      recvDisconnect();

      changeMode(); // 모드 변경
      setModeSetting(curMode); // 모드에 맞는 세팅
      delay(1000);
    }
    else startScanNStore(); // 못 찾았으면 다시 스캔
  }
  else{ //현재 모드가 Server일 경우
    
    //기다리다가 +CONNDECTED명령어를 받으면 연결된 것을 확인
    if(recvConnect()){
      //연결되었으면 노드 확인
      checkNode();
      //데이터 요청
      Serial.println("데이터를 요청합니다.");
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
    ATCommand("AT+MANUF=A\r", false, false);
    ATCommand("AT+ADVDATA=I'm A\r", false, false);
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
          //다음은 +Scanning이 들어온 다음 
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
  // 필요한 노드들의 데이터만 저장
  String s = "";

  while(1){
    if(Serial1.available() > 0){ // 버퍼에 쌓인 데이터가 있는지 확인
      char c = Serial1.read();
      if(c == 0x0D){
        if(s.indexOf("SCANNING") > -1){ // 스캔 시작 SCANNING
          Serial.println("스캔 시작");
          s = ""; //데이터 비움
        }
        else if(s.indexOf("OK") > -1){ // 스캔을 종료하는 OK
          Serial.println("스캔 종료");
          break;
        }
        else{ // 스캔한 데이터가 한 줄씩 들어오는 경우
          // 데이터가 원하는 형식인지 확인 (ADDR, NAME)
          // 데이터가 원하는 노드 중 하나인지 확인
          Serial.println(s);
          if(checkDataformat(s) && checkDataNode(s)){ 
            splitData(s); //형식도 맞고 원하는 노드일 경우 분리해서 저장
          }
          s = ""; //데이터 비움  
        }
      }
      else s += c; // 읽은 데이터가 끝이 아닌 경우 String에 합침
    }
    else delay(10); //버퍼에 쌓인 데이터가 없으면 delay 
  }
}

bool checkDataformat(String data){
  // 데이터가 ADDR과 NAME을 가지고 있는지 확인
  if((data.indexOf("BT_ADDR") > -1) 
                    && (data.indexOf("NAME") > -1)){
    return true;  
  }
  else return false;
}

bool checkDataNode(String data){
  //데이터의 NAME이 Client가 원하는 노드인지 확인
  String s = "";

  //NAME 추출
  int nameStart = data.indexOf("NAME");
  for(int i = nameStart+6; i < data.length(); i++){
    char temp = data.charAt(i);
    if(temp == ']') break; // 끝
    else s += temp;
  }

  // 필요한 노드 (A,B,C,D,E,F,G,H,I)
  //추출한 NAME 길이 확인
  if(s.length() != 1) return false;

  //추출한 노드의 철자 확인
  char c = s.charAt(0);
  if('A' <= c <= 'I') return true;
  else return false;
}

void splitData(String data){
  //data를 분해해서 저장
  String s = "";
  
  int btStart = data.indexOf("BT_ADDR");

  for(int i = btStart+8; i < data.length(); i++){
    char temp = data.charAt(i);
    if(temp == ']') break; // 끝
    else{ //끝이 아닐 때
      if(temp != ':') s += temp; // ADDR 저장
    }
  }

  int nameStart = data.indexOf("NAME");
  char name = data.charAt(nameStart+6); //NAME이 배열의 위치가 됨

  scannedData[name-65] = s; //addr만 저장
  
  Serial.println(s);
}

int findData(){ // 노드 A부터 노드 I에 대한 white list 제도
  for(int i = 0; i < 9; i++){
    Serial.print("현재 상태 ");
    Serial.print(char(i+65));
    Serial.print(" : ");
    Serial.println(scannedData[i]);
    
    if(!scannedData[i].equals("")){
        switch(i+65){
          case 'A':
            return i;
          case 'B':
            return i;
          case 'C':
            break;
          case 'D':
            break;
          case 'E':
            return i;
          case 'F':
            return i;
          case 'G':
            break;
          case 'H':
            break;
          case 'I':
            return i;
        }
    }
  }

  return -1;
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
            //연결이 성공되고 1초 정도 기다렸다가 내가 누구인지 알려줌
            Serial1.print("A.\r");
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

void checkNode(){ //연결된 후 client가 어떤 노드인지 알리는 메세지 받음
  String s = "";
  
  while(1){
    if(Serial1.available() > 0){
      char c = Serial1.read();
      if(c == '.'){ //읽은 데이터가 끝인지 확인
        if(s.length() == 1){
          connectedNode = (s.charAt(0)-65);
          Serial.print("연결된 노드 : ");
          Serial.println(s);
          return;
        }
        else{
          Serial.print("이상한 데이터 : ");
          Serial.println("s");
        }
      }
      else s += c;
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

bool recvRequest(){ //데이터 전송을 요청 받는 함수, Client
  String s = ""; 
  
  while(1){
    if(Serial1.available() > 0){
      char c = Serial1.read();
       if(c == '.'){ //읽은 데이터가 (끝)인지 확인
         if(s.indexOf("send") > -1){ //데이터 전송 요청 받음
            Serial.println(s);
            Serial1.print("ok.\r"); //데이터 전송 요청을 잘 받았다는 의미
            delay(100);
            return true;
         }
         else{
           Serial.print("이상한 데이터2 : ");
           Serial.println(s);
           s = "";
         }
       }
       else s += c; //읽은 데이터가 끝이 아닌 경우 String에 합침
    }
    else delay(10);
  } 
}

void sendData(int index){ //연결된 노드에 맞는 데이터를 전송
  switch(index+65){
    case 'A':
      Serial1.print("hi i am data.\r");
      delay(500);
      break;
    case 'B':
      break;
    case 'C':
      break;
    case 'D':
      break;
    case 'E':
      break;
    case 'F':
      break;
    case 'G':
      break;
    case 'H':
      break;
    case 'I':
      break;
  }

  Serial1.print("finish.\r"); //데이터 종료 메세지
  delay(500);
}

void checkRecvData(){ //finish가 들어오기 전까지 들어오는 데이터 받음, Server
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
          storedData[connectedNode] = s; //전송 받은 데이터 저장
          Serial.print("전송 받은 데이터 : ");
          Serial.println(storedData[connectedNode]); //전송된 데이터 출력
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
