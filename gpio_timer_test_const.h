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

//Master
const long long startMaster = 48000000;
const long long offsetBetweenPing = 1920000; //40ms

//Slave
const long long delay = 480000; //10ms


#endif /* GPIO_TIMER_TEST_CONST_H */

