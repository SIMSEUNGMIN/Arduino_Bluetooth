int reset = 5;
int pio10 = 52;
int pio11 = 53;

int u_sel = 23;
int spi = 22;

bool curMode = true; //현재 모드 Server = true, Client = false
bool mode = false; //시작하길 원하는 모드 Server = true, Client = false

int myNode = 0; //자신의 노드 (A~I)
int dstNode = -1; //패킷을 받았을 때 설정되는 노드 (패킷 전달 목적)

int connectedNode = -1; //현재 연결된 노드

//각 노드에서 받아온 데이터를 저장 (행 = dst 노드, 열 = src, src-> dst에 대한 데이터)
//데이터가 전달되고 나면 삭제되어야 함 /////////////////////////////////
String storedPacket[9][2] = "";

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
    
    while(!resultCheckNodeList){
      //하나라도 존재하지 않는 경우가 생긴다면 
      //scan 시작 (함수)
      startScanNStore();
      
      //한 번의 스캔이 끝날 때마다 확인
      resultCheckNodeList = checkNodeList();
      
      //필요한 노드의 정보가 다 모였다면 스캔을 끝내게 됨 (if문으로 resultChecKNodeList확인)
      if(resultCheckNodeList) break;
    }

    //while문을 빠져나온 경우 필요한 노드가 다 Mac주소를 가지고 있다는 뜻이 됨


    //연결을 요청할 노드를 찾음
    //dst가 set된 경우 dst를 연결 노드로 지정, 아닐 경우 연결 요청 한 적 없는 노드에게 연결
    //(이때 알파벳 순서대로 검사하기 때문에 A,C 둘다 연결 흔적이 없을 경우 A랑 먼저 연결하게 됨)
    int resultFindNode = findNodeForConnection();
    
    Serial.print("연결할 노드 : ");
    Serial.println(resultFindNode);

    if(resultFindNode != -1){ //연결할만한 노드가 있는 경우 연결 시도
      sendConnect(resultFindNode);
    }
    else{ //연결할 노드가 없는 경우에는 어떻게 할 것인지 생각
      Serial.println("연결할 노드가 없습니다.");
    }

    //연결된 경우 데이터를 전송
    //데이터가 전송 완료되면 finish 전송 후 종료
    sendPacket();
    
    //Server노드가 연결을 끊기를 기다림
    recvDisconnect();

    //dstNode 초기화
    dstNode = -1;

    //연결 history 갱신
    updateConHistory(); ///////////////만들어야함

    //연결이 끊기고 나면 모드 변경이 일어남
    changeMode(); // 모드 변경
    setModeSetting(curMode); // 모드에 맞는 세팅
    delay(1000);
  }
  else{ //현재 이 노드의 모드가 Server일 경우

    //Advertising하면서 연결을 기다림
    if(recvConnect()){
      //연결 요청이 온 경우 무조건 연결을 받음

      //finish를 받기 전까지 데이터 수신
      recvPacket();
      Serial.println("데이터 수신 완료");

      //COMMAND모드로 들어가 연결을 끊음
      ATCommand("AT+COMMAND\r", false, false);
      //Disconnect 명령 보냄
      sendDisconnect();

      //연결이 끊긴 것을 확인했으면
      //모드 변경
      changeMode();
      setModeSetting(curMode);
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

/*
 * 함수 위치 순서
 * 
 * client
 * checkNodeList
 * startScanNStore
 * findNodeForConnection
 * sendConnect
 * sendPacket
 * recvDisconnect
 * updateConHistory
 * 
 * server(구현 완료)
 * recvConnect
 * recvPacket
 * sendDisconnect
 */

bool checkNodeList(){ //Client, 현재 Client가 노드에 대한 정보를 전부 가지고 있는지 판단
  switch (myNode+65){
    case 'A':
      if(nodeList[('B'-65)][0] != "") return true;
      else return false;
    case 'B':
      if(nodeList[('A'-65)][0] != "" && nodeList[('C'-65)][0] != "") return true;
      else return false;
    case 'C':
      if(nodeList[('B'-65)][0] != "") return true;
      else return false;
    case 'D':
      return false;
    case 'E':
      return false;
    case 'F':
      return false;
    case 'G':
      return false;
    case 'H':
      return false;
    case 'I':
      return false;
  }
}

void startScanNStore(){ //Client, scan작업 및 스캔된 노드 whiteList적용하여 저장
  String s = "";

  Serial1.print("AT+SCAN=5\r"); //데이터 전송
  delay(100);

  while(1){
   if(Serial1.available() > 0){ // 버퍼에 쌓인 데이터가 있는지 확인
      char c = Serial1.read();
      if(c == 0x0D){ // 읽은 데이터가 \r(끝)인지 확인
        if(s.indexOf("OK") > -1){ // 스캔 시작을 알리는 OK
          Serial.println(s);
          //다음은 +Scanning이 들어온 다음 
          //데이터 목록이 들어옴
          storeNode();
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

void storeNode(){ //Client, 필요한 노드들의 데이터만 저장 (White List)
  String s = "";

  while(1){
    if(Serial1.available() > 0){ //버퍼에 쌓인 데이터가 있는지 확인
      char c = Serial1.read();
      if(c == 0x0D){
        if(s.indexOf("SCANNING") > -1){ //스캔 시작 SCANNING
          Serial.println("스캔 시작");
          s = "";
        }
        else if(s.indexOf("OK") > -1){ //스캔 종료 OK
          Serial.println("스캔 종료");
          return;
        }
        else{ // 스캔한 데이터가 한 줄씩 들어오는 경우
          // 데이터가 원하는 형식인지 확인 (ADDR, NAME)
          // 데이터가 원하는 노드 중 하나인지 확인 (White List)
          Serial.println(s);
          if(checkPacketformat(s) && checkPacketNode(s)){ 
            splitPacketNStore(s); //형식도 맞고 원하는 노드일 경우 분리해서 저장
          }
          s = ""; //데이터 비움  
        }
      }
      else s += c; // 읽은 데이터가 끝이 아닌 경우 String에 합침
    }
    else delay(10); //버퍼에 쌓인 데이터가 없으면 delay 
  }
}

bool checkPacketformat(String data){ //Client, 데이터가 ADDR과 NAME을 가지고 있는지 확인
  if((data.indexOf("BT_ADDR") > -1) 
                    && (data.indexOf("NAME") > -1)){
    return true;  
  }
  else return false;
}

bool checkPacketNode(String data){ //Client, 데이터의 NAME이 Client가 원하는 노드인지 확인
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

  //추출한 노드가 원하는 노드 중 하나가 맞는지 확인
  char scannedNode = s.charAt(0);
  if(checkWhiteList(scannedNode)) return true;
  else return false;
}

bool checkWhiteList(char node){ //Client, 추출한 노드가 원하는 노드 중 하나가 맞는지 확인
  switch(myNode+65){
    case 'A':
      if(node == 'B') return true;
      else return false;
    case 'B':
      if((node == 'A') || (node == 'C')) return true;
      else return false;
    case 'C':
      if(node == 'B') return true;
      else return false;
    case 'D':
      return false;
    case 'E':
      return false;
    case 'F':
      return false;
    case 'G':
      return false;
    case 'H':
      return false;
    case 'I':
      return false;
  }
}

void splitPacketNStore(String packet){ //Client, Packet를 분해해서 저장
  String s = "";

  int nameStart = packet.indexOf("NAME");
  char nodeName = packet.charAt(nameStart+6); //NAME이 배열의 위치가 됨
  
  
  int btStart = packet.indexOf("BT_ADDR");
  for(int i = btStart+8; i < packet.length(); i++){
    char temp = packet.charAt(i);
    if(temp == ']') break; // 끝
    else{ //끝이 아닐 때
      if(temp != ':') s += temp; // ADDR 저장
    }
  }

  //ADDR 추출 완료
  nodeList[(nodeName-65)][0] = s;

  //확인용 출력(나중에 생략 가능)
  Serial.print(nodeName);
  Serial.print(" : ");
  Serial.println(s);
}

int findNodeForConnection(){ //Client, 연결을 위한 노드 찾기
  if(dstNode != -1){
    //dstNode로 향하기 위해 연결되어야할 노드 찾기
    int result = chcekRoutingTable(dstNode);
    nodeList[result][1] = "Y";
    return result;
  }

  for(int i = 0; i < 9; i++){
    if(nodeList[i][0] != "" && nodeList[i][1] == ""){
      //만약 i의 addr이 존재하고 연결 history가 존재하지 않는다면
      nodeList[i][1] = 'Y';
      return i;
    }
  }
  
  return -1;
}

int chcekRoutingTable(int dst){//Client, dst로 가기 위해 연결되어야 할 노드를 찾음
  switch(dst+65){
    case 'A':
      return ('B'-65);
    case 'B':
      return ('B'-65);
    case 'C':
      return ('B'-65)
    case 'D':
      return -1;
    case 'E':
      return -1;
    case 'F':
      return -1;
    case 'G':
      return -1;
    case 'H':
      return -1;
    case 'I':
      return -1;
  }
}

void sendConnect(int index){ //Client, 지정한 server에게 연결을 요청
  String addr = nodeList[index][0]; //맥 주소
  
  String addrRequest = "AT+CONNECT=" + addr + "\r";
  Serial1.print(addrRequest);
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
            delay(1000); //연결 안정을 위한 대기
            return;
         }
         else{
            Serial1.print(addrRequest); // 다시 연결 시도
            delay(100);
         }
       }
       else s += c; // 읽은 데이터가 끝이 아닌 경우 String에 합침
    }
    else delay(10);
  } 
}

void sendPacket(int index){ //Client, 연결된 노드에 맞는 데이터 전송
  String packet = "";
  
  if(dstNode != -1){ //전달할 데이터가 있는 경우
    String src = storedPacket[index][0];
    String dst = ((char)(index+65)) + "";
    String data = storedPacket[index][1];

    packet = src + "|" + dst + "|" + data + "." + "\r";

    //전송 후 데이터 초기화
    storedPacket[index][0] = "";
    storedPacket[index][1] = "";
  }
  else{ //목적지가 없는 경우
    switch(index+65){
    case 'A':
      packet = "A|C|hello!!!.\r";
      delay(500);
      break;
    case 'B':
      packet = "I am B!!!.\r";
      break;
    case 'C':
      packet = "C|A|bye!!!.\r";
      break;
    case 'D':
      packet = "I am D!!!.\r";
      break;
    case 'E':
      packet = "I am E!!!.\r";
      break;
    case 'F':
      packet = "I am F!!!.\r";
      break;
    case 'G':
      packet = "I am G!!!.\r";
      break;
    case 'H':
      packet = "I am H!!!.\r";
      break;
    case 'I':
      packet = "I am I!!!.\r";
      break;
    }
  }

  String s= "";

  Serial1.print(packet); //패킷 전송
  
  while(1){
    if(Serial1.available() > 0){
      char c = Serial1.read();
       if(c == "."){
         if(s.indexOf("ok") > -1){ //내가 보낸 데이터가 잘 전송 되었을 시
            Serial.print("데이터 전송 완료");
            delay(50);
            Serial1.print("finish.\r"); //데이터 종료 메세지 전송
            delay(500);
            return;
         }
         else{
           Serial.print("이상한 데이터1 : ");
           Serial.println(s);
           Serial.print(packet); //재전송
           s = "";
         }
       }
       else s += c; //읽은 데이터가 끝이 아닌 경우 String에 합침
    }
    else delay(10);
  } 
}

void recvDisconnect(){ //Client, 연결이 끊기기를 기다림
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
          Serial.print("이상한 데이터2 : ");
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

void updateConHistory(){ //Client, 연결 종료 후 History 관리
  
  //연결 안 된 노드가 존재할 경우
  for(int i = 0; i < 9; i++){
    if(nodeList[i][0] != "" && nodeList[i][1] == "")
      return;
  }

  //모두 연결 이력이 있는 경우
  for(int i = 0; i < 9; i++){
    if(nodeList[i][0] != "")
      nodeList[i][i] = "";
  }
}

bool recvConnect(){ //Server
  String s = ""; 
  
  while(1){
    if(Serial1.available() > 0){
      char c = Serial1.read();
       if(c == 0x0D){ // 읽은 데이터가 \r(끝)인지 확인
         if(s.indexOf("CONNECTED") > -1){ // 연결 성공하면 return
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

void recvPacket(){ //Server
  //finish가 들어오기 전까지 데이터를 받음
  //finish가 아닐 경우 데이터를 받고 OK 전송
  String s = "";
  
  while(1){
    if(Serial1.available() > 0){
      char c = Serial1.read();
      if(c == '.'){ // 읽은 데이터가 끝인지 확인
        if(s.indexOf("finish") > -1){ // 데이터 전송이 끝났음
          Serial.println("데이터 수신 완료");
          return; // 데이터를 받는 함수 종료
        }
        else{

          //전송 받은 데이터의 dst 확인
          int dst = checkDst(s);

          //dst의 목적지가 나인 경우 print
          //내가 아닌 경우 저장
          if(dst == myNode){
            Serial.println(s);
          }
          else{
            storePacket(s, dst);
            dstNode = dst; //데이터를 전송할 노드 설정
          }
          
          //OK전송
          Serial1.print("ok.\r");
          //버퍼 지우기
          s = "";
        } 
      }
      else s += c;
    }
    else delay(10);
  }
}

int checkDst(String input){ //Server, dstIndex를 반환
  return ((input.subString(2,3)).charAt(0)-65);
}

void storePacket(String input, int dstIndex){ //Server, 데이터 저장
  //데이터 분리 (데이터 구조 : src, dst, data)
  String src = input.subString(0,1);
  String data = input.subString(4, input.length);

  //데이터 저장(행->dst, 열0->src, 열1->data)
  storedPacket[dstIndex][0] = src;
  storedPacket[dstIndex][1] = data; 
}

void sendDisconnect(){ //Server, 연결 끊기 요청
  String s = "";
  
  Serial1.print("AT+DISCONNECT\r");
  delay(100);

  while(1){
    if(Serial1.available() > 0){ //버퍼에 쌓인 데이터가 있는지 확인
      char c = Serial1.read();
      if(c == 0x0D){ //읽은 데이터가 \r(끝)인지 확인
        if(s.indexOf("OK") > -1){ //한 문장이 들어온 경우 OK 확인
          Serial.println(s);
          s = "";
        }
        else if(s.indexOf("DISCONNECT") > -1){
          Serial.println(s);
          return;
        }
        else{ //ERROR인 경우
          s = "";
          Serial1.print("AT+DISCONNECT\r");
          delay(100);
        }
      }
      else s += c; //읽은 데이터가 끝이 아닌 경우 String에 합침
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
