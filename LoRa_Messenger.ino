/*
 * Title: Long Range Radio messenger
 * 
 * Author: Zac Turner
 */
#include "heltec.h"

#define BAND 915E6

struct message
{
  String message;
  int received = -1; // -1 = N/A, 0 = Not acknowledged, 1 = acknowledged, 2 = Received
  int timeSent = 0;
};

const int rows = 6;
const int columns = 13;
String textLine = "             ";
message messages[rows - 1];

int iterator = 0;

const int joyPinX = 36;
const int joyPinY = 37;
const int button = 22;

/*
 * Draws all information on the display
 */
void drawDisplay()
{
  Heltec.display -> clear();

  Heltec.display -> drawString((iterator) * 9, 0, "_");
  Heltec.display -> drawString((iterator) * 9 + 3, 0, "_");
  
  for (int x = 0; x < columns; x++)
  {
    if (textLine == "")
      break;
    Heltec.display -> drawString(x * 9, 0, String(textLine[x]));
  }
  
  Heltec.display -> drawString(0, 1, "_______________________");
  Heltec.display -> drawString(118, 10, "|");

  for (int x = 0; x < rows - 1; x++)
  {
    int offset = ((x + 1) * 10) + 1;
    Heltec.display -> drawString(0, offset, messages[x].message);
    if (messages[x].received == 0)
    {
      Heltec.display -> drawString(118, offset, "|N");
      Heltec.display -> drawString(118, offset + 5, "|");
    }
    else if (messages[x].received == 1)
    {
      Heltec.display -> drawString(118, offset, "|A");
      Heltec.display -> drawString(118, offset + 5, "|");
    }
    else if (messages[x].received == 2)
    {
      Heltec.display -> drawString(118, offset, "|R");
      Heltec.display -> drawString(118, offset + 5, "|");
    }
    else
    {
      Heltec.display -> drawString(118, offset, "|");
      Heltec.display -> drawString(118, offset + 5, "|");
    }
  }
  
  Heltec.display -> display();
}

/*
 * Moves to the next acceptable character
 * Moves in this order: " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
 */
char nextAcceptableASCII(char ascii)
{
  if (ascii == 32)
    ascii = 65;
  else if (ascii >= 65 && ascii < 90)
    ascii++;
  else if (ascii == 90)
    ascii = 48;
  else if (ascii >= 48 && ascii <57)
    ascii++;
  else
    ascii = 32;

  return ascii;
}

/*
 * Moves to the previous acceptable character
 * Moves in this order: " 9876543210ZYXWVUTSRQPONMLKJIHGFEDCBA"
 */
char prevAcceptableASCII(char ascii)
{
  if (ascii == 32)
    ascii = 57;
  else if (ascii <= 57 && ascii > 48)
    ascii--;
  else if (ascii == 48)
    ascii = 90;
  else if (ascii <= 90 && ascii > 65)
    ascii--;
  else
    ascii = 32;
  
  return ascii;
}

/*
 * Moves the iterator and allows cycling through letters. Has a 150ms cooldown.
 */
int iterPrevTime = 0;
void moveIterator(int joyX, int joyY)
{
  int currentTime = millis();
  if (currentTime - iterPrevTime >= 150)
  {
    iterPrevTime = currentTime;
    if (joyX < 100) // joystick left
    {
      iterator--;
      if (iterator < 0)
        iterator = 0;
      else if (iterator > 12)
        iterator = 12;
      return;
    }
    if (joyX > 4000) // joystick right
    {
      iterator++;
      if (iterator < 0)
        iterator = 0;
      else if (iterator > 12)
        iterator = 12;
      return;
    }
    if (joyY < 100) // joystick up
    {
      textLine[iterator] = prevAcceptableASCII(textLine[iterator]);
      return;
    }
    if (joyY > 4000) // joystick down
    {
      textLine[iterator] = nextAcceptableASCII(textLine[iterator]);
      return;
    }
  }
}

/*
 * Adds the newest item to the history and shift the rest down
 */
void shiftHistory(String message)
{
  messages[4] = messages[3];
  messages[3] = messages[2];
  messages[2] = messages[1];
  messages[1] = messages[0];
  messages[0].message = message;

  messages[0].received = -1;
}

/*
 * Sends the message and clears the textline
 */
void sendIt()
{
  shiftHistory(textLine);
  messages[0].timeSent = millis();

  LoRa.beginPacket();
  LoRa.print(textLine);
  LoRa.endPacket();
  Serial.println("Sent: " + textLine);
  
  iterator = 0;
  textLine = "             ";
}

/*
 * Checks for incoming messages and acknowledges any it receives with a return message.
 */
void listener()
{
  int packetSize = LoRa.parsePacket();
  if (packetSize)
  {
    String packet = "";
    while (LoRa.available())
      packet += (char)LoRa.read();
    Serial.println("packet received: " + packet);
    if (packet == "@") // received acknowledgement
    {
      messages[0].received = 1;
      Serial.println("received acknowledgement");
    }
    else               // received message
    {
      Serial.println("message received, sending acknowledgement");
      shiftHistory(packet);
      messages[0].received = 2;
      bool beginAck = LoRa.beginPacket(); // send acknowledgement
      int bytesSent = LoRa.print("@");
      bool endAck = LoRa.endPacket();
      Serial.println("beginAck: " + String(beginAck) + "   bytesSent: " + String(bytesSent) + "   endAck: " + String(endAck));
    }
  }
}

/*
 * Checks for any sent messages that never received an ack and marks it as not acknowledged if 3 seconds have passed.
 */
void nAckChecker()
{
  for (int x = 0; x < 5; x++)
  {
    if (messages[x].timeSent > 0 && messages[x].received == -1)
    {
      if (millis() - messages[x].timeSent >= 3000)
      {
        messages[x].received = 0;
        Serial.println("no ack received");
      }
    }
  }
}

/*
 * Initializes the Display, LoRa radio, and serial
 */
void setup() 
{
  Heltec.begin(true, // Display Enabled
               true, // LoRa Radio Enabled
               true, // Serial Enabled, 115200 BAUD
               true, // PABoost Enabled
               BAND); // Radio Frequency

  pinMode(button, INPUT_PULLUP);
  LoRa.setSyncWord(0xDD); // only talks to devices with the same SyncWord
}

/*
 * Continually read the joystick and calls the other functions.
 */
bool prevButtonPress = 1;
void loop() 
{
  drawDisplay();

  int joyX = analogRead(joyPinX);
  int joyY = analogRead(joyPinY);
  int buttonPress = digitalRead(button); // 0 = pressed down

  moveIterator(joyX, joyY);

  if (buttonPress == 0 && prevButtonPress == 1) // makes sure only one message is sent with each click on the joystick
  {
    prevButtonPress = 0;
    sendIt();
  }
  else if (buttonPress == 1 && prevButtonPress == 0)
    prevButtonPress = 1;

  listener();

  nAckChecker();
}
