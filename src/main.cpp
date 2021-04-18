//
// Edited by Wenyi Li 
//
#include "Face_recognition.hpp"
#include <stdio.h>
#include <iostream>
#include "wiringPi.h"
#include "dht11.cpp"



using namespace std;
using namespace cv;


int main() {
                init_read_temp();
	        read_temp();
		Detection();

}
