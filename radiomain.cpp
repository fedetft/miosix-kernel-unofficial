
#include <cstdio>
#include <sys/ioctl.h>
#include "miosix.h"

#define private public //FIXME: hack
#include "cc2520.h"
#undef private

#include "SPI.h"

using namespace std;
using namespace miosix;

void RTC_IRQHandler()
{
    RTC->IFC=RTC_IFC_COMP0;
}

void waitRtc(int val)
{
    RTC->COMP0=(RTC->CNT+val) & 0xffffff;
    while(RTC->SYNCBUSY & RTC_SYNCBUSY_COMP0) ;
    
    {
        FastInterruptDisableLock dLock;
        //FIXME: fix race condition
        RTC->IEN |= RTC_IEN_COMP0;
        SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
        EMU->CTRL=0;
        __WFI();
        SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
        RTC->IEN &= ~RTC_IEN_COMP0;
        //oscillatorInit(); //FIXME
    }
}

void initRtc()
{
    FastInterruptDisableLock dLock;
    CMU->HFCORECLKEN0 |= CMU_HFCORECLKEN0_LE; //Enable clock to low energy peripherals
    CMU->LFACLKEN0 |= CMU_LFACLKEN0_RTC;
    while(CMU->SYNCBUSY & CMU_SYNCBUSY_LFACLKEN0) ;
    
    RTC->CNT=0;
    
    RTC->CTRL=RTC_CTRL_EN;
    while(RTC->SYNCBUSY & RTC_SYNCBUSY_CTRL) ;
    
    NVIC_EnableIRQ(RTC_IRQn);
}

int main()
{
//     //DEEP SLEEP test for current consumption
//     ioctl(STDOUT_FILENO,IOCTL_SYNC,0);
//     CMU->ROUTE&= ~CMU_ROUTE_CLKOUT1PEN; //Disable 32KHz output
//     initRtc();
//     for(;;) waitRtc(0xffffff);    

     //transceiver packet RX test
     currentSense::enable::high();
     SPI spi;
     Cc2520 cc2520(spi,
                   transceiver::cs::getPin(),
                   transceiver::reset::getPin(),
                   transceiver::vregEn::getPin());
     cc2520.setFrequency(2450);
     cc2520.setMode(Cc2520::RX);
     cc2520.writeReg(Cc2520::CC2520_FREQTUNE,13); //Important!
     cc2520.setAutoFCS(false);
     unsigned char *packet=new unsigned char[128];
     unsigned char length;
     
     int i=0;
     int lost=0;
     for(;;)
     {
         if(cc2520.isRxFrameDone() == 1)
         {
             ledOn();
             length=127; //FIXME: with 128 fails, because max packet is 127
             cc2520.readFrame(length,packet);
             miosix::memDump(packet,length);
             ledOff();
             int j;
             if(siscanf(reinterpret_cast<char*>(packet),"%d",&j)==1)
             {
                 lost+=(j-i-1);
                 i=j;
             }
             iprintf("Lost %d packets\n",lost);
         } else Thread::sleep(10);
     }


//     //transceiver packet TX test
//     currentSense::enable::high();
//     SPI spi;
//     Cc2520 cc2520(spi,
//                   transceiver::cs::getPin(),
//                   transceiver::reset::getPin(),
//                   transceiver::vregEn::getPin());
//     cc2520.setFrequency(2450);
//     cc2520.setMode(Cc2520::TX);
//     cc2520.writeReg(Cc2520::CC2520_FREQTUNE,13); //Important!
//     cc2520.setAutoFCS(true);
//     
//     for(int i=0;;i++)
//     {
//         greenLed::high();
//         char frame[32];
//         siprintf(frame,"%d Test message",i);
//         cc2520.writeFrame(strlen(frame),reinterpret_cast<unsigned char *>(frame));
//         transceiver::stxon::high();
//         delayUs(1);
//         transceiver::stxon::low();
//         Thread::sleep(10); //FIXME: handle events better
//         cc2520.isSFDRaised();
//         cc2520.isTxFrameDone();
//         greenLed::low();
//         Thread::sleep(1000);
//     }

}
