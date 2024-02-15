#define __UTILS_CPP__
#include "utils.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>
#include <Arduino.h>
#include <lwip/sockets.h>
#include <lwip/icmp.h>
#include <lwip/ip.h>
#include <lwip/netdb.h>

#include <HTTPClient.h>

// using namespace std; // im lazy
#define BUFFER_LEN_FOR_ARG_CHECK 100
// Automatic port number
#define PORT_NO 0

// Automatic port number
#define PING_SLEEP_RATE 1000000 x
#define PING_PKT_S 64
// Gives the timeout delay for receiving packets
// in seconds
#define RECV_TIMEOUT 1

/* socket variables , defined in amisReader and froniusSolarAPI*/
extern KEY_VALUE_MAP_t amisKeyValueMap;
extern KEY_VALUE_MAP_t froniusKeyValueMap;

static HTTP_REST_TARGET_t restTarget[REST_TARGET_COUNT] = {
	{"Amis reader", "amisreader", {0}, 80, "/rest", "GET /rest HTTP/1.0\r\n\r\n", -1, AMIS_VALUE_COUNT, &amisKeyValueMap},
	{"Fronius Solar API", "fronius rest", {0}, 80, "/status/powerflow", "GET /status/powerflow HTTP/1.0\r\n\r\n", -1, FRONIUS_VALUE_COUNT, &froniusKeyValueMap}};



static bool readJsonResponse(HTTP_REST_TARGET_t *target, WEBSOCK_DATA &webSockData,GET_JSON_DATA getJson);
int pingloop = 1;

// ping packet structure

/* void Serialprintln(const char *input...)
{
	va_list args;
	va_start(args, input);

	for (const char *i = input; *i != 0; ++i)
	{
		if (*i != '%')
		{
			DBG(*i);
			continue;
		}
		switch (*(++i))
		{
		case '%':
			DBG('%');
			break;
		case 's':
			DBG(va_arg(args, char *));
			break;
		case 'd':
			Serial.println(va_arg(args, int), DEC);
			break;
		case 'b':
			Serial.println(va_arg(args, int), BIN);
			break;
		case 'o':
			Serial.println(va_arg(args, int), OCT);
			break;
		case 'x':
			Serial.println(va_arg(args, int), HEX);
			break;
		case 'f':
			Serial.println(va_arg(args, double), 2);
			break;
		}
	}
	DBGln();
	va_end(args);
} */

void printHWInfo()
{
	DBGf("MEM: %d", esp_get_free_heap_size());

	esp_chip_info_t chip_info;
	esp_chip_info(&chip_info);

	DBGf("Hardware info: %d cores Wifi %s%s\n", chip_info.cores, (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
		 (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");
	DBGf("Silicon revision: %d\n", chip_info.revision);

	// get chip id
	uint32_t chipId = ESP.getEfuseMac();

	DBGf("Chip id: %x\n", chipId);
}
bool isNumber(char s[])
{
	for (int i = 0; s[i] != '\0'; i++)
	{
		if (isdigit(s[i]) == 0)
			return false;
	}
	return true;
}

bool floatNum(char *s)
{
	const char *ptr = s;
	double x = strtod(ptr, &s);

	// check if converted to long int
	if (*s == 0)
	{
		return true;
	}
	else
	{
		return false;
	}
	return false;
}

// char ret[INET_ADDRSTRLEN]
void ipv4_int_to_string(char *ret, uint32_t in, bool *const success)
{
	// char ret[INET_ADDRSTRLEN];
	in = htonl(in);
	// const char *inet_ntop(int af, const void *src, char *dst, socklen_t size)
	const bool _success = (NULL != inet_ntop(AF_INET, &in, ret, INET_ADDRSTRLEN));
	if (success)
	{
		*success = _success;
	}
	/* DBG("Return in ipv4_int_to_string ...");
	DBG(", success: ");DBG(_success);DBG(" ,ret: "); DBG(ret); */
	if (_success)
	{
		// ret.pop_back(); // remove null-terminator required by inet_ntop
	}
	else
	{
		char buf[BUFFER_LEN_FOR_ARG_CHECK] = {0};
		strerror_r(errno, buf, sizeof(buf));
		DBGf("Error inipv4_int_to_string  %s", strerror(errno));

		// throw std::runtime_error(String("error converting ipv4 int to String ") + to_string(errno) + String(": ") + String(buf));
		// ret = buf;
	}
}
// return is native-endian
// when an error occurs: if success ptr is given, it's set to false, otherwise a std::runtime_error is thrown.
uint32_t ipv4_string_to_int(char *in, bool *const success)
{
	uint32_t ret;
	const bool _success = (1 == inet_pton(AF_INET, in, &ret));
	ret = ntohl(ret);
	if (success)
	{
		*success = _success;
	}
	else if (!_success)
	{
		char buf[BUFFER_LEN_FOR_ARG_CHECK] = {0};
		strerror_r(errno, buf, sizeof(buf));
		DBGf("Error in ipv4_string_to_int %s", strerror(errno));
		ret = -1;
		strncpy(in, buf, INET_ADDRSTRLEN - 1);
		// throw std::runtime_error(String("error converting ipv4 String to int ") + to_string(errno) + String(": ") + String(buf));
	}
	return ret;
}
bool util_isFieldFilled(const char *key, const char *argument, StaticJsonDocument<JSON_OBJECT_SETUP_LEN> &data)
{
	if (strlen(argument) == 0)
	{
		char buf[BUFFER_LEN_FOR_ARG_CHECK];
		sprintf(buf, "Argument: %s kann nicht leer sein.", key);
		data["error"] = buf;
		DBGf("util_isFieldFilled: %s - empty!", key);

		return false;
	}
	return true;
}

bool util_checkParamInt(const char *key, const char *argument, StaticJsonDocument<JSON_OBJECT_SETUP_LEN> &data, int *result)
{
	if (util_isFieldFilled(key, argument, data))
		*result = atoi(argument);
	else
	{
		DBGf("utilCheckParamInt:  %s - empty", key);

		return false;
	}

	if (*result == 0)
	{
		char buf[BUFFER_LEN_FOR_ARG_CHECK];
		sprintf(buf, "Argument: %s ist kein numerischer Wert.", key);
		data["error"] = buf;
		DBGf("utilCheckParamInt: %s - kein numerischer Wert : %s", key, argument);

		return false;
	}
	return true;
}

bool util_checkParamFloat(const char *key, const char *argument, /* const JsonObject &jsonObj, */ StaticJsonDocument<JSON_OBJECT_SETUP_LEN> &data, float *result)
{
	if (util_isFieldFilled(key, argument, data))
		*result = atof(argument);
	else
	{
		DBGf("util_checkParamFloat: %s - empty", key);

		return false;
	}
	if (*result == 0.0)
	{
		char buf[BUFFER_LEN_FOR_ARG_CHECK];
		sprintf(buf, "Argument: %s ist kein FLießkommawert (z.B. 0.0,...)", key);
		data["error"] = buf;
		DBGf("util_checkParamFloat: %s - kein numerischer Werte: %s", key, argument);

		return false;
	}
	return true;
}

void util_pHW()
{
	esp_chip_info_t chip_info;

	esp_chip_info(&chip_info);

	DBGf("Hardware info");
	Serial.printf("%d cores Wifi %s%s\n", chip_info.cores, (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
				  (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");
	Serial.printf("Silicon revision: %d\n", chip_info.revision);
	Serial.printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
				  (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embeded" : "external");

	// get chip id
	String chipId = String((uint32_t)ESP.getEfuseMac(), HEX);
	chipId.toUpperCase();

	DBGf("Chip id: %s\n", chipId.c_str());
}

char *util_format_Watt_kWatt(double val, char *formatBuf)
{
	if (fabs(val) > 1000.0)
		sprintf(formatBuf, "%.2lf kW", val / 1000);
	else
		sprintf(formatBuf, "%.2lf W", val);
	return formatBuf;
}

String util_GET_Request(const char *url, int *httpResponseCode)
{
	HTTPClient http;
	http.begin(url);
	*httpResponseCode = http.GET();

	String payload = "{}";

	if (*httpResponseCode > 0)
	{
		DBGf("HTTP Response code: %d", *httpResponseCode);
		payload = http.getString();
	}
	else
	{
		DBGf("Error code: %d", *httpResponseCode);
		payload = "";
	}
	http.end();

	return payload;
}

bool utils_sock_initRestTargets( char *host, int index)
{

	char str[INET_ADDRSTRLEN];
	DBGf("utils_sock_initRestTargets - init(), host: %s ", host);
	// store this IP address in sa:
	inet_pton(AF_INET, host, &(restTarget[index].serverAddr.sin_addr));
	// now get it back and print it
	restTarget[index].serverAddr.sin_port = htons(restTarget[index].port);
	restTarget[index].serverAddr.sin_family = AF_INET;
	inet_ntop(AF_INET, &(restTarget[index].serverAddr.sin_addr), str, INET_ADDRSTRLEN);

	restTarget[0].socketFd = socket(AF_INET, SOCK_STREAM, 0);
	if (restTarget[0].socketFd >= 0)
	{
		int retVal = connect(restTarget[index].socketFd, (struct sockaddr *)&restTarget[index].serverAddr,
							 sizeof(restTarget[index].serverAddr));
		close(restTarget[index].socketFd);
		DBGf("utils_sock_initRestTargets - :: IP: %s, RetVal open socket %d", str, retVal);
		return true;
	}
	DBGf("AmisReader not available at IP: %s", host);
	return false;
}

bool utils_sock_readRestTarget(WEBSOCK_DATA &webSockData, int index,GET_JSON_DATA getJson)
{
 restTarget[0].socketFd = socket(AF_INET, SOCK_STREAM, 0);
    if (restTarget[0].socketFd >= 0)
    {
        int retVal = connect(restTarget[index].socketFd, (struct sockaddr *)&restTarget[index].serverAddr,
                             sizeof(restTarget[0].serverAddr));
        if (retVal >= 0 || errno == EISCONN)
        {
            if (!readJsonResponse(&restTarget[index], webSockData,getJson))
            {
                DBGf("amisReader_readRestTarget:: Cannot read Response of socket communication with amisReader ");
                close(restTarget[index].socketFd);
                return false;
            }
            close(restTarget[index].socketFd);
        }
        else
        {
            DBGf("amisReader_readRestTarget:: Socket error %s mit retVal: %d", strerror(errno), retVal);
            return false;
        }
    }
    return true;
}

#define RESPONSE_LENGTH 1024


bool readJsonResponse(HTTP_REST_TARGET_t *target, WEBSOCK_DATA &webSockData,GET_JSON_DATA getJson)
{
    int bytes, sent, received, total;
    char response[RESPONSE_LENGTH];
    int responseLen = sizeof(response);
    char *jsonStart;

    /* send the request */
    total = strlen(target->request);
    sent = 0;
    do
    {
        bytes = write(target->socketFd, target->request + sent, total - sent);
        if (bytes < 0)
            return bytes;
        if (bytes == 0)
            break;
        sent += bytes;
    } while (sent < total);

    /* receive the response */
    memset(response, 0, responseLen);
    total = responseLen - 1;
    received = 0;
    do
    {
        bytes = read(target->socketFd, response + received, total - received);
        if (bytes < 0)
        {
            DBGf("readJsonResponse() - cannot read bytes from socket (bytes <0)");
            return false;
            // return bytes;
        }

        if (bytes == 0)
            break;
        received += bytes;
    } while (received < 1); // total);

    if (received == total)
    {

        DBGf("readJsonResponse() - too much data (receivedd == total)");
        return false;
        return 0;
    }

    jsonStart = strchr(response, '{');
    // DBGf("readJsonResponse() - payload: %s", response);
    if (jsonStart != NULL)
    {
         (*getJson)(target, jsonStart, webSockData);
    }
    return true;
}


#ifdef NOT
struct ping_pkt
{
	struct icmphdr hdr;
	char msg[PING_PKT_S - sizeof(struct icmphdr)];
};
// Calculating the Check Sum
unsigned short checksum(void *b, int len)
{
	unsigned short *buf = b;
	unsigned int sum = 0;
	unsigned short result;

	for (sum = 0; len & gt; 1; len -= 2)
		sum += *buf++;
	if (len == 1)
		sum += *(unsigned char *)buf;
	sum = (sum & gt; > 16) + (sum & amp; 0xFFFF);
	sum += (sum & gt; > 16);
	result = ~sum;
	return result;
}

// Interrupt handler
void intHandler(int dummy) { pingloop = 0; }

// Performs a DNS lookup
char *dns_lookup(char *addr_host,
				 struct sockaddr_in *addr_con)
{
	printf("\nResolving DNS..\n & quot;);
	struct hostent* host_entity;
	char* ip = (char*)malloc(NI_MAXHOST * sizeof(char));
	int i;

	if ((host_entity = gethostbyname(addr_host)) == NULL) {
		// No ip found for hostname
		return NULL;
	}

	// filling up address structure
	strcpy(ip,
		inet_ntoa(*(struct in_addr*)host_entity - >
					h_addr));

	(*addr_con).sin_family = host_entity - >
	h_addrtype;
	(*addr_con).sin_port = htons(PORT_NO);
	(*addr_con).sin_addr.s_addr = *(long*)host_entity - >
	h_addr;

	return ip;
}

// Resolves the reverse lookup of the hostname
char *reverse_dns_lookup(char *ip_addr)
{
	struct sockaddr_in temp_addr;
	socklen_t len;
	char buf[NI_MAXHOST], *ret_buf;

	temp_addr.sin_family = AF_INET;
	temp_addr.sin_addr.s_addr = inet_addr(ip_addr);
	len = sizeof(struct sockaddr_in);

	if (getnameinfo((struct sockaddr *)&temp_addr, len, buf, sizeof(buf), NULL,
					0, NI_NAMEREQD))
	{
		printf(
			"
				Could not resolve reverse lookup of hostname\n &
				quot;);
		return NULL;
	}
	ret_buf = (char *)malloc((strlen(buf) + 1) * sizeof(char));
	strcpy(ret_buf, buf);
	return ret_buf;
}

// make a ping request
void send_ping(int ping_sockfd,
			   struct sockaddr_in *ping_addr,
			   char *ping_dom, char *ping_ip,
			   char *rev_host)
{
	int ttl_val = 64, msg_count = 0, i, addr_len, flag = 1,
		msg_received_count = 0;

	// receive buffer
	// ip header is included when receiving
	// from raw socket
	char rbuffer[128];
	struct ping_pkt *r_pckt;

	struct ping_pkt pckt;
	struct sockaddr_in r_addr;
	struct timespec time_start, time_end, tfs, tfe;
	long double rtt_msec = 0, total_msec = 0;
	struct timeval tv_out;
	tv_out.tv_sec = RECV_TIMEOUT;
	tv_out.tv_usec = 0;

	clock_gettime(CLOCK_MONOTONIC, &tfs);

	// set socket options at ip to TTL and value to 64,
	// change to what you want by setting ttl_val
	if (setsockopt(ping_sockfd, SOL_IP, IP_TTL, &ttl_val, sizeof(ttl_val)) != 0)
	{
		printf(
			"\nSetting socket options to TTL failed !\n
				& quot;);
		return;
	}

	else
	{
		printf("\nSocket set to TTL..\n & quot;);
	}

	// setting timeout of recv setting
	setsockopt(ping_sockfd, SOL_SOCKET, SO_RCVTIMEO,
			   (const char *)&tv_out, sizeof tv_out);

	// send icmp packet in an infinite loop
	while (pingloop)
	{
		// flag is whether packet was sent or not
		flag = 1;

		// filling packet
		bzero(&pckt, sizeof(pckt));

		pckt.hdr.type = ICMP_ECHO;
		pckt.hdr.un.echo.id = getpid();

		for (i = 0; i & lt; sizeof(pckt.msg) - 1; i++)
			pckt.msg[i] = i + '0';

		pckt.msg[i] = 0;
		pckt.hdr.un.echo.sequence = msg_count++;
		pckt.hdr.checksum = checksum(&pckt, sizeof(pckt));

		usleep(PING_SLEEP_RATE);

		// send packet
		clock_gettime(CLOCK_MONOTONIC, &time_start);
		if (sendto(ping_sockfd, &pckt, sizeof(pckt), 0,
				   (struct sockaddr *)ping_addr,
				   sizeof(*ping_addr)) &lt;
			= 0)
		{
			printf("\nPacket Sending Failed !\n
					   & quot;);
			flag = 0;
		}

		// receive packet
		addr_len = sizeof(r_addr);

		if (recvfrom(ping_sockfd,
					 rbuffer, sizeof(rbuffer), 0,
					 (struct sockaddr *)&r_addr, &addr_len) &lt;
			= 0 & amp; &msg_count & gt; 1)
		{
			printf("\nPacket receive failed !\n
					   & quot;);
		}

		else
		{
			clock_gettime(CLOCK_MONOTONIC, &time_end);

			double timeElapsed = ((double)(time_end.tv_nsec - time_start.tv_nsec)) / 1000000.0 rtt_msec = (time_end.tv_sec - time_start.tv_sec) * 1000.0 + timeElapsed;

			// if packet was not sent, don't receive
			if (flag)
			{
				if (!(r_pckt->hdr.type == 0 & amp; &r_pckt->hdr.code == 0))
				{
					printf(" Error..Packet received
								   with ICMP type %
								   d code % d\n &
							   quot;
						   , r_pckt->hdr.type, r_pckt->hdr.code);
				}
				else
				{
					printf(" % d bytes from
							   % s(h
								   : % s)(% s) msg_seq = % d ttl = % d rtt =
																	   % Lf ms.\n & quot;
						   , PING_PKT_S, ping_dom, rev_host,
						   ping_ip, msg_count, ttl_val,
						   rtt_msec);

					msg_received_count++;
				}
			}
		}
	}
	clock_gettime(CLOCK_MONOTONIC, &tfe);
	double timeElapsed = ((double)(tfe.tv_nsec - tfs.tv_nsec)) / 1000000.0;

	total_msec = (tfe.tv_sec - tfs.tv_sec) * 1000.0 + timeElapsed

														  printf("\n == = % s ping statistics ==
																 =\n & quot;
																 , ping_ip);
	printf("\n % d packets sent, % d packets received,
			   % f percent packet loss.Total time
		   : % Lf ms.\n\n & quot;
		   , msg_count, msg_received_count,
		   ((msg_count - msg_received_count) / msg_count) * 100.0,
		   total_msec);
}

// Driver Code
int main(int argc, char *argv[])
{
	int sockfd;
	char *ip_addr, *reverse_hostname;
	struct sockaddr_in addr_con;
	int addrlen = sizeof(addr_con);
	char net_buf[NI_MAXHOST];

	if (argc != 2)
	{
		printf("\nFormat % s & lt;
				   address &
				   gt;\n & quot;
			   , argv[0]);
		return 0;
	}

	ip_addr = dns_lookup(argv[1], &addr_con);
	if (ip_addr == NULL)
	{
		printf(
			"\nDNS lookup
				failed !Could not resolve hostname !\n &
				quot;);
		return 0;
	}

	reverse_hostname = reverse_dns_lookup(ip_addr);
	printf("\nTrying to connect to '%s' IP
		   : % s\n & quot;
		   , argv[1], ip_addr);
	printf("\nReverse Lookup domain
		   : % s & quot;
		   , reverse_hostname);

	// socket()
	sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if (sockfd & lt; 0)
	{
		printf(
			"\nSocket file descriptor not received !!\n
				& quot;);
		return 0;
	}
	else
		printf("\nSocket file descriptor % d received\n
				   & quot;
			   , sockfd);

	signal(SIGINT, intHandler); // catching interrupt

	// send pings continuously
	send_ping(sockfd, &addr_con, reverse_hostname, ip_addr, argv[1]);

	return 0;
}

#endif