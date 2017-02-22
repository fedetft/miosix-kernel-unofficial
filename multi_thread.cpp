/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   multi_thread.cpp
 * Author: fabiuz
 *
 * Created on February 22, 2017, 4:53 PM
 */

#include <cstdlib>
#include <stdio.h>
#include "miosix.h"
using namespace std;
using namespace miosix;

/*
 * Test if multi thread sleep works
 */


void t1(void*){
    for(;;){
        Thread::sleep(500);
        printf("T1\n");
    }
}

void t2(void*){
    for(;;){
        Thread::sleep(333);
        printf("T2\n");
    }
}
int main(int argc, char** argv) {
    Thread::create(t1,2048,3);
    Thread::create(t2,2048,3);
    
    Thread::sleep(1000000000);
    return 0;
}

