#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <vector>
#include <string.h>
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <unistd.h>
#include <cstdlib>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h> 
#include <sstream>
#include <string>
#include <netdb.h>
#include <algorithm>

#include "message.h"
#include "client.h"

using namespace std;

int main(int argc, char* argv[])
{
	unsigned short udp_port = 0;
	const char* server_host = "127.0.0.1";
	//process input arguments
	if ((argc != 3) && (argc != 5))
	{
		cout << "Usage: " << argv[0];
		cout << " [-s <server_host>] -p <udp_port>" << endl;
		return 1;
	}
	else
	{
		//system("clear");
		for (int i = 1; i < argc; i++)
		{
			if (strcmp(argv[i], "-p") == 0)
				udp_port = (unsigned short)atoi(argv[++i]);
			else if (strcmp(argv[i], "-s") == 0)
			{
				server_host = argv[++i];
				if (argc == 3)
				{
					cout << "Usage: " << argv[0];
					cout << " [-s <server_host>] -p <udp_port>" << endl;
					return 1;
				}
			}
			else
			{
				cout << "Usage: " << argv[0];
				cout << " [-s <server_host>] -p <udp_port>" << endl;
				return 1;
			}
		}
	}

	Client_State_T client_state = WAITING;
	string in_cmd;

	//UDP 
	int sk_UDP;

	struct sockaddr_in addr_server;
	bzero(&addr_server, sizeof(addr_server));
	socklen_t addr_len = sizeof(addr_server);
	addr_server.sin_family = AF_INET;
	addr_server.sin_addr.s_addr = inet_addr(server_host);
	addr_server.sin_port = htons(udp_port);

	char files_name[FILE_NAME_LEN];
	char data_buf[DATA_BUF_LEN];

	bzero(&files_name, sizeof(files_name));
	bzero(&data_buf, sizeof(data_buf));

	sk_UDP = socket(AF_INET, SOCK_DGRAM, 0);
	if (sk_UDP < 0) {
		cout << " - initialize socket failed." << endl;
		exit(1);
	}

	int status = 1;
	setsockopt(sk_UDP, SOL_SOCKET, SO_REUSEADDR | SO_BROADCAST, &status, sizeof(status));

	while (true)
	{
		usleep(100);
		switch (client_state)
		{
		case WAITING:
		{
			cout << "$ ";
			cin >> in_cmd;

			if (in_cmd == "ls")
			{
				client_state = PROCESS_LS;
			}
			else if (in_cmd == "send")
			{
				client_state = PROCESS_SEND;
			}
			else if (in_cmd == "remove")
			{
				client_state = PROCESS_REMOVE;
			}
			else if (in_cmd == "shutdown")
			{
				client_state = SHUTDOWN;
			}
			else if (in_cmd == "quit")
			{
				client_state = QUIT;
			}
			else
			{
				cout << " - wrong command." << endl;
				client_state = WAITING;
			}
			break;
		}
		case PROCESS_LS:
		{
			client_state = WAITING;

			// Sender
			Cmd_Msg_T CMD_LS;
			memset(&CMD_LS, 0, sizeof(CMD_LS));

			CMD_LS.cmd = 1;
			CMD_LS.error = 0;

			if (sendto(sk_UDP, &CMD_LS, sizeof(CMD_LS), 0, (struct sockaddr*) & addr_server, sizeof(addr_server)) < 0)
			{
				cout << " - PROCESS_LS sendto error!!!" << endl;
				break;
			}

			// Receiver
			Cmd_Msg_T CMD_RECV;
			memset(&CMD_RECV, 0, sizeof(CMD_RECV));

			if (recvfrom(sk_UDP, &CMD_RECV, sizeof(CMD_RECV), 0, (struct sockaddr*) & addr_server, &addr_len) < 0)
			{
				cout << " - PROCESS_LS recvfrom error!!!" << endl;
				break;
			}
			if (CMD_RECV.cmd != 1)
				cout << " - command response error." << endl;
			else if (CMD_RECV.size == 0)
				cout << " - server backup folder is empty" << endl;
			else
			{
				cout << " - number of files: " << CMD_RECV.size << endl;
				Cmd_Msg_T CMD_FILE;
				memset(&CMD_FILE, 0, sizeof(CMD_FILE));
				for (int i = 0; i < CMD_RECV.size; i++) {
					if (recvfrom(sk_UDP, &CMD_FILE, sizeof(CMD_FILE), 0, (struct sockaddr*) & addr_server, &addr_len) < 0)
					{
						cout << " - PROCESS_LS recvfrom error!!!" << endl;
						break;
					}
					else if (CMD_RECV.cmd != 1)
						cout << " - command response error." << endl;
					else
						cout << " - " << CMD_FILE.filename << endl;
					memset(&CMD_FILE, 0, sizeof(CMD_FILE));
				}
			}
			break;
		}
		case PROCESS_SEND:
		{
			client_state = WAITING;

			Cmd_Msg_T CMD_SD;
			memset(&CMD_SD, 0, sizeof(CMD_SD));

			CMD_SD.cmd = 2;
			CMD_SD.error = 0;

			string SD_FILE;
			cin >> SD_FILE;

			ifstream files_read(SD_FILE.data(), ios::in | ios::binary);
			if (!files_read.is_open())
			{
				cout << " - PROCESS_SEND file open error!!!"  << endl;
				break;
			}
			
			strcpy(CMD_SD.filename, SD_FILE.c_str());

			int size;
			FILE* fp = fopen(CMD_SD.filename, "r");
			if (!fp)
				size = -1;
			else
			{
				fseek(fp, 0L, SEEK_END);
				size = ftell(fp);
				fclose(fp);
			}
			CMD_SD.size = size;

			if (sendto(sk_UDP, &CMD_SD, sizeof(CMD_SD), 0, (struct sockaddr*) & addr_server, sizeof(addr_server)) < 0)
			{
				cout << " - PROCESS_SEND sendto error!!!" << endl;
				break;
			}

			Cmd_Msg_T SD_STATUS;
			memset(&SD_STATUS, 0, sizeof(SD_STATUS));
			if (recvfrom(sk_UDP, &SD_STATUS, sizeof(SD_STATUS), 0, (struct sockaddr*) & addr_server, &addr_len) < 0)
			{
				cout << " - PROCESS_SEND recvfrom error!!!" << endl;
				break;
			}
			else if (SD_STATUS.cmd != 2) {
				cout << " - command error." << endl;
			}
			else
			{
				if (SD_STATUS.error == 2)
				{
					string OVERWRITE;
					cout << "- file exists. Overwrite? <y,n>:" << endl;
					cin >> OVERWRITE;

					if (OVERWRITE == "y" || OVERWRITE == "Y")
					{
						Cmd_Msg_T OW_FILE;
						memset(&OW_FILE, 0, sizeof(OW_FILE));
						OW_FILE.error = 0;
						OW_FILE.cmd = 2;
						if (sendto(sk_UDP, &OW_FILE, sizeof(OW_FILE), 0, (struct sockaddr*) & addr_server, sizeof(addr_server)) < 0)
						{
							cout << " - PROCESS_SEND OW_FILE sendto error!!!" << endl;
							break;
						}
					}
					else
					{
						Cmd_Msg_T OW_FILE;
						memset(&OW_FILE, 0, sizeof(OW_FILE));
						OW_FILE.error = 2;
						OW_FILE.cmd = 2;
						if (sendto(sk_UDP, &OW_FILE, sizeof(OW_FILE), 0, (struct sockaddr*) & addr_server, sizeof(addr_server)) < 0)
							cout << " - PROCESS_SEND OW_FILE sendto error!!!" << endl;
						break;
					}
				}
			}

			Cmd_Msg_T SD_PORT;
			memset(&SD_PORT, 0, sizeof(SD_PORT));
			if (recvfrom(sk_UDP, &SD_PORT, sizeof(SD_PORT), 0, (struct sockaddr*) & addr_server, &addr_len) < 0)
			{
				cout << " - PROCESS_SEND SD_PORT recvfrom error!!!" << endl;
				break;
			}
			if (SD_PORT.cmd != 2) {
				cout << " - command error." << endl;
				break;
			}
			
			if (SD_PORT.error == 1) {
				cout << " - PROCESS_SEND SD_PORT file open failed!!!" << endl;
				break;
			}
			else {
				cout << "- file size: " << CMD_SD.size << endl;
				cout << "- TCP port: " << SD_PORT.port << endl;
			}
			
			// TCP
			int sk_TCP;
			
			struct sockaddr_in addr_server_tcp;
			bzero(&addr_server_tcp, sizeof(addr_server_tcp));
			socklen_t server_addr_tcp_length = sizeof(addr_server_tcp);
			addr_server_tcp.sin_family = AF_INET;
			addr_server_tcp.sin_port = htons(5000);
			
			sk_TCP = socket(AF_INET, SOCK_STREAM, 0);
			if (sk_TCP < 0)
			{
				cout << " - initialize socket failed!!!" << endl;
				exit(1);
			}

			if(inet_pton(AF_INET, server_host, &addr_server_tcp.sin_addr) <= 0)
			{
				cout << " - inet_pton error!!!" << endl;
				break;
            }

			if (connect(sk_TCP, (sockaddr*)&addr_server_tcp, server_addr_tcp_length) < 0)
			{
				cout << " - TCP connection error!!!" << endl;
				break;
			}

			fp = fopen(CMD_SD.filename, "r");
			if (!fp)
				size = -1;
			else
			{
				fseek(fp, 0L, SEEK_END);
				size = ftell(fp);
				fclose(fp);
			}
			
			int FILE_SIZE = size;
			char SD_BUF[DATA_BUF_LEN];
			bzero(&SD_BUF, sizeof(SD_BUF));

			while(FILE_SIZE > 0)
			{
				if(FILE_SIZE > DATA_BUF_LEN)
				{
					files_read.read(SD_BUF, DATA_BUF_LEN);
					int SD_LEN = send(sk_TCP, SD_BUF, DATA_BUF_LEN, 0);
					if (SD_LEN < 0)
					{
						cout << " - send file length error!!!" << endl;
						exit(1);
					}
					cout << " - send " << DATA_BUF_LEN <<" bytes." <<endl;
					bzero(&SD_BUF, sizeof(SD_BUF));
					FILE_SIZE -= DATA_BUF_LEN;
				}
				else
				{
				    files_read.read(SD_BUF, FILE_SIZE);
				    int SD_LEN = send(sk_TCP, SD_BUF, FILE_SIZE, 0);
				    if (SD_LEN < 0)
				    {
						cout << " - send file length error!!!" << endl;
					    exit(1);
					}
					cout << " - Send " << FILE_SIZE << " bytes" << endl;
					cout << " - file transmission is completed" << endl;
					FILE_SIZE = 0;
				}
			}

			close(sk_TCP);
			files_read.close();
		
			if(FILE_SIZE != 0)
			{
				cout << " - send file error!!!" << endl;
				break;
			}

			Cmd_Msg_T SD_END;
			memset(&SD_END, 0, sizeof(SD_END));
			if (recvfrom(sk_UDP, &SD_END, sizeof(SD_END), 0, (struct sockaddr*) & addr_server, &addr_len) < 0)
			{
				cout << " - SD_END recvfrom error!!!" << endl;
				break;
			}
			if (SD_END.error == 0)
				cout << "- file " << SD_FILE << " transfer successful!" << endl;
			else
				cout << "- transmission is failed" << endl;
			break;
		}
		case PROCESS_REMOVE:
		{
			client_state = WAITING;

			Cmd_Msg_T CMD_RM;
			memset(&CMD_RM, 0, sizeof(CMD_RM));

			CMD_RM.cmd = 4;
			CMD_RM.error = 0;

			string FILE_RM;
			cin >> FILE_RM;
			
			ifstream files_read(FILE_RM.data(), ios::in | ios::binary);

			if (!files_read.is_open())
			{
				cout << " - PROCESS_REMOVE file open error!!!" << endl;
				break;
			}
			strcpy(CMD_RM.filename, FILE_RM.c_str());

			if (sendto(sk_UDP, &CMD_RM, sizeof(CMD_RM), 0, (struct sockaddr*) & addr_server, sizeof(addr_server)) < 0)
			{
				cout << " - PROCESS_REMOVE sendto error!!!" << endl;
				break;
			}
			//cout << CMD_RM.filename << endl;

			Cmd_Msg_T RM_END;
			memset(&RM_END, 0, sizeof(RM_END));
			if (recvfrom(sk_UDP, &RM_END, sizeof(RM_END), 0, (struct sockaddr*) & addr_server, &addr_len) < 0)
			{
				cout << " - PROCESS_REMOVE recvfrom error!!!" << endl;
				break;
			}
			else if (RM_END.cmd == 7) 
			{
				if (RM_END.error == 0)
					cout << " - " << FILE_RM << " is removed." << endl;
			}
			else if (RM_END.error == 1)
				cout << " - file doesn't exist." << endl;
			break;
		}
		case SHUTDOWN:
		{
			client_state = WAITING;

			Cmd_Msg_T CMD_ST;
			memset(&CMD_ST, 0, sizeof(CMD_ST));

			CMD_ST.cmd = 5;
			CMD_ST.error = 0;

			if (sendto(sk_UDP, &CMD_ST, sizeof(CMD_ST), 0, (struct sockaddr*) & addr_server, sizeof(addr_server)) < 0)
			{
				cout << " - SHUTDOWN sendto error!!!" << endl;
				break;
			}

			Cmd_Msg_T ST_END;
			memset(&ST_END, 0, sizeof(ST_END));
			if (recvfrom(sk_UDP, &ST_END, sizeof(ST_END), 0, (struct sockaddr*) & addr_server, &addr_len) < 0)
			{
				cout << " - SHUTDOWN recvfrom error!!!" << endl;
				break;
			}
			else if (ST_END.cmd == 7)
			{
				if (ST_END.error == 0)
					cout << " - already shut down." << endl;
			}
			break;
		}
		case QUIT:
		{
			exit(1);
		}
		default:
		{
			client_state = WAITING;
			break;
		}
		}
	}
	return 0;
}
