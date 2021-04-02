#include <Wire.h>
#include "ESP8266WiFi.h"
#include <ESP8266WebServer.h>
#include <Adafruit_GFX.h>
#include "Adafruit_LEDBackpack.h"
#include "matrix.h"

// WiFi parameters to be configured
const char* ssid = "SSID";
const char* password = "PWD";
ESP8266WebServer server(80);

char *text = (char*)"Aguardando nome da faixa..."; //Define a startup message

String receivedText;

int SIZE_OF_NONE_ARRAY = sizeof(NONE_ARRAY);

const int CLUSTER_SIZE = 4;

Adafruit_8x8matrix matrix[CLUSTER_SIZE];

int *textArray;
int textLen;
int *joinedArray;

//TODO: use an array of display buffers
int displayOne[MATRIX_LEN];
int displayTwo[MATRIX_LEN];
int displayThree[MATRIX_LEN];
int displayFour[MATRIX_LEN];

void setup()
{
  Serial.begin(9600);
  Serial.println("8x8 LED Matrix Test");
  Serial.println(text);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
     delay(500);
     Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println(WiFi.localIP());
 
  server.on("/text", handleText);
  server.begin(); 
  
  int addresses[CLUSTER_SIZE] = {0x70, 0x71, 0x74, 0x75};
  for (int m = 0; m < CLUSTER_SIZE; m++)
  {
    matrix[m] = Adafruit_8x8matrix();
    matrix[m].begin(addresses[m]);
    matrix[m].setTextWrap(false);
    matrix[m].setTextColor(LED_ON);
    matrix[m].setRotation(3);
  }
  
  fillData();
}

int textChanged;

void handleText() 
{
  String value = server.arg("value");
  Serial.print("Value: ");
  Serial.println(value);
  receivedText = urldecode(value);
  server.send(202, "text/plain", "OK");
  textChanged = true;
}

//int aSecond;

void loop()
{  
  flush(matrix[3], displayOne);
  flush(matrix[2], displayTwo);
  flush(matrix[1], displayThree);
  flush(matrix[0], displayFour);
  if(textChanged)
  {
    textChanged = false;
    text = (char*)malloc(receivedText.length() * sizeof(int));
    receivedText.toCharArray(text, receivedText.length() + 1);
    //text = (char*)"Teste";
    fillData();
    return;
  }
  scroll();
  server.handleClient();
  delay(100);
  
}

void flush(Adafruit_8x8matrix matrix, int displayData[]) 
{
  matrix.clear();
  matrix.setCursor(0, 0);
  uint8_t* bmp = backToBitMap(displayData);
  matrix.drawBitmap(0, 0, bmp, 8, 8, LED_ON);
  matrix.writeDisplay();
  free(bmp);
}

void scroll() 
{
  int *scrolled = scrollMatrix(joinedArray, (textLen + CLUSTER_SIZE) * MATRIX_LEN);
  memcpy(joinedArray, scrolled, (textLen + CLUSTER_SIZE) * SIZE_OF_NONE_ARRAY);
  free(scrolled);
  splitData();
}

//////////////////////////
// Utils
//////////////////////////
int getRealSize(char *utf)
{
  int size = 0;
  for (int i = 0; i < strlen(utf); i++)
  {
    if (utf[i] != 0xc2 && utf[i] != 0xc3)
      size++;
  }
  return size;
}

int *indexedString(char *utfStr)
{
  int *result =(int *) malloc(getRealSize(utfStr) * sizeof(int));;
  int lastCharIndex = -1;
  int theChar = 0;
  for (int i = 0; i < strlen(utfStr); i++)
  {
    char ch = utfStr[i];
    if (ch == 0xc2)
    {
      lastCharIndex = 0;
      continue;
    }
    if (ch == 0xc3)
    {
      lastCharIndex = 1;
      continue;
    }

    if (lastCharIndex <= 0)
    {
      result[theChar++] = ch;
    }
    else
    {
      result[theChar++] = ch + 64; // => 0x7f - 0xbf
    }

    lastCharIndex = -1;
  }

  return result;
}

uint8_t* backToBitMap(int origin[]) {
  uint8_t* ret = (uint8_t *)malloc(8 * sizeof(uint8_t));
  for(int row = 0; row < 8; row++)
  {
    uint8_t val = 0;
    for(int col = 0; col < 8; col++)
    {
      bitWrite(val, 7 - col, origin[col + (row * 8)]);
    }
    ret[row] = val;
  }
  return ret;//_utfChars[164];
}

int *n2bits(uint8_t value)
{
  int *destination = (int *)malloc(8 * sizeof(int));
  for (int nBit = 7; nBit >= 0; nBit--)
  {
    int k = value >> nBit;
    destination[7 - nBit] = k & 1;
  }
  return destination;
}

int *convertChar(uint8_t *from)
{
  int *temp = (int *)malloc(MATRIX_LEN * sizeof(int));
  for (int row = 0; row <= 7; row++)
  {
    int *section = n2bits(from[row]);
    for (int j = 0; j <= 7; j++)
    {
      temp[(row * 8) + j] = section[j];
    }
    free(section);
  }
  
  return temp;
}

int *getMatrix(int *from, int display)
{ 
  int *result = (int *)malloc(MATRIX_LEN * sizeof(int));
  memcpy(result, from + (MATRIX_LEN * display), MATRIX_LEN * sizeof(int));
  return result;
}

void fillData()
{
  //  text = getFileContent(watchFileName);
  textArray = indexedString(text);
  textLen = getRealSize(text);
  free(joinedArray); // arduino realloc doesn't fit
  joinedArray = (int *)malloc((textLen + CLUSTER_SIZE) * SIZE_OF_NONE_ARRAY);
  for (int disp = 0; disp < CLUSTER_SIZE; disp++)
  {
    memcpy(joinedArray + (MATRIX_LEN * disp), NONE_ARRAY, SIZE_OF_NONE_ARRAY); //lead empty spaces
  }

  for (int ch = 0; ch < textLen; ch++)
  {
    int *CH = convertChar(_utfChars[textArray[ch] - 0x20]);
    int offset = (MATRIX_LEN * CLUSTER_SIZE) + (ch * MATRIX_LEN);
    memcpy(joinedArray + offset, CH, SIZE_OF_NONE_ARRAY);
    free(CH);
  }

  splitData();
  free(textArray);
}

void splitData()
{
  //TODO: convert to itens array
  int *dsp1 = getMatrix(joinedArray, 1);
  int *dsp2 = getMatrix(joinedArray, 2);
  int *dsp3 = getMatrix(joinedArray, 3);
  int *dsp4 = getMatrix(joinedArray, 4);
  memcpy(displayOne, dsp1, SIZE_OF_NONE_ARRAY);
  memcpy(displayTwo, dsp2, SIZE_OF_NONE_ARRAY);
  memcpy(displayThree, dsp3, SIZE_OF_NONE_ARRAY);
  memcpy(displayFour, dsp4, SIZE_OF_NONE_ARRAY);
  free(dsp1);
  free(dsp2);
  free(dsp3);
  free(dsp4);
}

int *scrollMatrix(int *from, int max)
{

  int *scrolled = (int *)malloc(max * sizeof(int));

  for (int pos = 1; pos <= max; pos++)
  {
    int ref = pos;
    if (pos % 8 == 0)
    {
      if (pos > (max - MATRIX_LEN))
      {
        ref = pos - (max - 56);
      }
      else
      {
        ref = pos + 56;
      }
    }
    scrolled[pos - 1] = from[ref];
  }

  return scrolled;
}

String urldecode(String str)
{
    
    String encodedString="";
    char c;
    char code0;
    char code1;
    for (int i =0; i < str.length(); i++){
        c=str.charAt(i);
      if (c == '+'){
        encodedString+=' ';  
      }else if (c == '%') {
        i++;
        code0=str.charAt(i);
        i++;
        code1=str.charAt(i);
        c = (h2int(code0) << 4) | h2int(code1);
        encodedString+=c;
      } else{
        
        encodedString+=c;  
      }
      
      yield();
    }
    
   return encodedString;
}

unsigned char h2int(char c)
{
    if (c >= '0' && c <='9'){
        return((unsigned char)c - '0');
    }
    if (c >= 'a' && c <='f'){
        return((unsigned char)c - 'a' + 10);
    }
    if (c >= 'A' && c <='F'){
        return((unsigned char)c - 'A' + 10);
    }
    return(0);
}
