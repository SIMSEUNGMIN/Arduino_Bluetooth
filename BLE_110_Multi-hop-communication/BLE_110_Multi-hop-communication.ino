int reset = 5;
int pio10 = 52;
int pio11 = 53;

int u_sel = 23;
int spi = 22;

bool curMode = true; //현재 모드 Server = true, Client = false
bool mode = false; //시작하길 원하는 모드 Server = true, Client = false

int myNode = 0; //자신의 노드 (A~I)

int connectedNode = -1; //현재 연결된 노드

//각 노드에서 받아온 데이터를 저장 (행 = src 노드, 열 = dst, src-> dst에 대한 데이터)
//데이터가 전달되고 나면 삭제되어야 함 /////////////////////////////////
String storedData[9][2] = "";

//필요한 노드들의 데이터를 저장할 List(9개 A~I) -> 계속 지속되는 List /////////////////////
//-> 각 List마다 scannedData로 최종 저장되어야 하는 노드가 다르지만
//   코드의 통일성을 위해서 9칸 배열을 생성
// 행 = scanned 노드, 열 = scanned 노드의 mac 주소, 연결 history
String nodeList[9][2] = ""; // -> 원래 명 : scannedData

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
  if(!curMode){ //현재 이 노드의 모드가 Client일 경우
    //자신의 nodeList에 자신이 가져야하는 Node가 다 Mac주소를 가지고 있는지 확인

    boolean resultCheckNodeList = checkNodeList();
    
    while(!resultCheckNodeList){ /////////////////////////////////////////////////// 아직 생성 안 함
      //하나라도 존재하지 않는 경우가 생긴다면 
      //scan 시작 (함수)
      startScanNStore();
      
      //한 번의 스캔이 끝날 때마다 확인 (resultCheckNodeList = checkNodeList)
      resultCheckNodeList = checkNodeList();
      
      //필요한 노드의 정보가 다 모였다면 스캔을 끝내게 됨 (if문으로 resultChecKNodeList확인)
      if(resultCheckNodeList) return;
      
    }

    //while문을 빠져나온 경우 필요한 노드가 다 Mac주소를 가지고 있다는 뜻이 됨
   
    //자신이 연결 요청을 했던 적 없는 노드에게 연결
    //(이때 알파벳 순서대로 검사하기 때문에 A,C 둘다 연결 흔적이 없을 경우 A랑 먼저 연결하게 됨)

    //연결된 경우 데이터를 전송

    //데이터를 다 전송 후 (상대방으로부터 OK를 받음)
    //OK를 받은 경우 Finish를 전송
    //Server노드가 연결을 끊음

    //연결이 끊기고 나면 모드 변경이 일어남
  }
  else{ //현재 이 노드의 모드가 Server일 경우

    //Advertising하면서 연결을 기다림
    
    //연결 요청이 온 경우 무조건 연결을 받음

    //연결 후 Client가 데이터를 전송했을 때 데이터를 받았으면 
    //OK를 전송 
    //Finish를 받으면 데이터 전송이 끝났다고 생각

    //(위의 절차를 밟는 함수 안에서 Data의 출력 및 분리 저장 또한 일어남)
    

    //COMMAND모드로 들어가 연결을 끊음

    //연결이 끊긴 것을 확인했으면
    //모드 변경
  
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
  //checkMode가 수행되는 경우는 AT+ROLE의 경우
  //(RoleChange 다음의 ROLE은 RoleChange의 데이터가 남아있을 수도 있어서 밑에 delay 1000)
  
  String s = "";
  
  while(1){
    if(Serial1.available() > 0){
      char c = Serial1.read();
      if(c == 0x0D){ // 한 줄 다 읽어왔을 때
        Serial.println(s);

        delay(1000); //모드가 변경되면서 설정들이 조절될 시간 
        SerialWrite();
        
        if(s.indexOf("SERVER") > -1){ // s에 SERVER이 있는 경우
          return true; // curmode = true (Server)
        }
        else{
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
