
#include <IRremote.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C display(0x20, 16, 2);

const byte capacitorBankInput = A0;
const byte capacitorInput = A1;
const byte loadVoltageInput = A3;

const byte relayControl = 12;
const byte pwmControl = 5;
const byte irInput = 3;

const float capacitorBankFactor = 9.2;
const float capacitorFactor = 4.3;
const float loadFactor = 4.3;

const float referenceVoltage = 5.0;
const float maximumADC = 1023.0;

const float minimumOutput = 0.0;
const float maximumOutput = 4.3;

const float switchingLevel = 0.70;
const float pwmReference = 9.5;

const unsigned long updateInterval = 500;

float target = 0.0;

float totalCapVoltage = 0.0;
float singleCapVoltage = 0.0;
float outputVoltage = 0.0;

bool seriesMode = false;

int pwmValue = 0;

unsigned long previousUpdate = 0;

byte firstNumber = 255;
byte secondNumber = 255;

byte parallelIcon[8] =
{
  B00000,
  B01010,
  B01010,
  B11011,
  B11011,
  B01010,
  B01010,
  B00000
};

byte seriesIcon[8] =
{
  B00000,
  B01010,
  B01010,
  B11111,
  B11111,
  B01010,
  B01010,
  B00000
};

void setup()
{
  Serial.begin(9600);

  pinMode(capacitorBankInput, INPUT);
  pinMode(capacitorInput, INPUT);
  pinMode(loadVoltageInput, INPUT);

  pinMode(relayControl, OUTPUT);
  pinMode(pwmControl, OUTPUT);

  digitalWrite(relayControl, LOW);
  analogWrite(pwmControl, 0);

  display.init();
  display.backlight();

  display.createChar(0, parallelIcon);
  display.createChar(1, seriesIcon);

  display.clear();

  IrReceiver.begin(irInput);
}

float getVoltage(byte inputPin, float multiplier)
{
  const byte samples = 40;

  unsigned long total = 0;

  for (byte i = 0; i < samples; i++)
  {
    total += analogRead(inputPin);
    delayMicroseconds(80);
  }

  float average = (float)total / samples;

  float pinVoltage =
      average * referenceVoltage / maximumADC;

  return pinVoltage * multiplier;
}

void readSensors()
{
  totalCapVoltage =
      getVoltage(capacitorBankInput, capacitorBankFactor);

  singleCapVoltage =
      getVoltage(capacitorInput, capacitorFactor);

  outputVoltage =
      getVoltage(loadVoltageInput, loadFactor);
}

int identifyDigit(unsigned int command)
{
  const unsigned int remoteValues[10] =
  {
    12, 16, 17, 18, 20,
    21, 22, 24, 25, 26
  };

  for (byte i = 0; i < 10; i++)
  {
    if (remoteValues[i] == command)
    {
      return i;
    }
  }

  return -1;
}

void displayInput(float value)
{
  display.setCursor(0, 1);
  display.print("                ");

  display.setCursor(0, 1);
  display.print("Input = ");
  display.print(value, 1);
}

void readRemote()
{
  if (!IrReceiver.decode())
  {
    return;
  }

  int digit =
      identifyDigit(IrReceiver.decodedIRData.command);

  if (digit >= 0)
  {
    if (firstNumber == 255)
    {
      firstNumber = digit;

      displayInput(firstNumber);
    }
    else
    {
      secondNumber = digit;

      float newTarget =
          firstNumber + secondNumber * 0.1;

      target =
          constrain(newTarget, minimumOutput, maximumOutput);

      displayInput(target);

      firstNumber = 255;
      secondNumber = 255;
    }
  }

  IrReceiver.resume();
}

void chooseMode()
{
  float limit =
      maximumOutput * switchingLevel;

  bool requiredMode =
      target > limit;

  if (requiredMode != seriesMode)
  {
    seriesMode = requiredMode;

    digitalWrite(
      relayControl,
      seriesMode ? HIGH : LOW
    );
  }
}

int calculatePWM(float voltage)
{
  if (voltage <= 0.0)
  {
    return 255;
  }

  if (voltage >= pwmReference)
  {
    return 0;
  }

  float fraction =
      voltage / pwmReference;

  int result =
      255 - (int)(fraction * 255.0);

  return constrain(result, 0, 255);
}

void regulateVoltage()
{
  int basePWM =
      calculatePWM(target);

  float difference =
      target - outputVoltage;

  int correction =
      (int)(difference * 2.0);

  correction =
      constrain(correction, -10, 30);

  pwmValue =
      constrain(basePWM + correction, 0, 255);

  analogWrite(pwmControl, pwmValue);
}

void updateDisplay()
{
  display.setCursor(0, 0);

  if (seriesMode)
  {
    display.write(byte(1));
  }
  else
  {
    display.write(byte(0));
  }

  display.print(" ");
  display.print(outputVoltage, 2);
  display.print("V");

  display.setCursor(8, 0);
  display.print("MLANDU");

  display.setCursor(0, 1);
  display.print(totalCapVoltage, 2);
  display.print("V");

  display.setCursor(8, 1);
  display.print(" ");

  display.setCursor(9, 1);
  display.print(singleCapVoltage, 2);
  display.print("V  ");
}

void runSystem()
{
  readSensors();
  chooseMode();
  regulateVoltage();
  updateDisplay();
}

void loop()
{
  readRemote();

  unsigned long currentTime = millis();

  if (currentTime - previousUpdate >= updateInterval)
  {
    previousUpdate = currentTime;

    runSystem();
  }
}



