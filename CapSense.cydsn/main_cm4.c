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
#include "queue.h"

QueueHandle_t pwmQueueHandle;

// This task controls the PWM
void pwmTask(void *arg)
{
    (void)arg;
    
    uint32_t msg;
    
    printf("Starting PWM Task\r\n");
    pwmQueueHandle = xQueueCreate(1,sizeof(uint32_t));
   
    while(1)
    {
        xQueueReceive(pwmQueueHandle,&msg,portMAX_DELAY);
        Cy_TCPWM_PWM_SetCompare0(PWM_1_HW,PWM_1_CNT_NUM,msg);
   }
}

// uartTask - the function which handles input from the UART
void uartTask(void *arg)
{
    (void)arg;
    
    char c;
    uint32_t msg;

    printf("Started UART Task\r\n");
    setvbuf(stdin,0,_IONBF,0);
	while(1)
	{
		if(Cy_SCB_UART_GetNumInRxFifo(UART_1_HW))
		{
		    c = getchar();
    		switch(c)
	    	{
		    	case '0': // send the stop message
			    	msg = 0;
		    		xQueueSend(pwmQueueHandle,&msg,portMAX_DELAY);               
		    	break;
		    	case '1': // Send the start message
		    		msg = 100;
		    		xQueueSend(pwmQueueHandle,&msg,portMAX_DELAY);
		    	break;
			    case '5':
				    msg = 50;
				    xQueueSend(pwmQueueHandle,&msg,portMAX_DELAY);
			    break;
			    case '?': // Print Help 
				    printf("s - stop PWM\r\nS - start PWM\r\n");
			    break;
		    }
		}
		taskYIELD();
    }
}

void capsenseTask(void *arg)
{
    (void)arg;
    
    uint32_t msg;
    
    int b0prev=0;
    int b1prev=0;
    int b0current=0;
    int b1current=0;
    int sliderPos;
    
    printf("Starting CapSense Task\r\n");
    
    CapSense_Start();
    CapSense_ScanAllWidgets();
        
    for(;;)
    {
        if(!CapSense_IsBusy())
        {
            CapSense_ProcessAllWidgets();
            sliderPos=CapSense_GetCentroidPos(CapSense_LINEARSLIDER0_WDGT_ID);
            if(sliderPos<0xFFFF) // If they are touching the slider then send the %
            {
                msg = sliderPos;
                xQueueSend(pwmQueueHandle,&msg,portMAX_DELAY);
            }
            b0current = CapSense_IsWidgetActive(CapSense_BUTTON0_WDGT_ID);
            b1current = CapSense_IsWidgetActive(CapSense_BUTTON1_WDGT_ID); 
            
            if(b0current && b0prev == 0) // If they pressed btn0
            {
                msg = 0; 
                xQueueSend(pwmQueueHandle,&msg,portMAX_DELAY);
            }
            if( b1current && b1prev == 0) // If they pressed btn0
            {
               msg = 100;
               xQueueSend(pwmQueueHandle,&msg,portMAX_DELAY);  
            }
            b0prev = b0current;
            b1prev = b1current;
               
            CapSense_UpdateAllBaselines();
            CapSense_ScanAllWidgets();
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
    printf("PWM Capsense & UART Project\r\n");
 
    // Mask a FreeRTOS Task called uartTask
    xTaskCreate(pwmTask,"PWM Task",configMINIMAL_STACK_SIZE*8,0,3,0);
    xTaskCreate(uartTask,"UART Task",configMINIMAL_STACK_SIZE*8,0,3,0);
    xTaskCreate( capsenseTask, "CapSense Task",2048*2,0,3,0);
    
    vTaskStartScheduler();  // Will never return
    
    for(;;) // It will never get here
    {
    }
}
