#include <WinSock2.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <stdbool.h>
#include "getopt.h"
#include "vendor.h"
//link to librarys
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")

struct vendor_list vendlist;

static void usage(char *cmd) {
	fprintf(stderr, "Usage: %s -t [IP/slash] or [IP]\n", cmd);
	exit(1);
}//end usage

static void controlc() {
	printf("Enter Ctrl-C to Abort. Please waiting...\n");
	WSACleanup();
	exit(0);
}//end controlc

static u_long lookupAddress(const char* pcHost) {
	struct in_addr address;
	struct hostent *pHE = NULL;
	u_long nRemoteAddr;
	bool was_a_name = false;

	nRemoteAddr = inet_addr(pcHost);

	if (nRemoteAddr == INADDR_NONE) {
		// pcHost isn't a dotted IP, so resolve it through DNS
		pHE = gethostbyname(pcHost);

		if (pHE == 0) {
			return INADDR_NONE;
		}//end if
		nRemoteAddr = *((u_long*)pHE->h_addr_list[0]);
		was_a_name = true;
	}
	if (was_a_name) {
		memcpy(&address, &nRemoteAddr, sizeof(u_long));
		printf("DNS: %s, is %s\n", pcHost, inet_ntoa(address));
	}//end if
	return nRemoteAddr;
}//end lookupAddress

static DWORD WINAPI sendAnARP(LPVOID lpArg) {
	u_long DestIp = *(int *)lpArg;

	IPAddr SrcIp = 0;       /* default for src ip */
	ULONG MacAddr[2];       /* for 6-byte hardware addresses */
	ULONG PhysAddrLen = 6;  /* default to length of six bytes */
	BYTE *bPhysAddr;
	DWORD dwRetVal;

	memset(&MacAddr, 0xff, sizeof(MacAddr));
	PhysAddrLen = 6;

	SetThreadAffinityMask(GetCurrentThread(), 1);

	// send arp request
	dwRetVal = SendARP(DestIp, SrcIp, &MacAddr, &PhysAddrLen);

	//output
	bPhysAddr = (BYTE *)& MacAddr;

	if (PhysAddrLen) {
		char message[256];
		char macaddr[256];
		int i;
		memset(message, 0, sizeof(message));
		memset(macaddr, 0, sizeof(macaddr));

		for (i = 0; i < (int)PhysAddrLen; i++) {
			sprintf(macaddr, "%s%.2X", macaddr, ((int) bPhysAddr[i]));
			if (i == (PhysAddrLen - 1))
				sprintf(message, "%s%.2X", message, (int)bPhysAddr[i]);
			else
				sprintf(message, "%s%.2X:", message, (int)bPhysAddr[i]);
		}//end for

		struct in_addr ip_addr;
		ip_addr.s_addr = DestIp;
		printf("%s\t%s\t%s\n", inet_ntoa(ip_addr), message, vendor_list_lookup(&vendlist, macaddr));
	}//end if
	return 0;
}//end sendAnARP

static u_long getFirstIP(u_long Ip, u_char slash) {
	IPAddr newDestIp = 0;
	int i;
	IPAddr oldDestIp = ntohl(Ip);
	u_long mask = 1 << (32 - slash);
	for (i = 0; i < slash; i++) {
		newDestIp += oldDestIp & mask;
		mask <<= 1;
	}//end for
	//skip network IP
	return ntohl(++newDestIp);
}//end getFirstIP

#define WSA_VERSION MAKEWORD(2, 2) // using winsock 2.2
static bool init_winsock() {
	WSADATA	WSAData = { 0 };
	if (WSAStartup(WSA_VERSION, &WSAData) != 0) {
		// Tell the user that we could not find a usable WinSock DLL.
		if (LOBYTE(WSAData.wVersion) != LOBYTE(WSA_VERSION) ||
			HIBYTE(WSAData.wVersion) != HIBYTE(WSA_VERSION)) {
			fprintf(stderr, "WSAStartup(): Incorrect winsock version\n");
		}//end if
		WSACleanup();
		return false;
	}//end if
	return true;
}//end init_winsock

int main(int argc, char *argv[]) {
	char *destIpStr = NULL;
	u_char slash = 32;
	int c;
	IPAddr destIp = 0;
	u_long loopcount;
	HANDLE *hHandles;
	DWORD ThreadId;
	u_long i = 0;
	char* vendorfile_path;

	while ((c = getopt(argc, argv, "t:")) != EOF) {
		switch (c) {
		case 't':
			destIpStr = optarg;
			break;
		default:
			usage(argv[0]);
		}//end while
	}//end while to read option

	if (!destIpStr) {
		usage(argv[0]);
	}//end if usage

	if (!init_winsock()) {
		exit(1);
	}//end if

	//get arugment
	strtok(destIpStr, "//");
	char *slash_str = strtok(NULL, "");
	if (slash_str) {
		slash = atoi(slash_str);
	}//end if

	//translate to ulong
	destIp = lookupAddress(destIpStr);

	//move to first IP
	if (slash != 32) {
		destIp = getFirstIP(destIp, slash);
	}//end if

	if (destIp == INADDR_NONE) {
		fprintf(stderr, "Error: could not determine IP address for %s\n", destIpStr);
		exit(1);
	}//end if

	//set control-c handler
	SetConsoleCtrlHandler((PHANDLER_ROUTINE)&controlc, TRUE);

	for (i = strlen(argv[0]) - 1; argv[0][i] != '\\' && argv[0][i] != '/'; --i);
	vendorfile_path = calloc(i + sizeof("mac-vendor.txt") + 1, 1);
	memcpy(vendorfile_path, argv[0], i);
	strcat(vendorfile_path, "\\mac-vendor.txt");
	vendor_list_init(&vendlist, vendorfile_path);

	loopcount = 1 << (32 - slash);
	hHandles = (HANDLE *)malloc(sizeof(HANDLE) * loopcount);

	//inject
	for (i = 0; i < loopcount; i++) {
		struct in_addr ip_addr;
		ip_addr.s_addr = destIp;

		hHandles[i] = CreateThread(NULL, 0, sendAnARP, &destIp, 0, &ThreadId);

		if (hHandles[i] == NULL) {
			fprintf(stderr, "Could not create Thread.\n");
			exit(1);
		}//end if

		Sleep(10);
		u_long temp = ntohl(destIp);
		destIp = ntohl(++temp);
	}//end for

	for (i = 0; i < loopcount; i++) {
		WaitForSingleObject(hHandles[i], INFINITE);
	}//end for

	vendor_list_free(&vendlist);
	WSACleanup();

	exit(0);
}//end main