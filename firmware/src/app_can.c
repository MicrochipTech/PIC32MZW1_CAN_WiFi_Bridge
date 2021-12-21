/*******************************************************************************
  MPLAB Harmony Application Source File

  Company:
    Microchip Technology Inc.

  File Name:
    app_can.c

  Summary:
    This file contains the source code for the MPLAB Harmony application.

  Description:
    This file contains the source code for the MPLAB Harmony application.  It
    implements the logic of the application's state machine and it may call
    API routines of other MPLAB Harmony modules in the system, such as drivers,
    system services, and middleware.  However, it does not call any of the
    system interfaces (such as the "Initialize" and "Tasks" functions) of any of
    the modules in the system or make any assumptions about when those functions
    are called.  That is the responsibility of the configuration-specific system
    files.
 *******************************************************************************/

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************

#include <stddef.h>                     // Defines NULL
#include <stdbool.h>                    // Defines true
#include <stdlib.h>                     // Defines EXIT_FAILURE
#include <stdarg.h>
#include "definitions.h"                // SYS function prototypes

// *****************************************************************************
// *****************************************************************************
// Section: Global Data Definitions
// *****************************************************************************
// *****************************************************************************

// *****************************************************************************
/* Application Data

  Summary:
    Holds application data

  Description:
    This structure holds the application's data.

  Remarks:
    This structure should be initialized by the APP_CAN_Initialize function.

    Application strings and buffers are be defined outside this structure.
*/

APP_CAN_DATA app_canData;
/* Application's state machine enum */
typedef enum
{
    APP_STATE_CAN_RECEIVE,
    APP_STATE_CAN_TRANSMIT,
    APP_STATE_CAN_IDLE,
    APP_STATE_CAN_START_RX,
    APP_STATE_CAN_XFER_SUCCESSFUL,
    APP_STATE_CAN_XFER_ERROR
} APP_STATES;

/* Variable to save application state */
static APP_STATES state = APP_STATE_CAN_START_RX;
/* Variable to save Tx/Rx transfer status and context */
static uint32_t status = 0;
static uint32_t xferContext = 0;
/* Variable to save Tx/Rx message */
static uint32_t messageID = 0;
static uint8_t message[8];
static uint8_t messageLength = 0;
static uint8_t rx_message[8];
static uint32_t rx_messageID = 0;
static uint8_t rx_messageLength = 0;
static uint16_t timestamp = 0;
static CAN_MSG_RX_ATTRIBUTE msgAttr = CAN_MSG_RX_DATA_FRAME;

 QueueHandle_t queue_can2wifi;
 extern QueueHandle_t queue_wifi2can; 
// *****************************************************************************
// *****************************************************************************
// Section: Application Callback Functions
// *****************************************************************************
// *****************************************************************************

/* This function will be called by CAN PLIB when transfer is completed */
// *****************************************************************************
/* void APP_CAN_Callback(uintptr_t context)

  Summary:
    Function called by CAN PLIB upon transfer completion

  Description:
    This function will be called by CAN PLIB when transfer is completed.
    In this function, current state of the application is obtained by context
    parameter. Based on current state of the application and CAN error
    state, next state of the application is decided.

  Remarks:
    None.
*/
void APP_CAN_Callback(uintptr_t context)
{
    xferContext = context;
    
    /* Check CAN Status */
    status = CAN1_ErrorGet();

    if ((status & (CAN_ERROR_TX_RX_WARNING_STATE | CAN_ERROR_RX_WARNING_STATE |
                   CAN_ERROR_TX_WARNING_STATE | CAN_ERROR_RX_BUS_PASSIVE_STATE |
                   CAN_ERROR_TX_BUS_PASSIVE_STATE | CAN_ERROR_TX_BUS_OFF_STATE)) == CAN_ERROR_NONE)
    {
        
        switch ((APP_STATES)context)
        {
            case APP_STATE_CAN_RECEIVE:
            {
                //SYS_CONSOLE_PRINT("CAN Callback(): APP_STATE_CAN_RECEIVE \r\n");
                state = APP_STATE_CAN_XFER_SUCCESSFUL;  
                break;
            }
            case APP_STATE_CAN_TRANSMIT:
            {
                xferContext = context;
                //SYS_CONSOLE_PRINT("CAN Callback(): APP_STATE_CAN_TRANSMIT \r\n");
                state = APP_STATE_CAN_XFER_SUCCESSFUL;    
                break;
            }
            default:
                break;
        }
    }
    else
    {
        state = APP_STATE_CAN_XFER_ERROR;
    }
}

// *****************************************************************************
// *****************************************************************************
// Section: Application Local Functions
// *****************************************************************************
// *****************************************************************************


// *****************************************************************************
// *****************************************************************************
// Section: Application Initialization and State Machine Functions
// *****************************************************************************
// *****************************************************************************

/*******************************************************************************
  Function:
    void APP_CAN_Initialize ( void )

  Remarks:
    See prototype in app_can.h.
 */

void APP_CAN_Initialize ( void )
{
    /* Place the App state machine in its initial state. */
    app_canData.state = APP_CAN_STATE_INIT;

    SYS_CONSOLE_MESSAGE("APP_CAN_Initialize\r\n");
    
    queue_can2wifi = xQueueCreate( 10, sizeof( struct canBusMessage * ) );
    sem_c2w = xSemaphoreCreateMutex();
    xSemaphoreGive( sem_c2w );

    /* TODO: Initialize your application's state machine and other
     * parameters.
     */
}


/******************************************************************************
  Function:
    void APP_CAN_Tasks ( void )

  Remarks:
    See prototype in app_can.h.
 */

void APP_CAN_Tasks ( void )
{

    uint8_t count = 0;
    struct canBusMessage *canMessage = NULL;
    struct canBusMessage *wifiMsg = NULL;
    int i = 0;      // counter

    
    if (state == APP_STATE_CAN_START_RX)
    {
        /* Read user input */
        //UART1_Read((void *)&user_input, 1);

        SYS_CONSOLE_PRINT("%s(): Waiting for CAN message: \r\n", __func__);
        CAN1_CallbackRegister( APP_CAN_Callback, (uintptr_t)APP_STATE_CAN_RECEIVE, 1 );
        state = APP_STATE_CAN_IDLE;
        memset(rx_message, 0x00, sizeof(rx_message));
        /* Receive New Message */
        if (CAN1_MessageReceive(&rx_messageID, &rx_messageLength, rx_message, &timestamp, 1, &msgAttr) == false)  
        {
            SYS_CONSOLE_PRINT("%s(): CAN1_MessageReceive request has failed\r\n", __func__);
        }

    }
        
    // check the queue to get msg from wifi app
    if( queue_wifi2can != 0 )
    {
       if( xSemaphoreTake( sem_w2c, portMAX_DELAY) )
       {
           if( xQueueReceive( queue_wifi2can, &wifiMsg, 10 ) )
           {
               // SYS_CONSOLE_PRINT("APP_CAN_Bus_Tasks 2, wifi data = %d %d\r\n", wifiMsg->data[0], wifiMsg->data[1]);

                // To Do
               // send CAN Bus message

                messageID = (uint32_t) wifiMsg->id;
                messageLength = (uint8_t) wifiMsg->len;
                for (count = 0; count <8; count++)
                {
                    message[count] = (uint8_t) wifiMsg->data[count];
                }

                CAN1_CallbackRegister( APP_CAN_Callback, (uintptr_t)APP_STATE_CAN_TRANSMIT, 0 );
                state = APP_STATE_CAN_IDLE;
                SYS_CONSOLE_PRINT("%s(): Send WiFi message to CAN device\r\n", __func__);
                if (CAN1_MessageTransmit(messageID, messageLength, message, 0, CAN_MSG_TX_DATA_FRAME) == false)
                {
                    SYS_CONSOLE_PRINT("%s(): CAN1_MessageTransmit request has failed\r\n", __func__);
                }             


                vPortFree(wifiMsg);
           }
            xSemaphoreGive( sem_w2c);
       }
    }
    
    /* Check the application's current state. */
    switch (state)
    {
        case APP_STATE_CAN_IDLE:
        {
            /* Application can do other task here */
            break;
        }
        case APP_STATE_CAN_XFER_SUCCESSFUL:
        {
            if ((APP_STATES)xferContext == APP_STATE_CAN_RECEIVE)
            {
                /* Print message to Console */
                SYS_CONSOLE_PRINT("%s(): New Message Received from CAN Bus\r\n", __func__);
                uint8_t length = rx_messageLength;
                SYS_CONSOLE_PRINT("%s(): Message - Timestamp : 0x%x ID : 0x%x Length : 0x%x ", __func__,  (unsigned int)timestamp, (unsigned int) rx_messageID,(unsigned int) rx_messageLength);
                SYS_CONSOLE_PRINT("Message : ");
                while(length)
                {
                    SYS_CONSOLE_PRINT("0x%x ", rx_message[rx_messageLength - length--]);
                }
                SYS_CONSOLE_PRINT("\r\n");

                if( queue_can2wifi != 0 )
                {
                    // Send a pointer to a struct AMessage object.  Don't block if the
                    // queue is already full.

                    canMessage = pvPortMalloc(sizeof(struct canBusMessage));
                    canMessage->ts = timestamp;
                    canMessage->id = rx_messageID;
                    canMessage->len = rx_messageLength;
                    for (i = 0; i < 8; i++)
                        canMessage->data[i] = (unsigned int) rx_message[i];

                     //memcpy( canMessage->data, rx_message, 8);

                    if( xSemaphoreTake( sem_c2w, portMAX_DELAY) )
                    {
                        //SYS_CONSOLE_PRINT("%s(): send CAN message to WiFi queue \r\n", __func__);
                        if ( pdTRUE != xQueueSend( queue_can2wifi, &canMessage, ( TickType_t ) 0 ) )
                        {
                            SYS_CONSOLE_PRINT("%s(): Failed to add CAN message to WiFi queue ... \r\n", __func__);
                        }
                        xSemaphoreGive( sem_c2w );
                    }
                }


                memset(rx_message, 0x00, sizeof(rx_message));
                /* Receive New Message */
                if (CAN1_MessageReceive(&rx_messageID, &rx_messageLength, rx_message, &timestamp, 1, &msgAttr) == false)  
                {
                    SYS_CONSOLE_PRINT("%s(): CAN1_MessageReceive request has failed\r\n", __func__);
                }
            } 
            else if ((APP_STATES)xferContext == APP_STATE_CAN_TRANSMIT)
            {
                SYS_CONSOLE_PRINT("\r%s(): WiFi Msg Transmit Success to CAN device \r\n", __func__);
            }
         
            state = APP_STATE_CAN_IDLE;
            
            break;
        }
        case APP_STATE_CAN_XFER_ERROR:
        {
            if ((APP_STATES)xferContext == APP_STATE_CAN_RECEIVE)
            {
                SYS_CONSOLE_PRINT("%s(): Error in received CAN message\r\n",  __func__);
            }
            else
            {
                SYS_CONSOLE_PRINT("%s(): Failed to receive CAN message\r\n",  __func__);
            }

            state = APP_STATE_CAN_START_RX;
            break;
        }
        default:
            break;
    }
        

    /* Execution should not come here during normal operation */

}


/*******************************************************************************
 End of File
 */
