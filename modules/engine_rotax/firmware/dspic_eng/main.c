#include "xc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "config_bits.h"
#include "led.h"
#include "ecan_mod.h"
#include "relay.h"
#include "rpm.h"
#include "fuelflow.h"
#include "analog.h"
#include "thermocouple.h"
#include "modair_bus.h"
#include "params.h"
#include "module_console.h"

//==============================================================================
//--------------------GLOBAL VARIABLES------------------------------------------
//==============================================================================
//s_param_settings PARAM_LIST[PARAM_CNT];
u16 rate_cnt[PARAM_CNT];

//==============================================================================
//--------------------GLOBAL CONSTANTS------------------------------------------
//==============================================================================
// TODO: make this constant is stored in flash (such that it is rewritable)
__attribute__((aligned(FLASH_PAGE_SIZE))) const s_param_settings PARAM_LIST[PARAM_CNT] = {
    {.pid=0xFF02, .name="Rotax582", .rate=0}, // MODULE
    {.pid=0x0010, .name="BUS VOLT", .rate=0x20},
    {.pid=0x0011, .name="EGT 1   ", .rate=0x20},
    {.pid=0x0012, .name="EGT 2   ", .rate=0x20},
    {.pid=0x0013, .name="RPM     ", .rate=0x20},
    {.pid=0x0014, .name="ENG HRS ", .rate=0x20},
    {.pid=0x0015, .name="ENG ON  ", .rate=0x20},
    {.pid=0x0016, .name="MAINTAIN", .rate=0x20},
    {.pid=0x0017, .name="FUEL LVL", .rate=0x20},
    {.pid=0x0018, .name="H2O TEMP", .rate=0x20},
    {.pid=0x0019, .name="RELAY   ", .rate=0x20},
    {.pid=0x001A, .name="OD1 OUT ", .rate=0x20},
    {.pid=0x001B, .name="OD2 OUT ", .rate=0x20}
};

const s_param_const PARAM_CONST[PARAM_CNT] = {
    {.canrx_fnc_ptr=&module_ecanrx, .cntdwn_fnc_ptr=0}, // MODULE
    {.canrx_fnc_ptr=0, .cntdwn_fnc_ptr=0},
    {.canrx_fnc_ptr=0, .cntdwn_fnc_ptr=0},
    {.canrx_fnc_ptr=0, .cntdwn_fnc_ptr=0},
    {.canrx_fnc_ptr=0, .cntdwn_fnc_ptr=0},
    {.canrx_fnc_ptr=0, .cntdwn_fnc_ptr=0},
    {.canrx_fnc_ptr=0, .cntdwn_fnc_ptr=0},
    {.canrx_fnc_ptr=0, .cntdwn_fnc_ptr=0},
    {.canrx_fnc_ptr=0, .cntdwn_fnc_ptr=0},
    {.canrx_fnc_ptr=0, .cntdwn_fnc_ptr=0},
    {.canrx_fnc_ptr=0, .cntdwn_fnc_ptr=0},
    {.canrx_fnc_ptr=0, .cntdwn_fnc_ptr=0},
    {.canrx_fnc_ptr=0, .cntdwn_fnc_ptr=0}
};

//==============================================================================
//--------------------INTERRUPTS------------------------------------------------
//==============================================================================
void __attribute__((interrupt, auto_psv)) _T1Interrupt(void)
{
    u8 i;
    for (i=1;i<PARAM_CNT;i++)
        if (rate_cnt[i])
            rate_cnt[i]--;
    _T1IF = 0;  // clear the interrupt
}

void __attribute__((interrupt, auto_psv)) _CNInterrupt(void)
{
    fuelflow_irq();
    rpm_irq();
    _CNIF = 0;  // clear the interrupt
}

void __attribute__((interrupt, auto_psv)) _AD1Interrupt(void)
{
    analog_irq();
    _AD1IF = 0;  // clear the interrupt
}

//==============================================================================
//--------------------INIT FUNCTIONS--------------------------------------------
//==============================================================================
void tmr1_init(u16 freq_hz)
{
    T1CON = 0b1000000000110000; // TMR1 on, 1:256 prescale, Fosc/2
    PR1 = F_CY/256/freq_hz;
}

void irq_init(void)
{
    _AD1IF = 0; // ADC1 Event Interrupt Flag
    _AD1IP = 1; // lowest priority level
    _AD1IE = ENABLE; // ADC1 Event Interrupt Enable

    _T1IF = 0; // Timer1 Flag
    _T1IP = 2; // second lowest priority
    _T1IE = ENABLE; // timer1 interrupt enable

    _CNIF = 0; // ChangeNotification Flag
    _CNIP = 3; // third lowest priority level
    _CNIE = ENABLE; // change notification interrupt enable
}

//==============================================================================
//--------------------MAIN LOOP-------------------------------------------------
//==============================================================================
int main(void)
{
    u8 i;
    OSCTUN = 0; // 7.37 MHz
    CLKDIV = 0; // N1=2, N2=2
    PLLFBD = 63; // M=65
    // Fosc = 7.37*M/(N1*N2) = 119.7625 MHz
    // Fcy  = Fosc/2 = 59.88125 MIPS
    while (OSCCONbits.LOCK!=1){}; // Wait for PLL to lock

    // Call Parameter Init functions
    module_init();
    led_init();
    ecan_init();
    thermocouple_init();
    relay_init();
    fuelflow_init();
    rpm_init();
    analog_init();
    tmr1_init(50); // 50 Hz == 20 ms ticks
    irq_init();

    while(1)
    {
        // handle CAN receive messages here
        ecan_process();

        // handle TMR timeout function calls here
        for (i=1;i<PARAM_CNT;i++)
            if ((rate_cnt[i]==0)&&(PARAM_LIST[i].rate)) {
                if (PARAM_CONST[i].cntdwn_fnc_ptr) // if timeout fnc exists
                    PARAM_CONST[i].cntdwn_fnc_ptr(i);
                rate_cnt[i] = PARAM_LIST[i].rate; // reload countdown value
            }
        // Bus Voltage
        // Thermocouple 1
        // Thermocouple 2
        // RPM
        // Engine Hours / Hobbs Meter
        // Engine On Time since started
        // Maintenance Timer
        // Fuel Level
        // Water Temperature

        // Relay Output
        // Open Drain 1 Output
        // Open Drain 2 Output

        // Fuel Flow Instantaneous
        // Fuel Flow Average since started
        // Time to Empty Tank (fuel endurance)
        // Range to Empty Tank (fuel range)
        // Fuel Burned
    }
}
