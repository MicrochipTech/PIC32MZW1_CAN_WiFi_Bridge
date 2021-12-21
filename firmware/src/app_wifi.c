/*******************************************************************************
  MPLAB Harmony Application Source File

  Company:
    Microchip Technology Inc.

  File Name:
    app_wifi.c

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

#include "system/net/sys_net.h"

// *****************************************************************************
// *****************************************************************************
// Section: Global Data Definitions
// *****************************************************************************
// *****************************************************************************
SYS_MODULE_OBJ g_tcpServHandle = SYS_MODULE_OBJ_INVALID;
SYS_NET_Config g_tcpServCfg;

#define RECV_BUFFER_LEN		1400
#define SEND_BUFFER_LEN		100
uint8_t recv_buffer[RECV_BUFFER_LEN + 1];
uint8_t send_buffer[SEND_BUFFER_LEN + 1];

QueueHandle_t queue_wifi2can; 
extern  QueueHandle_t queue_can2wifi;

int g_isSocketConnect = 0;

#define CAN_MESSAGE_TEMPLATE "{\"Timestamp\": %u,\"ID\":%x,\"LEN\":%u,\"DAT0\":%x,\"DAT1\":%x,\"DAT2\":%x,\"DAT3\":%x,\"DAT4\":%x,\"DAT5\":%x,\"DAT6\":%x,\"DAT7\":%x}"


// *****************************************************************************
/* Application Data

  Summary:
    Holds application data

  Description:
    This structure holds the application's data.

  Remarks:
    This structure should be initialized by the APP_WIFI_Initialize function.

    Application strings and buffers are be defined outside this structure.
*/

APP_WIFI_DATA app_wifiData;

// *****************************************************************************
// *****************************************************************************
// Section: Application Callback Functions
// *****************************************************************************
// *****************************************************************************

void TcpServCallback(uint32_t event, void *data, void* cookie)
{
    struct canBusMessage *canMessage = NULL;

    switch(event)
    {
        case SYS_NET_EVNT_CONNECTED:
        {
            SYS_CONSOLE_PRINT("TcpServCallback(): Status UP\r\n");
            g_isSocketConnect = 1;
            break;
        }

        case SYS_NET_EVNT_DISCONNECTED:
        {
            SYS_CONSOLE_PRINT("TcpServCallback(): Status DOWN\r\n");
            g_isSocketConnect = 0;
            break;
        }

        case SYS_NET_EVNT_RCVD_DATA:
        {
            int32_t len = RECV_BUFFER_LEN;
            
            SYS_CONSOLE_PRINT("TcpServCallback(): SYS_NET_EVNT_RCVD_DATA\r\n");
            while(len == RECV_BUFFER_LEN)
            {
                len = SYS_NET_RecvMsg(g_tcpServHandle, recv_buffer, RECV_BUFFER_LEN);
                if(len>0)
                {
                    SYS_CONSOLE_PRINT("TcpServCallback():  New Message Received from WiFi interface = %s\r\n", recv_buffer);
                    canMessage = pvPortMalloc(sizeof(struct canBusMessage));
                
                    sscanf( (char *)recv_buffer, CAN_MESSAGE_TEMPLATE, &(canMessage->ts), &(canMessage->id), &(canMessage->len), &(canMessage->data[0]), &(canMessage->data[1]), &(canMessage->data[2]), &(canMessage->data[3]), &(canMessage->data[4]), &(canMessage->data[5]), &(canMessage->data[6]), &(canMessage->data[7]) );          
                         
                    if( xSemaphoreTake( sem_w2c, portMAX_DELAY) )
                    {
                        if ( pdTRUE != xQueueSend( queue_wifi2can, &canMessage, ( TickType_t ) 0 ) )
                        {
                            SYS_CONSOLE_PRINT("[%s] Failed to add WiFi message to queue_wifi2can ... ", __func__);
                        }
                        xSemaphoreGive( sem_w2c );
                    }
                    
            }
            }
            
            
            break;
        }
    }
}

// *****************************************************************************
// *****************************************************************************
// Section: Application Local Functions
// *****************************************************************************
// *****************************************************************************


/* TODO:  Add any necessary local functions.
*/


// *****************************************************************************
// *****************************************************************************
// Section: Application Initialization and State Machine Functions
// *****************************************************************************
// *****************************************************************************

/*******************************************************************************
  Function:
    void APP_WIFI_Initialize ( void )

  Remarks:
    See prototype in app_wifi.h.
 */

void APP_WIFI_Initialize ( void )
{
    SYS_CONSOLE_MESSAGE("APP_WIFI_Initialize\r\n");

     g_tcpServCfg.mode = SYS_NET_INDEX0_MODE;
     g_tcpServCfg.port = SYS_NET_INDEX0_PORT;
     g_tcpServCfg.enable_tls = SYS_NET_INDEX0_ENABLE_TLS;
     g_tcpServCfg.ip_prot = SYS_NET_INDEX0_IPPROT;
     g_tcpServCfg.enable_reconnect = SYS_NET_INDEX0_RECONNECT;
     g_tcpServHandle = SYS_NET_Open(&g_tcpServCfg, TcpServCallback, 0); 
     if(g_tcpServHandle != SYS_MODULE_OBJ_INVALID)
         SYS_CONSOLE_PRINT("TCP Service Initialized Successfully\r\n");

     queue_wifi2can = xQueueCreate( 10, sizeof( struct canBusMessage * ) );
     sem_w2c = xSemaphoreCreateMutex();
     xSemaphoreGive( sem_w2c );
}


/******************************************************************************
  Function:
    void APP_WIFI_Tasks ( void )

  Remarks:
    See prototype in app_wifi.h.
 */

void APP_WIFI_Tasks ( void )
{

    struct canBusMessage *canMsg = NULL;
    
    SYS_NET_Task(g_tcpServHandle);
    
    // check the CAN queue to get the CAN bus msg
     if( queue_can2wifi != 0 )
     {
        if( xSemaphoreTake( sem_c2w, portMAX_DELAY) )
        {
            if( xQueueReceive( queue_can2wifi, &canMsg, 10 ) )
            {
                if (g_isSocketConnect)
                {
                        sprintf((char *) send_buffer, CAN_MESSAGE_TEMPLATE, canMsg->ts, canMsg->id, canMsg->len, canMsg->data[0], canMsg->data[1], canMsg->data[2], canMsg->data[3], canMsg->data[4], canMsg->data[5], canMsg->data[6], canMsg->data[7]);
                        SYS_NET_SendMsg(g_tcpServHandle, send_buffer, strlen((char *)send_buffer));
                        SYS_CONSOLE_PRINT("%s(): Send CAN message to TCP Client\r\n", __func__);
                }
                
                vPortFree(canMsg);
            }
             xSemaphoreGive( sem_c2w);
        }
     }
}


/*******************************************************************************
 End of File
 */
