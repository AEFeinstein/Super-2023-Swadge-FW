//==============================================================================
// Includes
//==============================================================================

#if defined(WINDOWS) || defined(WIN32) || defined(WIN64) || defined(_WIN32) || defined(_WIN64) || defined(__MINGW32__)
    #define USING_WINDOWS 1
#elif defined(__linux__)
    #define USING_LINUX 1
#else
    #error "OS Not Detected"
#endif

#if USING_WINDOWS
    #include <WinSock2.h>
#elif USING_LINUX
    #include <sys/socket.h> // for socket(), connect(), sendto(), and recvfrom() 
    #include <arpa/inet.h>  // for sockaddr_in and inet_addr() 
#endif

 #include <unistd.h>

#include "emu_esp.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_random.h"

#include "espNowUtils.h"
#include "p2pConnection.h"

//==============================================================================
// Defines
//==============================================================================

#define ESP_NOW_PORT 32888
#define MAXRECVSTRING 1024  // Longest string to receive 

//==============================================================================
// Variables
//==============================================================================

hostEspNowRecvCb_t hostEspNowRecvCb = NULL;
hostEspNowSendCb_t hostEspNowSendCb = NULL;

int socketFd;

//==============================================================================
// Functions
//==============================================================================

/**
 * Initialize ESP-NOW and attach callback functions
 *
 * @param recvCb A callback to call when data is sent
 * @param sendCb A callback to call when data is received
 */
void espNowInit(hostEspNowRecvCb_t recvCb, hostEspNowSendCb_t sendCb)
{
    // Save callbacks
    hostEspNowRecvCb = recvCb;
    hostEspNowSendCb = sendCb;

#if USING_WINDOWS
    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0)
    {
        ESP_LOGE("WIFI", "WSAStartup failed");
        return;
    }
#endif

    // Create a best-effort datagram socket using UDP
    if ((socketFd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
    {
        ESP_LOGE("WIFI", "socket() failed");
        return;
    }

    // Set socket to allow broadcast
    int broadcastPermission = 1;
    if (setsockopt(socketFd, SOL_SOCKET, SO_BROADCAST, (void *) &broadcastPermission,
                   sizeof(broadcastPermission)) < 0)
    {
        ESP_LOGE("WIFI", "setsockopt() failed");
        return;
    }

    // Allow multiple sockets to bind to the same port
    int enable = 1;
    if (setsockopt(socketFd, SOL_SOCKET, SO_REUSEADDR, (void *) &enable, sizeof(int)) < 0)
    {
        ESP_LOGE("WIFI", "setsockopt() failed");
        return;
    }

#if USING_WINDOWS
    //-------------------------
    // Set the socket I/O mode: In this case FIONBIO
    // enables or disables the blocking mode for the
    // socket based on the numerical value of iMode.
    // If iMode = 0, blocking is enabled;
    // If iMode != 0, non-blocking mode is enabled.
    u_long iMode = 1;
    if (ioctlsocket(socketFd, FIONBIO, &iMode) != 0)
    {
        ESP_LOGE("WIFI", "ioctlsocket() failed");
        return;
    }
#else
    int optval_enable = 1;
    setsockopt(socketFd, SOL_SOCKET, O_NONBLOCK, (char *)&optval_enable, sizeof(optval_enable));
#endif

    // Set nonblocking timeout
    struct timeval read_timeout;
    read_timeout.tv_sec = 0;
    read_timeout.tv_usec = 10;
    setsockopt(socketFd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&read_timeout, sizeof(read_timeout));

    // Construct bind structure
    struct sockaddr_in broadcastAddr;                  // Broadcast Address
    memset(&broadcastAddr, 0, sizeof(broadcastAddr));  // Zero out structure
    broadcastAddr.sin_family = AF_INET;                // Internet address family
    broadcastAddr.sin_addr.s_addr = htonl(INADDR_ANY); // Any incoming interface
    broadcastAddr.sin_port = htons(ESP_NOW_PORT);      // Broadcast port

    // Bind to the broadcast port
    if (bind(socketFd, (struct sockaddr *) &broadcastAddr, sizeof(broadcastAddr)) < 0)
    {
        ESP_LOGE("WIFI", "bind() failed");
        return;
    }
}

/**
 * This function is automatically called to de-initialize ESP-NOW
 */
void espNowDeinit(void)
{
    close(socketFd);
#if USING_WINDOWS
    WSACleanup();
#endif
}

/**
 * This is a wrapper for esp_now_send. It also sets the wifi power with
 * wifi_set_user_fixed_rate()
 *
 * @param data    The data to broadcast using ESP NOW
 * @param dataLen The length of the data to broadcast
 */
void espNowSend(const char* data, uint8_t dataLen)
{
    struct sockaddr_in broadcastAddr; // Broadcast address

    // Construct local address structure
    memset(&broadcastAddr, 0, sizeof(broadcastAddr));   // Zero out structure
    broadcastAddr.sin_family = AF_INET;                 // Internet address family
    broadcastAddr.sin_addr.s_addr = htonl(INADDR_NONE); // Broadcast IP address  // inet_addr("255.255.255.255");
    broadcastAddr.sin_port = htons(ESP_NOW_PORT);       // Broadcast port

    // Tack on ESP-NOW header and randomized MAC address
    char espNowPacket[dataLen + 24];
    uint8_t mac[6] = {0};
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    sprintf(espNowPacket, "ESP_NOW-%02X%02X%02X%02X%02X%02X-", mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
    int hdrLen = strlen(espNowPacket);
    memcpy(&espNowPacket[hdrLen], data, dataLen);

    // For the callback
    uint8_t bcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    // Send the packet
    int sentLen = sendto(socketFd, espNowPacket, hdrLen + dataLen, 0, (struct sockaddr *)&broadcastAddr, sizeof(broadcastAddr));
    if (sentLen != (hdrLen + dataLen))
    {
        ESP_LOGE("WIFI", "sendto() sent a different number of bytes than expected");
        hostEspNowSendCb(bcastMac, ESP_NOW_SEND_FAIL);
    }
    else
    {
        hostEspNowSendCb(bcastMac, ESP_NOW_SEND_SUCCESS);
    }
}

/**
 * Check the ESP NOW receive queue. If there are any received packets, send
 * them to hostEspNowRecvCb()
 */
void checkEspNowRxQueue(void)
{
    char recvString[MAXRECVSTRING+1]; // Buffer for received string
    int recvStringLen;                // Length of received string

    // While we've received a packet
    while ((recvStringLen = recvfrom(socketFd, recvString, MAXRECVSTRING, 0, NULL, 0)) > 0)
    {
        // If the packet matches the ESP_NOW format
        uint8_t recvMac[6] = {0};
        if(6 == sscanf(recvString, "ESP_NOW-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX-",
                       &recvMac[0],
                       &recvMac[1],
                       &recvMac[2],
                       &recvMac[3],
                       &recvMac[4],
                       &recvMac[5]))
        {
            // Make sure the MAC differs from our own
            uint8_t ourMac[6] = {0};
            esp_wifi_get_mac(WIFI_IF_STA, ourMac);
            if(0 != memcmp(recvMac, ourMac, sizeof(ourMac)))
            {
                // If it does, send it to the application through the callback
                hostEspNowRecvCb(recvMac, &recvString[21], recvStringLen - 21, 0);
            }
        }
    }
}

/**
  * @brief     Get mac of specified interface
  *
  * @param      ifx  interface
  * @param[out] mac  store mac of the interface ifx
  *
  * @return
  *    - ESP_OK: succeed
  *    - ESP_ERR_WIFI_NOT_INIT: WiFi is not initialized by esp_wifi_init
  *    - ESP_ERR_INVALID_ARG: invalid argument
  *    - ESP_ERR_WIFI_IF: invalid interface
  */
esp_err_t esp_wifi_get_mac(wifi_interface_t ifx UNUSED, uint8_t mac[6])
{
    // Randomly generate MAC
    static uint8_t randMac[6] = {0};
    static bool macRandomized = false;
    if(!macRandomized)
    {
        macRandomized = true;
        randMac[0] = esp_random();
        randMac[1] = esp_random();
        randMac[2] = esp_random();
        randMac[3] = esp_random();
        randMac[4] = esp_random();
        randMac[5] = esp_random();
    }
    memcpy(mac, randMac, sizeof(randMac));
    return ESP_OK;
}
