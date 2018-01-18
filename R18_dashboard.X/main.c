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

uCAN_MSG canMessage;
bool can_msg_received = false;
bool refresh_screen = false;

void wait2secs(){
    __delay_ms(2000);
}

//void ECAN_ISR_ECAN_RXBI(void) {
//    PIE5bits.RXB0IE = 0;
//    if(CANSTATbits.ICODE2 & CANSTATbits.ICODE1 & !CANSTATbits.ICODE0) {
//        can_msg_received = true;
//        down_shift_SetHigh();
//    } else {
//        PIE5bits.RXB0IE = 1;
//    }
//    // Not supported yet
//    // clear the ECAN interrupt flag
//    PIR5bits.RXB0IF = 0;
//}

void refresh() {
    refresh_screen = true;
}

void main(void) {   
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
    RXF4SIDH = 0xC8;
    RXF4SIDL = 0x00; //Filter for ID 0x640
    RXF3SIDH = 0xC8;
    RXF3SIDL = 0x20; //Filter for ID 0x641
    RXF2SIDH = 0xC8;
    RXF2SIDL = 0x40; //Filter for ID 0x642
    
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
    
    INTERRUPT_GlobalInterruptEnable();
    INTERRUPT_PeripheralInterruptEnable();
    
    while (1) {  
        if (CAN_receive(&canMessage)) {
            if (canMessage.frame.id == 0x640) {
                rpm = ((canMessage.frame.data0 << 8) | canMessage.frame.data1);
                oilP = ((canMessage.frame.data2 << 8) | canMessage.frame.data3) / 10;
                fuelP = ((canMessage.frame.data0 << 4) | canMessage.frame.data5);
                tp = canMessage.frame.data6;
                speed = canMessage.frame.data7;
            } else if (canMessage.frame.id == 0x641) {
                //oilTemp = canMessage.frame.data0;
                gear = canMessage.frame.data6;
            } else if (canMessage.frame.id == 0x642) {
                engTemp = canMessage.frame.data0;
                oilTemp = canMessage.frame.data1;
                battVolts = canMessage.frame.data2;
            }
        }
        if(refresh_screen) {
            display(rpm, oilP, fuelP, tp, speed, gear, engTemp, oilTemp, battVolts);
            up_shift_SetLow();
            down_shift_SetLow();
            warning_1_SetLow();
            warning_2_SetLow();
            warning_3_SetLow();
            warning_4_SetLow();
            refresh_screen = false;
            TMR1_Reload();
        }
    }
}


/**
 End of File
*/