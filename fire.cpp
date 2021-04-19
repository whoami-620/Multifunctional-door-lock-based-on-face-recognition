//
// Created by fanzou
//
#include "fire.h"
#include <wiringPi.h>
#include <stdio.h>
#include <sys/time.h>

#define FIRE 5
#define VOICE 4


void doVoice(void)// make warning
{
    
    wiringPiSetup();
    pinMode(VOICE,OUTPUT); 
    
    digitalWrite(VOICE,LOW); 
    for(int i=1;i<=6;i++){
        digitalWrite(VOICE,HIGH); 
        delay(500);
        digitalWrite(VOICE,LOW); 
        delay(1000);
    }
    digitalWrite(VOICE,LOW);
}

int fireDetection(void)// start fire Detection
{

    wiringPiSetup();
    pinMode(FIRE, INPUT);     
    

        if (digitalRead(FIRE) == 1)
        {

        }
        else
        {
            printf("fire detected\n");
            doVoice();  

        }

        
    return 0;
}

