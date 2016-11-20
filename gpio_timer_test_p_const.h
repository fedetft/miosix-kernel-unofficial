/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   gpio_timer_test_const.h
 * Author: fabiuz
 *
 * Created on November 20, 2016, 5:19 PM
 */

#ifndef GPIO_TIMER_TEST_CONST_H
#define GPIO_TIMER_TEST_CONST_H

//Common
const long long timeout = 1000000000000LL;
const long long t10ms = 480000;
const long long t1ms = 48000;
const long long t1s = 48000000;
const long long noticeableValues[]={    0x00000001,
                                        0x00009C40,
                                        0x000000C8,
                                        0x0BEB0001,
                                        0x1000FFFF,
                                        0x1C00FFC0,
                                        0x37359400,
                                        0x5302D241,
                                        0x56E10001,
                                        0x5AA176C1,
                                        0x5CA10000,
                                        0x5FA176C1,
                                        0xFAFFC000,
                                        0x100000000,
                                        0x10A001FFF
};

//Master
const long long startMaster = 48000000;
const long long offsetBetweenPing = 1920000; //40ms

//Slave
const long long delay = 480000; //10ms


#endif /* GPIO_TIMER_TEST_CONST_H */

