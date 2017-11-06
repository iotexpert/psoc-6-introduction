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
QueueHandle_t bleQueueHandle;


int connected=0;


void notifyCapSense(uint8_t capValue)
{    
    
    cy_ble_gatt_db_attr_handle_t cccdHandle;
    cy_stc_ble_gatts_db_attr_val_info_t myWrite;
    
    if(!connected)
        return;
    
    memset(&cccdHandle,0,sizeof(cccdHandle));
   
    // If notifications are on... then send notification
    cy_stc_ble_gatts_db_attr_val_info_t 	param;
    uint8_t cccd[2];
    param.connHandle = cy_ble_connHandle[0];
    param.handleValuePair.attrHandle = CY_BLE_CAPSENSE_CAPSLIDER_CLIENT_CHARACTERISTIC_CONFIGURATION_DESC_HANDLE;
    param.flags = CY_BLE_GATT_DB_LOCALLY_INITIATED;
    param.offset = 0;  
    param.handleValuePair.value.val = cccd;
    param.handleValuePair.value.len = 2;
    param.handleValuePair.value.actualLen = 2;
   
    Cy_BLE_GATTS_ReadAttributeValueCCCD(&param);
        
    if(param.handleValuePair.value.val[0] & 0x01) // if CCCD is on... notify.
    {   cy_stc_ble_gatts_handle_value_ntf_t v1;
        v1.connHandle = cy_ble_connHandle[0];
        v1.handleValPair.attrHandle = CY_BLE_CAPSENSE_CAPSLIDER_CHAR_HANDLE;
        v1.handleValPair.value.actualLen = 1;
        v1.handleValPair.value.len = 1;
        v1.handleValPair.value.val = &capValue;
        
        Cy_BLE_GATTS_Notification(&v1);
        
    }
}

void customEventHandler(uint32_t event, void *eventParameter)
{
     cy_stc_ble_gatts_write_cmd_req_param_t   *writeReqParameter;   

    /* Take an action based on the current event */
    switch (event)
    {
        /* This event is received when the BLE stack is Started */
        case CY_BLE_EVT_STACK_ON:
            Cy_BLE_GAPP_StartAdvertisement(CY_BLE_ADVERTISING_FAST,
                       CY_BLE_PERIPHERAL_CONFIGURATION_0_INDEX);
        break;
         
        /* This event is received when device is disconnected */
        case CY_BLE_EVT_GAP_DEVICE_DISCONNECTED:
            Cy_BLE_GAPP_StartAdvertisement(CY_BLE_ADVERTISING_FAST, CY_BLE_PERIPHERAL_CONFIGURATION_0_INDEX);
            connected = 0;
        break;

        case CY_BLE_EVT_GATT_CONNECT_IND:
            connected = 1;
        break;
        
        case CY_BLE_EVT_GATTS_WRITE_REQ:
            writeReqParameter = (cy_stc_ble_gatts_write_cmd_req_param_t *)eventParameter;

            if(CY_BLE_CAPSENSE_CAPSLIDER_CLIENT_CHARACTERISTIC_CONFIGURATION_DESC_HANDLE ==  writeReqParameter->handleValPair.attrHandle)
            {
               printf("Notifications pn\r\n");
                cy_stc_ble_gatts_db_attr_val_info_t myWrite;
                myWrite.offset = 0;
                myWrite.flags = CY_BLE_GATT_DB_PEER_INITIATED;
                myWrite.connHandle = writeReqParameter->connHandle;
                myWrite.handleValuePair = writeReqParameter->handleValPair;
                Cy_BLE_GATTS_WriteAttributeValueCCCD(&myWrite);
            }
            
            Cy_BLE_GATTS_WriteRsp(writeReqParameter->connHandle);
        break;

        /* Do nothing for all other events */
        default:
        break;
    }
}

/*******************************************************************************
* Function Name: bleTask
********************************************************************************
*
* This function is the the main task for running BLE.  It is launched by the FreeRTOS
* task scheduler in main_cm4.c
*
* \param 
* void * arg is not used by this task
* \return
* void - this function never returns
*
*******************************************************************************/
void bleTask(void *arg)
{
    (void)arg;
   
    uint32_t msg;
    
    bleQueueHandle = xQueueCreate(1,sizeof(uint32_t));
    Cy_BLE_Start(customEventHandler);
    
    for(;;)
    {
        Cy_BLE_ProcessEvents();
        
        if(xQueueReceive(bleQueueHandle,&msg,0) == pdTRUE)
        {
            notifyCapSense((uint8_t)msg);
        }
        
        taskYIELD();
        
    }
}


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
                xQueueSend(pwmQueueHandle,&msg,0);
                xQueueSend(bleQueueHandle,&msg,0);
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
    xTaskCreate( bleTask, "BLE Task",4096,0,3,0);
    vTaskStartScheduler();  // Will never return
    
    for(;;) // It will never get here
    {
    }
}
