#include<xc.h>
#include "mcc_generated_files/mcc.h"
#include "FT800.h"
#include "display.h"
#include "mcc_generated_files/ecan.h"

#define FT_ACTIVE  0x00
#define FT_STANDBY 0x41
#define FT_SLEEP   0x42
#define FT_PWRDOWN 0x50
#define FT_CLKEXT  0x44
#define FT_CLK48M  0x62
#define FT_CLK36M  0x61
#define FT_CORERST 0x68
#define UP_SHIFT_RPM 9000

uCAN_MSG canMessage;
bool refresh_screen = false;
    bool up_blink = false;

typedef struct {
    int current, current_number, current_int, current_dec, last, last_int, last_dec, best, best_number, best_int, best_dec;
} Lap_time;

void wait2secs(){
    __delay_ms(2000);
}

void refresh() {
    refresh_screen = true;
}

void LED_blink() {
    INTERRUPT_PeripheralInterruptDisable();
    up_shift_LAT = !up_shift_GetValue();
    down_shift_LAT = !down_shift_GetValue();
    INTERRUPT_PeripheralInterruptEnable();
}

void main(void) {   
    Lap_time lap_time= {0,1,0,0,0,0,0,9999,0,99,99};
    bool timer_started = false;
    char* message = "READY";
    int rpm = 0, oilP = 0, fuelP = 0, tp = 0, speed = 0, gear = 0, engTemp = 0, oilTemp = 0, battVolts = 0;
    wait2secs(); 
    
    // Initialize the device
    SYSTEM_Initialize();
    
    // CAN Configuration
    CIOCONbits.CLKSEL = 1;
    CIOCONbits.ENDRHI = 1;
    CIOCONbits.TX2SRC = 0;
    CIOCONbits.TX2EN = 1; 
    RXB0CONbits.RXM0 = 1;
    RXB0CONbits.RXM1 = 0; //receive only standard messages
    RXB0CONbits.RB0DBEN = 0;
    PIE5bits.RXB0IE = 0;
    PIE5bits.RXB1IE = 0;
    PIR5bits.ERRIF = 0;
    PIR5bits.WAKIF = 0;
    
    // CAN FILTER CONFIG
    // RXF5 & 6 not available in mode 0
//    RXF4SIDH = 0xC8;
//    RXF4SIDL = 0x00; //Filter for ID 0x640
//    RXF3SIDH = 0xC8;
//    RXF3SIDL = 0x20; //Filter for ID 0x641
    RXF2SIDH = 0xC8;
    RXF2SIDL = 0x40; //Filter for ID 0x642
    RXM0SIDH = 0xFF;
    RXM0SIDL = 0b11011111;
    RXM1SIDH = 0xFF;
    RXM1SIDL = 0x11011111;
    
    // SPI Configuration for LCD
    SSPSTATbits.SMP = 0;
    SSPSTATbits.CKE = 1;
    SSPCON1bits.SSPEN = 1;
    SSPCON1bits.CKP = 1;
    SSPCON1bits.SSPM = 0b0010; //FOSC/64    
    FT800_Init();
     
    wait2secs(); 
    SSPCON1bits.SSPM = 0b0001; //FOSC/16  

    up_shift_SetLow();
    down_shift_SetLow();
    warning_1_SetLow();
    warning_2_SetHigh();
    warning_3_SetLow();
    warning_4_SetHigh();
    
    display(rpm, oilP, fuelP, tp, speed, gear, engTemp, oilTemp, battVolts);
    
    TMR1_SetInterruptHandler(&refresh); 
    TMR0_SetInterruptHandler(*LED_blink);
    TMR0_StopTimer();
    INTERRUPT_GlobalInterruptEnable();
    INTERRUPT_PeripheralInterruptEnable();
    
    while (1) {  
        if (CAN_receive(&canMessage)) {
            if (canMessage.frame.id == 0x640) {
                rpm = ((canMessage.frame.data0 << 8) | canMessage.frame.data1);
                oilP = ((canMessage.frame.data2 << 8) | canMessage.frame.data3) / 10;
                fuelP = ((canMessage.frame.data4 << 8) | canMessage.frame.data5) / 10;
                tp = canMessage.frame.data6;
                speed = canMessage.frame.data7;
            } else if (canMessage.frame.id == 0x641) {
                gear = canMessage.frame.data6;
            } else if (canMessage.frame.id == 0x642) {
                engTemp = canMessage.frame.data0;
                oilTemp = canMessage.frame.data1;
                battVolts = canMessage.frame.data2;
            } else if (canMessage.frame.id == 0x643) {
                if(canMessage.frame.data0 >> 7) {
                    *message = "Radio ON";
                } else if(canMessage.frame.data0 >> 6) {
                    *message = "DRS & AutoShift ON";
                } else {
                    *message = "R18 ";
                } 
                warning_1_LAT = canMessage.frame.data0 >> 5; //coolant temp
                warning_2_LAT = canMessage.frame.data0 >> 4; // oil temp
                warning_4_LAT = canMessage.frame.data0 >> 3; // oil pressure
                warning_3_LAT = canMessage.frame.data0 >> 2; // fuel pressure

            } else if (canMessage.frame.id == 0x644) {
                lap_time.last = ((canMessage.frame.data0 << 8) | canMessage.frame.data1);
                lap_time.current = ((canMessage.frame.data2 << 8) | canMessage.frame.data3);
                lap_time.current_number = ((canMessage.frame.data4 << 8) | canMessage.frame.data5);
                lap_time.last_int = lap_time.last/100;
                lap_time.last_dec = lap_time.last%100;
                lap_time.current_int = lap_time.current/100;
                lap_time.current_dec = lap_time.current%100;
                if(lap_time.current < lap_time.best) {
                    lap_time.best = lap_time.current;
                    lap_time.best_int = lap_time.current_int;
                    lap_time.best_dec = lap_time.current_dec;
                }
            }
        }
        if(refresh_screen) {
            display_start();
            display_labels();
            display_waterTemp(engTemp);
            display_oilTemp(oilTemp);
            display_fuel(4.3);
            display_battery(battVolts);
            display_oilPress(oilP);
            display_gear(gear);
            display_rpm(rpm);
            display_speed(speed);
            display_tp(tp);
            display_laptime(lap_time.current_int, lap_time.current_dec, lap_time.best_int, lap_time.best_dec,
                             lap_time.last_int, lap_time.last_dec, lap_time.current_number, lap_time.best_number);
            display_message(message);
            display_end();
            if(rpm == 0 && !timer_started) {
                TMR0_StartTimer();
                timer_started = true;
            } else {
                TMR0_StopTimer();
                timer_started = false;
                up_shift_SetLow();
                down_shift_SetLow();
            }
            refresh_screen = false;
            TMR1_Reload();
        }
    }
}


/**
 End of File
*/