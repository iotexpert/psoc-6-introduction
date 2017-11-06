/* ========================================
 *
 * Copyright YOUR COMPANY, THE YEAR
 * All Rights Reserved
 * UNPUBLISHED, LICENSED SOFTWARE.
 *
 * CONFIDENTIAL AND PROPRIETARY INFORMATION
 * WHICH IS THE PROPERTY OF your company.
 *
 * ========================================
*/
#include "project.h"
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"

// uartTask - the function which handles input from the UART
void uartTask(void *arg)
{
    (void)arg;
    char c;
    setvbuf(stdin,0,_IONBF,0); // Turn off buffering
    while(1)
    {
        if(Cy_SCB_UART_GetNumInRxFifo(UART_1_HW)) // if there is a keypress
        {
            c = getchar();
            switch(c)
            {
                case 's':
                    printf("Stopped PWM\r\n");
                    Cy_TCPWM_PWM_Disable(PWM_1_HW,PWM_1_CNT_NUM);
                break;
                case 'S':
                    printf("Started PWM\r\n");
                    Cy_TCPWM_PWM_Enable(PWM_1_HW,PWM_1_CNT_NUM);
                    Cy_TCPWM_TriggerStart(PWM_1_HW, PWM_1_CNT_MASK);
                break;
            }
        }
        else
            taskYIELD();
    }
}


int main(void)
{
    __enable_irq(); /* Enable global interrupts. */

    /* Place your initialization/startup code here (e.g. MyInst_Start()) */

    PWM_1_Start();
    UART_1_Start();
    
    printf("\033[2J\033[H"); // VT100 Clear Screen
    printf("Test\r\n");
 
    // Mask a FreeRTOS Task called uartTask
    xTaskCreate(uartTask,"UART Task",configMINIMAL_STACK_SIZE,0,1,0);
    vTaskStartScheduler();  // Will never return
    
    for(;;) // It will never get here
    {
    }
}
