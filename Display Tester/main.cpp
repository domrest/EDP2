#include <algorithm>
#include "mbed.h"
#include <list>
#include <stdlib.h>

 
using namespace std;



Ticker TimerInt;
AnalogIn Switch(PTB0);
AnalogIn Ain(PTB1);
AnalogOut Aout(PTE30);

#define max7219_reg_noop         0x00
#define max7219_reg_digit0       0x01
#define max7219_reg_digit1       0x02
#define max7219_reg_digit2       0x03
#define max7219_reg_digit3       0x04
#define max7219_reg_digit4       0x05
#define max7219_reg_digit5       0x06
#define max7219_reg_digit6       0x07
#define max7219_reg_digit7       0x08
#define max7219_reg_decodeMode   0x09
#define max7219_reg_intensity    0x0a
#define max7219_reg_scanLimit    0x0b
#define max7219_reg_shutdown     0x0c
#define max7219_reg_displayTest  0x0f

#define LOW 0
#define HIGH 1

SPI max72_spi(PTD2, NC, PTD1);
DigitalOut load(PTD0); //will provide the load signal


char  pattern_1[4] = { 0x40, 0x7c,0x00,0x0};
char  pattern_2[4] = { 0x5c, 0x54,0x74,0x0};
char  pattern_3[4] = { 0x54, 0x54,0x7c,0x0};
char  pattern_4[4] = { 0x70, 0x10,0x7c,0x0};
char  pattern_5[4] = { 0x74, 0x54,0x5c,0x0};
char  pattern_6[4] = { 0x7c, 0x54,0x5c,0x0};
char  pattern_7[4] = { 0x40, 0x40,0x7c,0x0};
char  pattern_8[4] = { 0x7c, 0x54,0x7c,0x0};
char  pattern_9[4] = { 0x70, 0x50,0x7c,0x0};
char  pattern_0[4] = { 0x7c, 0x44,0x7c,0x0};

char *nums[10] = {pattern_0,pattern_1,pattern_2,pattern_3,pattern_4,pattern_5,pattern_6,pattern_7,pattern_8,pattern_9};


char output[8] = {0x01, 0x01,0x01,0x01,0x01,0x01,0x01,0x01};
char deadOut[8] = {0x81, 0x42,0x24,0x18,0x18,0x24,0x42,0x81};
char randOut[8] = {0x80, 0x40,0x20,0x10,0x08,0x04,0x02,0x01};

float digAmpBuff[20];
int beatBuffer[5];


float previous[67];
float raw[20];

float changer = 0;

float prevValue = 0;
float lastPeak = 0;



list<float> buffer(300);



/*
Write to the maxim via SPI
args register and the column data
*/
void write_to_max( int reg, int col)
{
    load = LOW;            // begin
    max72_spi.write(reg);  // specify register
    max72_spi.write(col);  // put data
    load = HIGH;           // make sure data is loaded (on rising edge of LOAD/CS)
}

//writes 8 bytes to the display  
void pattern_to_display(char *testdata){
    int cdata; 
    for(int idx = 0; idx <= 7; idx++) {
        cdata = testdata[idx]; 
        write_to_max(idx+1,cdata);
    }
}

void setup_dot_matrix ()
{
    // initiation of the max 7219
    // SPI setup: 8 bits, mode 0
    max72_spi.format(8, 0);
     
  
  
       max72_spi.frequency(100000); //down to 100khx easier to scope ;-)
      

    write_to_max(max7219_reg_scanLimit, 0x07);
    write_to_max(max7219_reg_decodeMode, 0x00);  // using an led matrix (not digits)
    write_to_max(max7219_reg_shutdown, 0x01);    // not in shutdown mode
    write_to_max(max7219_reg_displayTest, 0x00); // no display test
    for (int e=1; e<=8; e++) {    // empty registers, turn all LEDs off
        write_to_max(e,0);
    }
   // maxAll(max7219_reg_intensity, 0x0f & 0x0f);    // the first 0x0f is the value you can set
     write_to_max(max7219_reg_intensity,  0x08);     
 
}

void checkDead(){
    bool dead = true;
    for (int i= 0; i<8; i++){
        if (output[i] > 0x01)
            dead = false;
    }
    
    if (dead){
        pattern_to_display(deadOut);
    }else
        pattern_to_display(output);
}


void read(){
    buffer.push_back(Ain.read());
    changer = Switch.read();

}


char hex(float num){
    
    if (num>0.21){
        PTB->PCOR = 0x80000; /* turn on green LED */
        PTB->PCOR = 0x040000;
        return 0xff;
    }else if (num>0.18){
        PTB->PCOR = 0x80000; /* turn on green LED */
        PTB->PCOR = 0x040000;
        return 0x7f;
    }else if (num>0.10){
        PTB->PCOR = 0x80000; /* turn on green LED */
        PTB->PCOR = 0x040000;
        return 0x3f;
    }else if (num>0.08){
        PTB->PCOR = 0x80000; /* turn on green LED */
        PTB->PCOR = 0x040000;
        return 0x1f;
    }else if (num>0.06){
        PTB->PCOR = 0x80000; /* turn on green LED */
        PTB->PCOR = 0x040000;
        return 0x0f;
    }else if (num>0.05){
        PTB->PCOR = 0x80000; /* turn on green LED */
        PTB->PCOR = 0x040000;
        return 0x07;
    }else if (num>0.025){
        PTB->PSOR = 0x80000; /* turn off green LED */
        PTB->PSOR = 0x040000;
        return 0x03;
    }else if (num>0.0){
        PTB->PSOR = 0x80000; /* turn off green LED */
        PTB->PSOR = 0x040000;
        return 0x01;
    }else{
        PTB->PSOR = 0x80000; /* turn off green LED */
        PTB->PSOR = 0x040000;
        return 0x00;
    }

}

float normalize(float value){
    float min = 1;
    for (int i = 0; i<19; i++){
        raw[i] = raw[i+1];
        if (raw[i]<min)
            min = raw[i];
    }
    raw[19] = value;
    if (value< min)
        min = value;
    value = value-min;
    return max<float>(0, value);
}
 

int calculateTime(int i, int j){
    int ticks = j-i;
    ticks = ticks * 90;
    int bpm = 60000/ticks;
    return bpm;
    
    
}

 
int beats(){
    int p1=0;
    float value=0;
    float prevValue=1;
    bool peak = false;
    for(int i=66; i>=0;i--){
        value = previous[i];
        if (value<0.05 && peak == true){
            value = min(value, prevValue);
            peak = false;
            if (p1==0) p1=i;
            else return calculateTime(i,p1);
        }else if (value<0.05){
            value = min(value, prevValue);
            peak = false;
        }else{
            if (peak == true){
                if (value>lastPeak){
                    peak = false;
                    value = max(value, prevValue);
                }else{
                    value = min(value, prevValue);
                }
            }else{
                if (prevValue>value){
                    peak = true;
                    lastPeak = prevValue;
                }
                value = max(value, prevValue);
            }
        }
        prevValue = value;
    }
    return 0;
} 
 

void wait(int n) {
    int i;
    int j;
    for(i = 0 ; i < n; i++)
        for (j = 0; j < 3500; j++) {}
} 

float digAmp(float value){
    bool low = true;
    
    for (int i = 0; i<19; i++){
        digAmpBuff[i] = digAmpBuff[i+1];
        if (digAmpBuff[i]>0.075) low = false;
    }
    
    digAmpBuff[19] = value;

    if (value<0.025) return value;
    if (low) value = value*2;
    if (low && value<0.03) value = value*4;
    return value;
    
}


int main()
{
    
    SIM->SCGC5 |= 0x400; /* enable clock to Port B */
    PORTB->PCR[19] = 0x100; /* make PTB19 pin as GPIO */
    PORTB->PCR[18] = 0x100;
    PTB->PDDR |= 0x40000; 
    PTB->PDDR |= 0x80000; /* make PTB19 as output pin */ 

    TimerInt.attach_us(&read, 1250); // setup ticker to call flip at 80Hz
    
    float avg = 0;
    setup_dot_matrix ();      /* setup matric */
    bool change = false;
    bool prevC = false;
    
    pattern_to_display(output);
    
    fill(raw, raw + 20, 1);
    while(1){  

        if (changer>0.5 && prevC == false ){change = false;}
            
        else if (changer>0.5 && prevC == true) { change = true;}
        
        if (buffer.size()>120)
            buffer.clear();
        
        
    
        if (change && buffer.size()>60){
            
            int size = 60;
            int size2 = size;
            avg = 0;
            // Pop from list and average the values
            while (size != 0){
                avg = avg +  buffer.front();
                buffer.pop_front();
                size --;
            }
            buffer.clear();
            avg = avg /(float) size2 * 1.5;
            
            // Normalize the value, removing the DC offset
            avg = normalize(avg); 
            avg = digAmp(avg);
            char line = hex(avg);
            // Add Value to array of 67 previous values
            
            
            for (int i = 0; i<67; i++){
                previous[i] = previous[i+1];
            }
            previous[66] = avg;
            
            // Give value to display moving the display forwards
            int peaks = beats();
            
            int num1 = peaks/10;
            
            if (num1>9){
                num1 = 9;
            }
            int num2 = peaks%10;
            
            for (int e=0; e<4; e++){
                output[e] = nums[num1][e];
            }
            for (int e=4; e<8; e++){
                output[e] = nums[num2][e%4];
            }
            
            pattern_to_display(output);

            PTB->PSOR = 0x80000; /* turn off green LED */

            
        }
        
        // Check for 60 values taken
        else if (buffer.size()>60){

            int size = 60;
            int size2 = size;
            avg = 0;
            // Pop from list and average the values
            while (size != 0){
                avg = avg +  buffer.front();
                buffer.pop_front();
                size --;
            }
            buffer.clear();
            avg = avg /(float) size2 * 1.5;
            
            // Normalize the value, removing the DC offset
            avg = normalize(avg);
            avg = digAmp(avg);
            
            // Add Value to array of 40 previous values
            for (int i = 0; i<67; i++){
                previous[i] = previous[i+1];
            }
            previous[66] = avg;
            
            // Give value to display moving the display forwards
            char line = hex(avg);
            for (int i = 0; i<8; i++){
                output [i]=output[i+1];
            }
            output [7]=line;

            
            checkDead();
            
            // Output to display
            //pattern_to_display(output);
            
        }
        
                
        else{
            wait(1);
            if (changer<0.5 && change == true){prevC = false;}
            
            if (changer<0.5 && change == false){prevC = true;}
        }     
    }
}
