/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   cmu_main.cpp
 * Author: fabiuz
 *
 * Created on November 13, 2016, 5:08 PM
 */

#include <cstdlib>
#include <cstdio>
#include "miosix.h"
#include "interfaces/arch_registers.h"
#include "kernel/kernel.h"
#include "kernel/scheduler/timer_interrupt.h"

using namespace std;
using namespace miosix;

int value=-1,i=0;
Thread *tWaiting=nullptr;
/*
 * 
 */
int main(int argc, char** argv) {
    CMU->CALCTRL=CMU_CALCTRL_DOWNSEL_LFXO|CMU_CALCTRL_UPSEL_HFXO;
    //due to hardware timer characteristic, the real counter trigger at value+1
    //tick of LFCO to yield the maximun from to up counter
    CMU->CALCNT=700; 
    //enable interrupt
    CMU->IEN=CMU_IEN_CALRDY;
    NVIC_SetPriority(CMU_IRQn,3);
    NVIC_ClearPendingIRQ(CMU_IRQn);
    NVIC_EnableIRQ(CMU_IRQn);
    
    while(1){
        CMU->CMD=CMU_CMD_CALSTART;
        { 
            FastInterruptDisableLock dLock;
            tWaiting=Thread::IRQgetCurrentThread();
            Thread::IRQwait();
            {
                FastInterruptEnableLock eLock(dLock);
                Thread::yield();
            }
        }
        printf("Status: %lu, value=%d\n",CMU->STATUS,value);
    }
    return 0;
}

void __attribute__((naked)) CMU_IRQHandler()
{
    saveContext();
    asm volatile("bl _Z10cmuhandlerv");
    restoreContext();
}

void __attribute__((used)) cmuhandler(){
    greenLed::high();
    if(CMU->IF & CMU_IF_CALRDY){
        CMU->IFC=CMU_IFC_CALRDY | 0xFF;
        delayMs(i++);
        if(i>=400) i=0;
        greenLed::low();
        value=CMU->CALCNT;
        //Reactivating the thread that is waiting for the event.
        if(tWaiting){
            tWaiting->IRQwakeup();
            if(tWaiting->IRQgetPriority()>Thread::IRQgetCurrentThread()->IRQgetPriority())
                Scheduler::IRQfindNextThread();
            tWaiting=nullptr;
        }
    }
}