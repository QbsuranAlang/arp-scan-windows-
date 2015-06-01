#include <WinSock2.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <dnet.h>
#include "getopt.h"
#pragma comment(lib, "Ws2_32.lib") //³sµ²lib
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "dnet.lib")

arp_t *arp_handle;

void controlc() {
	printf("Enter Ctrl-C to Abort. Please waiting...\n");
	WSACleanup();
	exit(0);
}

u_long LookupAddress(const char* pcHost)
{
	in_addr Address;
	u_long nRemoteAddr = inet_addr(pcHost);
	bool was_a_name = false;

	if (nRemoteAddr == INADDR_NONE) {
		// pcHost isn't a dotted IP, so resolve it through DNS
		hostent* pHE = gethostbyname(pcHost);

		if (pHE == 0) {
			return INADDR_NONE;
		}
		nRemoteAddr = *((u_long*)pHE->h_addr_list[0]);
		was_a_name = true;

	}
	if (was_a_name) {
		memcpy(&Address, &nRemoteAddr, sizeof(u_long)); 
		printf("DNS: %s, is %s\n", pcHost, inet_ntoa(Address));
	}
	return nRemoteAddr;
}

DWORD WINAPI sendAnARP(LPVOID lpArg)
{
	u_long DestIp = *(int *)lpArg;

	IPAddr SrcIp = 0;       /* default for src ip */
	ULONG MacAddr[2];       /* for 6-byte hardware addresses */
	ULONG PhysAddrLen = 6;  /* default to length of six bytes */
	BYTE *bPhysAddr;
	DWORD dwRetVal;
	//get reply time
	LARGE_INTEGER response_timer1;
	LARGE_INTEGER response_timer2;
	LARGE_INTEGER cpu_frequency;
	double response_time;

	memset(&MacAddr, 0xff, sizeof (MacAddr));
	PhysAddrLen = 6;

	SetThreadAffinityMask(GetCurrentThread(), 1); 
	QueryPerformanceFrequency((LARGE_INTEGER *)&cpu_frequency);

	//sned arp and wait for reply
	QueryPerformanceCounter((LARGE_INTEGER *) &response_timer1);
	dwRetVal = SendARP(DestIp, SrcIp, &MacAddr, &PhysAddrLen);
	QueryPerformanceCounter((LARGE_INTEGER *) &response_timer2);

	response_time = ( (double) ( (response_timer2.QuadPart - response_timer1.QuadPart) * (double) 1000.0 / (double) cpu_frequency.QuadPart) );

	//output
	bPhysAddr = (BYTE *) & MacAddr;

	if (PhysAddrLen) {
		char message[256];
		memset(message, 0, 256);
		sprintf(message, "Reply that ");

		for (int i = 0; i < (int) PhysAddrLen; i++) {
			if (i == (PhysAddrLen - 1))
				sprintf(message, "%s%.2X", message, (int)bPhysAddr[i]);
			else
				sprintf(message, "%s%.2X:" ,message, (int)bPhysAddr[i]);
		}
		
		struct in_addr ip_addr;
		ip_addr.s_addr = DestIp;
		sprintf(message, "%s is %s in %f", message, inet_ntoa(ip_addr), response_time);
		printf("%s\n", message);
	}
	return NULL;
}

u_long GetFirstIP(u_long Ip, u_char slash)
{
	IPAddr newDestIp = 0;
	IPAddr oldDestIp = ntohl(Ip);
	u_long mask = 1 << (32 - slash);
	for(int i = 0 ; i < slash ; i++)
	{
		newDestIp += oldDestIp & mask;
		mask <<= 1;
	}
	//skip network IP
	return ntohl(++newDestIp);
}

int change_ARP(const struct arp_entry *object, void *p)
{
	char *ip = addr_ntoa(&object->arp_pa);
	char *mac = addr_ntoa(&object->arp_ha);
	if(!strcmp(mac, "00:00:00:00:00:00") || !strcmp(ip, "0.0.0.0"))
		return -1;

	arp_add(arp_handle, object);
	fprintf(stdout, "%s at %s changed to static.\n", ip, mac);
	return 0;
}

int main(int argc, char *argv[])
{
	char *DestIpString = NULL;
	u_char slash = 32;
	int c;
	int staticARP = 0;

	while ((c = getopt(argc, argv, "t:s")) != EOF)
	{
		switch(c)
		{
		case 't':
			DestIpString = optarg;
			break;
		case 's':
			staticARP = 1;
			break;
		}
	}//end while to read option
	if(!DestIpString)
	{
		fprintf(stderr, "Option:\n");
		fprintf(stderr, "\t-t , target ,format: IP/slash | IP\n");
		fprintf(stderr, "\t-s , -s , make your arp cache to static, the option is optional\n");
		fprintf(stderr, "Usage:\n");
		fprintf(stderr, "\tarp-scan -t IP/slash [-s]\n");
		fprintf(stderr, "\tarp-scan -t IP\n");
		exit(1);
	}

	//get arugment
	strtok(DestIpString, "//");
	char *slash_str = strtok(NULL, "");
	if(slash_str)
		slash = atoi(slash_str);

	//translate to ulong
	IPAddr DestIp = LookupAddress(DestIpString);

	//move to first IP
	if(slash != 32)
		DestIp = GetFirstIP(DestIp, slash);
	
	if (DestIp == INADDR_NONE) {
		fprintf(stderr, "Error: could not determine IP address for %s\n", DestIpString);
		return 255;
	}

	SetConsoleCtrlHandler((PHANDLER_ROUTINE)&controlc, TRUE);
	
	u_long loopcount = 1 << (32 - slash);
	HANDLE *hHandles = (HANDLE *)malloc(sizeof(HANDLE) * loopcount);
	DWORD ThreadId;

	for(u_long i = 0 ; i < loopcount ; i++)
	{
		struct in_addr ip_addr;
		ip_addr.s_addr = DestIp;
		
		hHandles[i] = CreateThread(NULL, 0, sendAnARP , &DestIp, 0, &ThreadId);
		
		if (hHandles[i] == NULL) {
			fprintf(stderr, "Could not create Thread.\n");
			exit(1);
		}
		
		Sleep(10);
		u_long temp = ntohl(DestIp);
		DestIp = ntohl(++temp);

	}
	
	for (int i = 0 ; i < loopcount ; i++) {
		WaitForSingleObject(hHandles[i], INFINITE);
	}

	WSACleanup();

	if(!staticARP)
		exit(0);

	if(!(arp_handle = arp_open()))
	{
		fprintf(stderr, "Couldn't set arp to static.\n");
		exit(1);
	}//end if

	arp_loop(arp_handle, change_ARP, NULL);
	arp_close(arp_handle);
	exit(0);
}