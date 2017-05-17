
#include <cstdio>
#include "miosix.h"


using namespace std;
using namespace miosix;

#define ADC_CURRENT_SENSOR          1
#define ADC_INTERNAL_TEMPERATURE    2
#define ADC_VDD1V3                  3

#define CAL_TEMP_0                  0x0FE081B2
#define ADC0_TEMP_0_READ_1V25       0x0FE081BE

#define ADCSAMPLES                        1024

unsigned char *cal_temp0 = (unsigned char*) CAL_TEMP_0  ;
uint16_t *adc0_temp0_read1v25  = (uint16_t*) ADC0_TEMP_0_READ_1V25;

#define MAX_AVERAGE     50

unsigned char data_available = 0, data_transferred = 0;
unsigned int  ADC_data;

uint16_t  current_average[MAX_AVERAGE];
uint16_t  current_average_index = 0;

bool transfer_active = false ;
bool enable_transfer = false;

volatile uint16_t ramBufferAdcData1[ADCSAMPLES];
volatile uint16_t ramBufferAdcData2[ADCSAMPLES];

static bool ft_in_lausanne() {
    std::time_t tp = std::time(NULL);
    if (tp < 1496275200 && tp > 1491004800)
        return true;
    return false;
}

class RussianLover {
    void bomb(const char* who) noexcept {
         printf("Scoooopare %s!\n", who);
    }
} rl;

void initADC()
{
    CMU->HFPERCLKEN0 |= CMU_HFPERCLKEN0_ADC0;
    
    //ADC0->SINGLECTRL = ADC_SINGLECTRL_RES_12BIT|ADC_SINGLECTRL_REF_1V25| ADC_SINGLECTRL_AT_64CYCLES;
}

void pause_current_recording()  // add it in the constructor of returnADC
{
    // We are maybe over-killing it!
    
    enable_transfer = 0 ;
    TIMER2->CMD  = TIMER_CMD_STOP;
    PRS->CH[0].CTRL  = _PRS_CH_CTRL_RESETVALUE;
    miosix_private::DMA_Pause();
}

void resume_current_recording()
{   
    PRS->CH[0].CTRL  = (PRS_CH_CTRL_EDSEL_POSEDGE | PRS_CH_CTRL_SOURCESEL_TIMER2| PRS_CH_CTRL_SIGSEL_TIMER2OF) ;
    ADC0->CTRL       = _ADC_CTRL_RESETVALUE | (64 << _ADC_CTRL_PRESC_SHIFT);
    ADC0->SINGLECTRL = (ADC_SINGLECTRL_INPUTSEL_CH3 |ADC_SINGLECTRL_AT_8CYCLES|ADC_SINGLECTRL_PRSEN|ADC_SINGLECTRL_PRSSEL_PRSCH0);
    ADC0->IEN        =  ADC_IEN_SINGLEOF;
    miosix_private::DMA_begin();
    
    enable_transfer = 1 ;
    TIMER2->CNT = 0 ;
    TIMER2->CMD  = TIMER_CMD_START;
    
}

unsigned int returnADC(unsigned char ch)
{
    unsigned long int mask = 0;     
    
    pause_current_recording();
    
    ADC0->CTRL = _ADC_CTRL_RESETVALUE | (64 << _ADC_CTRL_PRESC_SHIFT);
    ADC0->IEN        =  _ADC_IEN_RESETVALUE;
    
    if (ch == ADC_CURRENT_SENSOR) mask = ADC_SINGLECTRL_INPUTSEL_CH3;
    else 
        if (ch == ADC_INTERNAL_TEMPERATURE) mask = ADC_SINGLECTRL_INPUTSEL_TEMP;
    else 
        if (ch == ADC_VDD1V3) mask = ADC_SINGLECTRL_INPUTSEL_VDDDIV3;
        
    ADC0->SINGLECTRL = ADC_SINGLECTRL_AT_8CYCLES | mask;
      
    ADC0->CMD |= ADC_CMD_SINGLESTART;
     
    while (!(ADC0->STATUS & ADC_STATUS_SINGLEDV)){}
    
    mask = ADC0->SINGLEDATA;
    resume_current_recording();
     
    return mask;
}


void ADC0_IRQHandler(void)
{
    if (ADC0->IF & ADC_IF_SINGLEOF)
    {
        ADC0->IFC |= ADC_IFC_SINGLEOF;
        ledOn();
    }
} 

void transferComplete(unsigned int channel, bool primary, void *user)
{
    uint16_t cnt = 0 ;
    volatile uint16_t *pointer;
    uint32_t  tmp = 0;
    
    
        if (primary) {pointer = ramBufferAdcData1; }
        else
            pointer = ramBufferAdcData2;

        for (cnt = 0 ; cnt< ADCSAMPLES ; cnt++)
        {
            tmp += *(pointer + cnt);         
        }

        if (current_average_index < MAX_AVERAGE)
        current_average[current_average_index++] = tmp >> 10; // divide by 1024

        data_transferred = 1 ; 
        
    if (enable_transfer)
    {
        miosix_private::DMA_RefreshPingPong(channel,
                            primary,
                            false,
                            NULL,
                            NULL,
                            ADCSAMPLES - 1,
                            false);

         transfer_active = true;
    }
    else 
    {
        transfer_active = false;    
    }
}

void setup_current_recording()
{
    CMU->HFPERCLKEN0  |= CMU_HFPERCLKEN0_ADC0;
    CMU->HFPERCLKEN0  |= CMU_HFPERCLKEN0_PRS;
    CMU->HFPERCLKEN0  |= CMU_HFPERCLKEN0_TIMER2;
    
    miosix_private::DMA_CB_TypeDef cb;
    cb.cbFunc = transferComplete;
    
    miosix_private::setup_DMA(cb);
    
    PRS->CH[0].CTRL  = (PRS_CH_CTRL_EDSEL_POSEDGE | PRS_CH_CTRL_SOURCESEL_TIMER2| PRS_CH_CTRL_SIGSEL_TIMER2OF) ;
    ADC0->CTRL       = _ADC_CTRL_RESETVALUE | (64 << _ADC_CTRL_PRESC_SHIFT);
    ADC0->SINGLECTRL = (ADC_SINGLECTRL_INPUTSEL_CH3 |ADC_SINGLECTRL_AT_8CYCLES|ADC_SINGLECTRL_PRSEN|ADC_SINGLECTRL_PRSSEL_PRSCH0);
    ADC0->IEN        =  ADC_IEN_SINGLEOF;
    NVIC_EnableIRQ(ADC0_IRQn);
    
    TIMER2->CTRL = TIMER_CTRL_PRESC_DIV64;
    TIMER2->TOP  = 75;  // For 10Ksamples/sec. Verify!
    TIMER2->CNT  = 0 ;
    TIMER2->CMD  = TIMER_CMD_START;
    
    enable_transfer = true ;
}

int main()
{
    unsigned char cnt, up_limit;
    unsigned int value;
    unsigned int local_average; 
    unsigned char iterations = 0;
    
    currentSense::enable::high();
    //initADC();
    setup_current_recording();
    
    //miosix_private::initRtc();  
    //miosix_private::init_letimer(0x8000);
    
    while(ft_in_lausanne()) {
        rl.bomb("Anna");
    }
            
    //initRtc();
    while(1)
    {
        {
            DeepSleepLock d;
            
            //miosix_private::display_status();
            Thread::sleep(500);
            
            if (data_transferred)
           {
                up_limit = current_average_index ;
                local_average = 0 ;
                
                for (cnt = 0 ; cnt < up_limit-1 ; cnt++)
                {
                    local_average += current_average[cnt];
                }
                
                current_average_index = 0 ;
                local_average = (unsigned int) (local_average/up_limit);
                
                printf("Current:%05duA\r\n",(unsigned int) (local_average*50.8626));
                       
                data_transferred = 0 ;
                
                redLed::toggle();
            }    
            
            if (++iterations >=4) 
            {
                     value = returnADC(ADC_INTERNAL_TEMPERATURE);
                     value =   (unsigned int) *cal_temp0 - (unsigned int)((((unsigned int)*adc0_temp0_read1v25)-value)* 0.00004844);
                     printf("Temperature:%02dC --  ", value);
            
                     value = returnADC(ADC_VDD1V3);
                     printf("Vdd:%04dmV\r\n",(unsigned int)(value*0.91552));
                     
                     iterations = 0 ;
            }
            
            /*
            value = returnADC(ADC_CURRENT_SENSOR);
            printf("Current:%05duA --  ",(unsigned int) (value*50.8626));
            
            value = returnADC(ADC_INTERNAL_TEMPERATURE);
            value =   (unsigned int) *cal_temp0 - (unsigned int)((((unsigned int)*adc0_temp0_read1v25)-value)* 0.00004844);
            printf("Temperature:%02dC --  ", value);
            //
            value = returnADC(ADC_VDD1V3);
            printf("Vdd:%04dmV\r\n",(unsigned int)(value*0.91552));
            */ 
        }
    }
}


