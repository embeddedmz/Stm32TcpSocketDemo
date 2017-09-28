/**
 * @file main.cpp
 * @brief Main routine
 *
 * @section License
 *
 * Copyright (C) 2010-2017 Oryx Embedded SARL. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * @author Oryx Embedded SARL (www.oryx-embedded.com)
 * @version 1.7.8
 **/

//Dependencies
#include <stdlib.h>
#include "stm32f4xx.h"
#include "stm32f4_discovery.h"
#include "stm32f4_discovery_lcd.h"
#include "os_port.h"
#include "core/net.h"
#include "drivers/stm32f4x7_eth.h"
#include "drivers/lan8720.h"
#include "dhcp/dhcp_client.h"
#include "ipv6/slaac.h"
#include "debug.h"

//Application configuration
#define APP_MAC_ADDR "00-AB-CD-EF-04-07"

//#define APP_USE_DHCP ENABLED
#define APP_USE_DHCP DISABLED
#define APP_IPV4_HOST_ADDR "192.168.69.2"
#define APP_IPV4_SUBNET_MASK "255.255.255.0"
#define APP_IPV4_DEFAULT_GATEWAY "192.168.69.42"
#define APP_IPV4_PRIMARY_DNS "8.8.8.8"
#define APP_IPV4_SECONDARY_DNS "8.8.4.4"

//#define APP_USE_SLAAC ENABLED
#define APP_USE_SLAAC DISABLED
#define APP_IPV6_LINK_LOCAL_ADDR "fe80::407"
#define APP_IPV6_PREFIX "2001:db8::"
#define APP_IPV6_PREFIX_LENGTH 64
#define APP_IPV6_GLOBAL_ADDR "2001:db8::407"
#define APP_IPV6_ROUTER "fe80::1"
#define APP_IPV6_PRIMARY_DNS "2001:4860:4860::8888"
#define APP_IPV6_SECONDARY_DNS "2001:4860:4860::8844"

//Constant definitions
#define PC_SERVER_IP_ADDR "192.168.69.42"
#define PC_SERVER_PORT    4242
#define APP_SERVER_NAME "www.oryx-embedded.com"
#define APP_SERVER_PORT 80
#define APP_REQUEST_URI "/test.php"

//Global variables
uint_t lcdLine = 0;
uint_t lcdColumn = 0;

DhcpClientSettings dhcpClientSettings;
DhcpClientContext dhcpClientContext;
SlaacSettings slaacSettings;
SlaacContext slaacContext;


/**
 * @brief Set cursor location
 * @param[in] line Line number
 * @param[in] column Column number
 **/
void lcdSetCursor(uint_t line, uint_t column)
{
   lcdLine = MIN(line, 10);
   lcdColumn = MIN(column, 20);
}


/**
 * @brief Write a character to the LCD display
 * @param[in] c Character to be written
 **/
extern "C"
void lcdPutChar(char_t c)
{
   if(c == '\r')
   {
      lcdColumn = 0;
   }
   else if(c == '\n')
   {
      lcdColumn = 0;
      lcdLine++;
   }
   else if(lcdLine < 10 && lcdColumn < 20)
   {
      //Display current character
      LCD_DisplayChar(lcdLine * 24, lcdColumn * 16, c);

      //Advance the cursor position
      if(++lcdColumn >= 20)
      {
         lcdColumn = 0;
         lcdLine++;
      }
   }
}


/**
 * @brief I/O initialization
 **/
void ioInit(void)
{
   GPIO_InitTypeDef GPIO_InitStructure;

   //LED configuration
   STM_EVAL_LEDInit(LED3);
   STM_EVAL_LEDInit(LED4);
   STM_EVAL_LEDInit(LED5);
   STM_EVAL_LEDInit(LED6);

   //Clear LEDs
   STM_EVAL_LEDOff(LED3);
   STM_EVAL_LEDOff(LED4);
   STM_EVAL_LEDOff(LED5);
   STM_EVAL_LEDOff(LED6);

   //Initialize user button
   STM_EVAL_PBInit(BUTTON_USER, BUTTON_MODE_GPIO);

   //Enable GPIOE clock
   RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);

   //Configure PE2 (PHY_RST) pin as an output
   GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
   GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
   GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
   GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
   GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
   GPIO_Init(GPIOE, &GPIO_InitStructure);

   //Reset PHY transceiver (hard reset)
   GPIO_ResetBits(GPIOE, GPIO_Pin_2);
   sleep(10);
   GPIO_SetBits(GPIOE, GPIO_Pin_2);
   sleep(10);
}

#define COMMAND_LENGTH 4
const char* g_arrCommands[] = {"STAR", "STOP", "LED3", "LED5", "LED6"};
char_t g_szIpRemote[256];
uint16_t g_clientPort;
IpAddr clientIpAddr;
OsEvent eventStartSendStatuts;
volatile bool_t g_bSendStatus = FALSE;
void CommandServer(void *param)
{
   error_t error;
   size_t length;
   unsigned key = 0;
   Socket* socket;
   Socket* socketCli;
   static char_t buffer[COMMAND_LENGTH + 1];

   //char_t buffer[40];
#if (IPV4_SUPPORT == ENABLED)
   Ipv4Addr ipv4Addr;
#endif

   //Point to the network interface
   NetInterface* interface = &netInterface[0];

   //Initialize LCD display
   //lcdSetCursor(2, 0);
   //printf("IPv4 Addr\r\n");
   //lcdSetCursor(5, 0);
   //printf("Press user button\r\nto run test\r\n");
#if (IPV4_SUPPORT == ENABLED)
   //Display IPv4 host address
   //lcdSetCursor(3, 0);
   ipv4GetHostAddr(interface, &ipv4Addr);
   //printf("%-16s\r\n", ipv4AddrToString(ipv4Addr, buffer));
#endif

   // Fill zeroes on client IP address
   memset(g_szIpRemote, '\0', sizeof(g_szIpRemote));

   //Open a TCP socket
   socket = socketOpen(SOCKET_TYPE_STREAM, SOCKET_IP_PROTO_TCP);
   //Failed to open socket?
   if(!socket) {
      TRACE_ERROR("socketOpen error !\r\n");
      return;
   }

   //Set timeout for blocking functions
   error = socketSetTimeout(socket, INFINITE_DELAY);
   //Any error to report?
   if(error) {
      TRACE_ERROR("socketSetTimeout error !\r\n");
      return;
   }

   //Associate the socket with the relevant interface
   error = socketBindToInterface(socket, interface);
   //Unable to bind the socket to the desired interface?
   if(error) {
      TRACE_ERROR("socketBindToInterface error !\r\n");
      return;
   }

   //Bind newly created socket to port 4242
   error = socketBind(socket, &IP_ADDR_ANY, 4242);
   //Failed to bind socket to port 80?
   if(error) {
      TRACE_ERROR("socketBind error !\r\n");
      return;
   }

   //Place socket in listening state
   error = socketListen(socket, 1);
   //Any failure to report?
   if(error) {
      TRACE_ERROR("socketListen error !\r\n"); // we can use the line __LINE__ macro
      return;
   }

   //Endless loop
   while(1)
   {
      //Accept an incoming connection
      socketCli = socketAccept(socket, &clientIpAddr, &g_clientPort);

      //Make sure the socket handle is valid
      if(socketCli != NULL)
      {
         strcpy(g_szIpRemote, ipAddrToString(&clientIpAddr, NULL));

         TRACE_INFO("Incoming Connection from client %s port %" PRIu16 "...\r\n",
               g_szIpRemote, g_clientPort);

         //Read response body
         while (socketReceive(socketCli, buffer, sizeof(buffer), &length, 0) == 0)
         {
            buffer[COMMAND_LENGTH] = '\0';

            for (key = 0; key < sizeof(g_arrCommands)/sizeof(const char*); ++key)
            {
               if (strcmp(g_arrCommands[key], buffer) == 0)
                  break;
            }

            switch (key)
            {
            case 0: /* STAR */
               g_bSendStatus = TRUE;
               osSetEvent(&eventStartSendStatuts);
               break;
            case 1: /* STOP */
               g_bSendStatus = FALSE;
               break;
            case 2: /* LED3 */
               STM_EVAL_LEDToggle(LED3);
               break;
            case 3: /* LED5 */
               STM_EVAL_LEDToggle(LED5);
               break;
            case 4: /* LED6 */
               STM_EVAL_LEDToggle(LED6);
               break;
            default:
               error = socketSend(socketCli, "ERROR: UNKNOWN CMD\r\n", strlen("ERROR: UNKNOWN CMD\r\n"), NULL, 0);
               //Any error to report?
               if(error)
               {
                  TRACE_ERROR("socketSend error !\r\n"); // we can use the line __LINE__ macro
               }
               break;
            }
         }
         socketClose(socketCli);
      }
      else
      {
         TRACE_ERROR("socketAccept error !\r\n");
      }

      //Loop delay
      osDelayTask(100);
   }
}

void SendLEDStatuts(void* pParam)
{
   error_t error;
   size_t length;
   IpAddr ipAddr;
   Socket *socket;
   char_t szStatus[256];
   char_t szIpRemote[256];

   memset(szStatus, '\0', sizeof(szStatus));
   memset(szIpRemote, '\0', sizeof(szIpRemote));

   if (!osCreateEvent(&eventStartSendStatuts))
   {
      TRACE_ERROR("osCreateEvent error !\r\n");
      return;
   }

   while (1)
   {
      if (!osWaitForEvent(&eventStartSendStatuts, INFINITE_DELAY))
      {
         TRACE_ERROR("osWaitForEvent error !\r\n");
         break;
      }

      //Create a new socket to handle the request
      socket = socketOpen(SOCKET_TYPE_STREAM, SOCKET_IP_PROTO_TCP);
      //Any error to report?
      if(!socket)
      {
         TRACE_ERROR("socketOpen error !\r\n");
         break;
      }

      //Resolve TCP server name
      error = getHostByName(NULL, g_szIpRemote, &ipAddr, 0);
      //Any error to report?
      if(error)
      {
         TRACE_ERROR("getHostByName error !\r\n");
         break;
      }

      error = socketConnect(socket, &ipAddr, PC_SERVER_PORT);
      if(error)
      {
         TRACE_ERROR("socketConnect error !\r\n");
         break;
      }

      while (g_bSendStatus)
      {
         length = sprintf(szStatus, "LED3 : %s | LED5 : %s | LED6 : %s\r\n",
               ( (GPIOD->ODR & GPIO_Pin_13) != (uint32_t)Bit_RESET) ? "ON" : "OFF",
                     ( (GPIOD->ODR & GPIO_Pin_14) != (uint32_t)Bit_RESET) ? "ON" : "OFF",
                           ( (GPIOD->ODR & GPIO_Pin_15) != (uint32_t)Bit_RESET) ? "ON" : "OFF");

         error = socketSend(socket, szStatus, length, NULL, 0);
         //Any error to report?
         if(error)
         {
            TRACE_ERROR("socketSend error !\r\n");
            break;
         }

         osDelayTask(5000);
      }
      //Close the connection
      socketClose(socket);
      osResetEvent(&eventStartSendStatuts);
   }
   osDeleteEvent(&eventStartSendStatuts);
}

/**
 * @brief LED blinking task
 **/

void blinkTask(void *param)
{
   //Endless loop
   while(1)
   {
      STM_EVAL_LEDOn(LED4);
      osDelayTask(100);
      STM_EVAL_LEDOff(LED4);
      osDelayTask(900);
   }
}


/**
 * @brief Main entry point
 * @return Unused value
 **/

int main(void)
{
   error_t error;
   NetInterface *interface;
   OsTask *task;
   MacAddr macAddr;
#if (APP_USE_DHCP == DISABLED)
   Ipv4Addr ipv4Addr;
#endif
#if (APP_USE_SLAAC == DISABLED)
   //Ipv6Addr ipv6Addr;
#endif

   //Initialize kernel
   osInitKernel();
   //Configure debug UART at 9600 bps
   debugInit(9600);

   //Start-up message
   TRACE_INFO("\r\n");
   TRACE_INFO("*******************************************************\r\n");
   TRACE_INFO("*** CycloneTCP TCP Client/Server Demo by embeddedmz ***\r\n");
   TRACE_INFO("*******************************************************\r\n");
   TRACE_INFO("Copyright: 2010-2017 Oryx Embedded SARL\r\n");
   TRACE_INFO("Compiled: %s %s\r\n", __DATE__, __TIME__);
   TRACE_INFO("Target: STM32F407\r\n");
   TRACE_INFO("\r\n");

   //Configure I/Os
   ioInit();

   //Initialize LCD display
   /*STM32f4_Discovery_LCD_Init();
   LCD_SetBackColor(Blue);
   LCD_SetTextColor(White);
   LCD_SetFont(&Font16x24);
   LCD_Clear(Blue);*/

   //Welcome message
   //lcdSetCursor(0, 0);
   //printf("HTTP Client Demo\r\n");

   //TCP/IP stack initialization
   error = netInit();
   //Any error to report?
   if(error)
   {
      //Debug message
      TRACE_ERROR("Failed to initialize TCP/IP stack!\r\n");
   }

   //Configure the first Ethernet interface
   interface = &netInterface[0];

   //Set interface name
   netSetInterfaceName(interface, "eth0");
   //Set host name
   netSetHostname(interface, "TCPClientDemo");
   //Select the relevant network adapter
   netSetDriver(interface, &stm32f4x7EthDriver);
   netSetPhyDriver(interface, &lan8720PhyDriver);
   //Set host MAC address
   macStringToAddr(APP_MAC_ADDR, &macAddr);
   netSetMacAddr(interface, &macAddr);

   //Initialize network interface
   error = netConfigInterface(interface);
   //Any error to report?
   if(error)
   {
      //Debug message
      TRACE_ERROR("Failed to configure interface %s!\r\n", interface->name);
   }

#if (IPV4_SUPPORT == ENABLED)
#if (APP_USE_DHCP == ENABLED)
   //Get default settings
   dhcpClientGetDefaultSettings(&dhcpClientSettings);
   //Set the network interface to be configured by DHCP
   dhcpClientSettings.interface = interface;
   //Disable rapid commit option
   dhcpClientSettings.rapidCommit = FALSE;

   //DHCP client initialization
   error = dhcpClientInit(&dhcpClientContext, &dhcpClientSettings);
   //Failed to initialize DHCP client?
   if(error)
   {
      //Debug message
      TRACE_ERROR("Failed to initialize DHCP client!\r\n");
   }

   //Start DHCP client
   error = dhcpClientStart(&dhcpClientContext);
   //Failed to start DHCP client?
   if(error)
   {
      //Debug message
      TRACE_ERROR("Failed to start DHCP client!\r\n");
   }
#else
   //Set IPv4 host address
   ipv4StringToAddr(APP_IPV4_HOST_ADDR, &ipv4Addr);
   ipv4SetHostAddr(interface, ipv4Addr);

   //Set subnet mask
   ipv4StringToAddr(APP_IPV4_SUBNET_MASK, &ipv4Addr);
   ipv4SetSubnetMask(interface, ipv4Addr);

   //Set default gateway
   ipv4StringToAddr(APP_IPV4_DEFAULT_GATEWAY, &ipv4Addr);
   ipv4SetDefaultGateway(interface, ipv4Addr);

   //Set primary and secondary DNS servers
   ipv4StringToAddr(APP_IPV4_PRIMARY_DNS, &ipv4Addr);
   ipv4SetDnsServer(interface, 0, ipv4Addr);
   ipv4StringToAddr(APP_IPV4_SECONDARY_DNS, &ipv4Addr);
   ipv4SetDnsServer(interface, 1, ipv4Addr);
#endif
#endif

#if (IPV6_SUPPORT == ENABLED)
#if (APP_USE_SLAAC == ENABLED)
   //Get default settings
   slaacGetDefaultSettings(&slaacSettings);
   //Set the network interface to be configured
   slaacSettings.interface = interface;

   //SLAAC initialization
   error = slaacInit(&slaacContext, &slaacSettings);
   //Failed to initialize SLAAC?
   if(error)
   {
      //Debug message
      TRACE_ERROR("Failed to initialize SLAAC!\r\n");
   }

   //Start IPv6 address autoconfiguration process
   error = slaacStart(&slaacContext);
   //Failed to start SLAAC process?
   if(error)
   {
      //Debug message
      TRACE_ERROR("Failed to start SLAAC!\r\n");
   }
#else
   //Set link-local address
   ipv6StringToAddr(APP_IPV6_LINK_LOCAL_ADDR, &ipv6Addr);
   ipv6SetLinkLocalAddr(interface, &ipv6Addr);

   //Set IPv6 prefix
   ipv6StringToAddr(APP_IPV6_PREFIX, &ipv6Addr);
   ipv6SetPrefix(interface, 0, &ipv6Addr, APP_IPV6_PREFIX_LENGTH);

   //Set global address
   ipv6StringToAddr(APP_IPV6_GLOBAL_ADDR, &ipv6Addr);
   ipv6SetGlobalAddr(interface, 0, &ipv6Addr);

   //Set default router
   ipv6StringToAddr(APP_IPV6_ROUTER, &ipv6Addr);
   ipv6SetDefaultRouter(interface, 0, &ipv6Addr);

   //Set primary and secondary DNS servers
   ipv6StringToAddr(APP_IPV6_PRIMARY_DNS, &ipv6Addr);
   ipv6SetDnsServer(interface, 0, &ipv6Addr);
   ipv6StringToAddr(APP_IPV6_SECONDARY_DNS, &ipv6Addr);
   ipv6SetDnsServer(interface, 1, &ipv6Addr);
#endif
#endif

   task = osCreateTask("User Task", SendLEDStatuts, NULL, 800, OS_TASK_PRIORITY_NORMAL);
   //Failed to create the task?
   if(task == OS_INVALID_HANDLE)
   {
      //Debug message
      TRACE_ERROR("Failed to create task!\r\n");
   }

   //Create user task
   task = osCreateTask("User Task", CommandServer, NULL, 800, OS_TASK_PRIORITY_NORMAL);
   //Failed to create the task?
   if(task == OS_INVALID_HANDLE)
   {
      //Debug message
      TRACE_ERROR("Failed to create task!\r\n");
   }

   //Create a task to blink the LED
   task = osCreateTask("Blink", blinkTask, NULL, 500, OS_TASK_PRIORITY_NORMAL);
   //Failed to create the task?
   if(task == OS_INVALID_HANDLE)
   {
      //Debug message
      TRACE_ERROR("Failed to create task!\r\n");
   }

   //Start the execution of tasks
   osStartKernel();

   //This function should never return
   return 0;
}
