#define NOISE_TAPPED 0x9
#define NOISE_SR_WIDTH 15

const unsigned char volTable[] = {
  250,
  198,
  157,
  125,
  99,
  79,
  62,
  49,
  39,
  31,
  25,
  19,
  15,
  12,
  9,
  0
};

const unsigned char ledPins[] = {4, 5, 6, 7};

short freq[4];
short count[4];
short attn[4];
unsigned short noiseSr = 0x8000;
bool flipflop[4];

unsigned char currReg;
unsigned char currType;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(3, OUTPUT);
  for(char i = 0; i < 4; i++) {
    pinMode(ledPins[i], OUTPUT);
  }
  cli();

  TCCR1A = 0;
  TCCR1B = 0;
  OCR1A = 576;
  TCCR1B |= (1 << WGM12);
  TCCR1B |= (1 << CS10); //16mHz/64Hz = 250kHz -> roughly equal to divided psg frequency
  TIMSK1 |= (1 << OCIE1A);

  TCCR2B = TCCR2B & 0b11111000 | 0x01; //set pwm on timer2 to max frequency
  sei();
  Serial.println("Initialized!");
  for(char i = 0; i < 4; i++){
    attn[i] = 15;
  }
}

void psgWrite(unsigned char data){
  bool first = (data & 128) != 0;
  if(first) {
    currReg = (data >> 5) & 3;
    currType = (data >> 4) & 1;
  }

  if(currType != 0) {
    attn[currReg] = data & 0x0f;
    digitalWrite(ledPins[currReg], attn[currReg] > 7 ? LOW : HIGH);
  } else if(first && currReg == 3) {
    freq[3] = data & 7;
    noiseSr = 1 << NOISE_SR_WIDTH;
  } else if(first) {
    freq[currReg] = (freq[currReg] & 0x3f0) | (data & 0x0f);
  } else {
    freq[currReg] = (freq[currReg] & 0x0f) | ((data &0x3f) << 4);
  }
}

int noiseParity(int val) {
  val ^= val >> 8;
  val ^= val >> 4;
  val ^= val >> 2;
  val ^= val >> 1;
  return val & 1;
}

inline void updateChannel(const unsigned char i, unsigned int& output) {
  count[i] -= 8;
  if(count[i] <= 0) {
    count[i] += freq[i];
    flipflop[i] = !flipflop[i];
  }

  output += flipflop[i] ? volTable[attn[i]] : 0;
  
}

byte incomingByte;
unsigned int output = 0;
ISR(TIMER1_COMPA_vect) {
  output = 0;
  updateChannel(0, output);
  updateChannel(1, output);
  updateChannel(2, output);
  
  count[3]-= 8;
  if(count[3] <= 0) {
    int nf = freq[3] & 0x03;
    int fb = (freq[3] >> 0x02) & 0x01;
    count[3] += nf == 3 ? freq[2] : (0x10 << nf);

    noiseSr = (noiseSr >> 1) | ((fb == 1 ? noiseParity(noiseSr & NOISE_TAPPED) : (noiseSr & 0x1)) << NOISE_SR_WIDTH);
    flipflop[3] = (noiseSr & 0x01) != 0;
    output += flipflop[3] ? volTable[attn[3]] : 0;
  }
}

void loop() {
  if (Serial.available() > 0) {
    incomingByte = Serial.read();
    psgWrite(incomingByte);
    //Serial.println(incomingByte);
  }

  analogWrite(3, output / 4);
}
