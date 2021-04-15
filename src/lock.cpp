//
// Created by fanzou
//

#include "lock.h"
#include <stdio.h>
#include <wiringPi.h>
#define LOCK 27

using namespace std;

int Open()
{
        
        wiringPiSetup();  
        pinMode(LOCK, OUTPUT);  
		digitalWrite(LOCK, HIGH);
        delay(500);
        digitalWrite(LOCK, LOW);
        return 0;
}


int close()
{
        
        wiringPiSetup();  
        pinMode(LOCK, INPUT);  
        return 0;
}

