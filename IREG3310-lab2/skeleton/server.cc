#include <sys/types.h>
#include <sys/stat.h>
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

#include "message.h"
#include "server.h"

using namespace std;

Server_State_T server_state;
string cmd_string[] = { " ", "CMD_LS", "CMD_SEND","CMD_GET","CMD_REMOVE", "CMD_SHUTDOWN" };

int main(int argc, char* argv[])
{
	unsigned short udp_port = 0;
	if ((argc != 1) && (argc != 3))
	{
		cout << "Usage: " << argv[0];
		cout << " [-p <udp_port>]" << endl;
		return 1;
	}
	else
	{
		//system("clear");
		//process input arguments
		for (int i = 1; i < argc; i++)
		{
			if (strcmp(argv[i], "-p") == 0)
				udp_port = (unsigned short)atoi(argv[++i]);
			else
			{
				cout << "Usage: " << argv[0];
				cout << " [-p <udp_port>]" << endl;
				return 1;
			}
		}
	}

	checkDirectory("./backup");

    char DIR[100];
    getcwd(DIR, sizeof(DIR));
	cout << " - current working directory: " << DIR << endl;

	int sk_UDP;

	struct sockaddr_in addr_server;
	memset(&addr_server, 0, sizeof(addr_server));
	addr_server.sin_family = AF_INET;
	addr_server.sin_addr.s_addr = htonl(INADDR_ANY);
	addr_server.sin_port = htons(udp_port);
		
	sk_UDP = socket(AF_INET, SOCK_DGRAM, 0);
	if (sk_UDP < 0)
	{
		cout << " - initialize socket failed." << endl;
		exit(1);
	}

	if (bind(sk_UDP, (struct sockaddr*) & addr_server, sizeof(addr_server)) == -1)
	{
		cout << " - bind socket failed." << endl;
		exit(1);
	}

	int status = 1;
	setsockopt(sk_UDP, SOL_SOCKET, SO_REUSEADDR | SO_BROADCAST, &status, sizeof(status));

	
	while (true)
	{
		cout << " Waiting UDP command@:" << udp_port << endl;
		struct sockaddr_in addr_client;
		socklen_t addr_len = sizeof(addr_client);

		bool existORnot;

		Cmd_Msg_T CMD_NUM;
		memset(&CMD_NUM, 0, sizeof(CMD_NUM));

		usleep(100);

		if (recvfrom(sk_UDP, &CMD_NUM, sizeof(CMD_NUM), 0, (struct sockaddr*) & addr_client, &addr_len) < 0)
		{
			cout << " - recvfrom error!!!" << endl;
			exit(1);
		}

		if (CMD_NUM.cmd == 1)
		{
			server_state = PROCESS_LS;
		}

		else if (CMD_NUM.cmd == 2)
		{
			server_state = PROCESS_SEND;
		}

		else if (CMD_NUM.cmd == 4)
		{
			server_state = PROCESS_REMOVE;
		}

		else if (CMD_NUM.cmd == 5)
		{
			server_state = SHUTDOWN;
		}
		else
		{
			server_state = WAITING;
		}

		switch (server_state)
		{
		case WAITING:
		{
			// Do nothing
			break;
		}
		case PROCESS_LS:
		{
			server_state = WAITING;
			cout << "[CMD RECEIVED]: CMD_LS" << endl;

			vector <string> files;

			getDirectory("./backup", files);

			int num_of_files = files.size();

			Cmd_Msg_T FILE_SIZE;
			memset(&FILE_SIZE, 0, sizeof(FILE_SIZE));

			FILE_SIZE.size = num_of_files;
			FILE_SIZE.error = 0;
			FILE_SIZE.cmd = 1;
			sendto(sk_UDP, &FILE_SIZE, sizeof(FILE_SIZE), 0, (struct sockaddr*)& addr_client, sizeof(addr_client));

			if (num_of_files == 0)
			{
				cout << " - server backup folder is empty" << endl;
			}
			

			Cmd_Msg_T LS_FILE;
			memset(&LS_FILE, 0, sizeof(LS_FILE));
			for (int i = 0; i < files.size(); i++) {
				cout << " - " << files[i] << endl;

				LS_FILE.cmd = 1;
				LS_FILE.error = 0;
				strcpy(LS_FILE.filename, files[i].c_str());

				sendto(sk_UDP, &LS_FILE, sizeof(LS_FILE), 0, (struct sockaddr*) & addr_client, sizeof(addr_client));
				// Re-initialize LS_FILE
				memset(&LS_FILE, 0, sizeof(LS_FILE));
			}
			// CMD_NUM = Waiting
			memset(&CMD_NUM, 0, sizeof(CMD_NUM));
			break;
		}
		case PROCESS_SEND:
		{
			server_state = WAITING;
			cout << "[CMD RECEIVED]: CMD_SEND" << endl;

			cout << " - filename : " << CMD_NUM.filename << endl;
			cout << " - filesize : " << CMD_NUM.size << endl;
			Cmd_Msg_T FILE_STATUS;
			memset(&FILE_STATUS, 0, sizeof(FILE_STATUS));

			existORnot = checkFile(CMD_NUM.filename, "./backup");

			cout << "File exist?: " << existORnot << endl;

			string filename_s = CMD_NUM.filename;
			string path = "./backup/" + filename_s;

			if (existORnot) 
			{
				FILE_STATUS.error = 2;
				FILE_STATUS.cmd = 2;
				sendto(sk_UDP, &FILE_STATUS, sizeof(FILE_STATUS), 0, (struct sockaddr*) & addr_client, sizeof(addr_client));
				cout << "file " << CMD_NUM.filename << " exist; overwrite?" << endl;

				Cmd_Msg_T FILE_OW;
				memset(&FILE_OW, 0, sizeof(FILE_OW));
				if (recvfrom(sk_UDP, &FILE_OW, sizeof(FILE_OW), 0, (struct sockaddr*) & addr_client, &addr_len) < 0)
				{
					cout << " - overwrite recvfrom error!!!" << endl;
					memset(&CMD_NUM, 0, sizeof(CMD_NUM));
					break;
				}
				if (FILE_OW.error == 0) 
				{
					unlink(path.data());
					cout << " - overwrite the file" << endl;
				}
				else if (FILE_OW.error == 2) 
				{
					memset(&CMD_NUM, 0, sizeof(CMD_NUM));
					cout << " - do not overwrite" << endl;
					break;
				}
				if (FILE_OW.cmd != 2) 
				{
					cout << " - command error." << endl;
					break;
				}
			}
			else
			{
				FILE_STATUS.error = 0;
				FILE_STATUS.cmd = 2;
				sendto(sk_UDP, &FILE_STATUS, sizeof(FILE_STATUS), 0, (struct sockaddr*) & addr_client, sizeof(addr_client));
			}


			// TCP

			int sk_TCP = socket(AF_INET, SOCK_STREAM, 0);
			if (sk_TCP < 0) {
				cout << " - initialize socket failed." << endl;
				exit(1);
			}

			int status = 1;
			setsockopt(sk_TCP, SOL_SOCKET, SO_REUSEADDR, &status, sizeof(status));

			// band
			struct sockaddr_in server_tcp;
			memset(&server_tcp, 0, sizeof(server_tcp));
			server_tcp.sin_family = AF_INET;
			server_tcp.sin_addr.s_addr = htonl(INADDR_ANY);
			server_tcp.sin_port = htons(5000);
			if (bind(sk_TCP, (struct sockaddr*) & server_tcp, sizeof(server_tcp)) < 0)
			{
				cout << " - band socket error!!!" << endl;
				close(sk_TCP);
				exit(1);
			}

			// listen
			if (listen(sk_TCP, 50) < 0)
			{
				cout << " - listen socket error!!!" << endl;
				close(sk_TCP);
				exit(1);
			}
			else
				cout << " - listen @: " << server_tcp.sin_port<<endl;

			// Sender
			char DIR[100];
			getcwd(DIR, sizeof(DIR));
			cout << " - current working directory: " << DIR << endl;

			Cmd_Msg_T FILE;
			memset(&FILE, 0, sizeof(FILE));
			
			ofstream file_write(path.data(), ios::out | ios::binary);

			if (!file_write.is_open()) {
				cout << " - file open error!!!" << endl;
				FILE.error = 1;
				FILE.cmd = 2;
				FILE.port = 0;
				sendto(sk_UDP, &FILE, sizeof(FILE), 0, (struct sockaddr*) & addr_client, sizeof(addr_client));

				close(sk_TCP);
				memset(&CMD_NUM, 0, sizeof(CMD_NUM));
				break;
			}
			else {
				FILE.error = 0;
				FILE.cmd = 2;
				FILE.port = server_tcp.sin_port;
				sendto(sk_UDP, &FILE, sizeof(FILE), 0, (struct sockaddr*) & addr_client, sizeof(addr_client));
			}

			// Receiver
			char FILE_BUF[DATA_BUF_LEN];
			memset(&FILE_BUF, 0, DATA_BUF_LEN);

			int FILE_SIZE = CMD_NUM.size;
			int CURRENT_SIZE = 0;
			while (true) 
			{
				struct sockaddr_in client_tcp;
				memset(&client_tcp, 0, sizeof(client_tcp));
				socklen_t addr_client_tcp_len = sizeof(client_tcp);

				int CLIENT_SK = accept(sk_TCP, (struct sockaddr*) & client_tcp, &addr_client_tcp_len);
				if (CLIENT_SK < 0)
				{
					cout << " - TCP connection error!!!" << endl;
					exit(1);
				}
				else
				{
					cout << " - connected with client" << endl;
				}

				while (FILE_SIZE > 0) {
					char FILE_BUF[DATA_BUF_LEN];
					memset(&FILE_BUF, 0, DATA_BUF_LEN);

					if (FILE_SIZE > DATA_BUF_LEN)
					{
						int readbytes = recv(CLIENT_SK, FILE_BUF, DATA_BUF_LEN, 0);
						if (readbytes == -1)
						{
							cout << " - file receive error!!!" << endl;
							close(sk_TCP);
							break;
						}
						else if (readbytes == 0)
						{
							cout << " - " << CMD_NUM.filename << "has been received." << endl;
							break;
						}
						else
						{
							CURRENT_SIZE = CURRENT_SIZE + readbytes;
							cout << " - total bytes recerived : " << CURRENT_SIZE << endl;
							file_write.write(FILE_BUF, DATA_BUF_LEN);
							FILE_SIZE -= DATA_BUF_LEN;
						}
					}
					else
					{
						int readbytes = recv(CLIENT_SK, FILE_BUF, FILE_SIZE, 0);
						if (readbytes == -1)
						{
							cout << " - file receive error!!!" << endl;
							close(sk_TCP);
							break;
						}
						if (readbytes == FILE_SIZE)
						{
							CURRENT_SIZE = CURRENT_SIZE + readbytes;
							cout << " - total bytes recerived : " << CURRENT_SIZE << endl;
							file_write.write(FILE_BUF, FILE_SIZE);
							FILE_SIZE = 0;
						}
					}

				}
				file_write.close();
				close(sk_TCP);
				break;
			}

			if (FILE_SIZE == 0)
			{
				cout << " - " << CMD_NUM.filename << " has been received." << endl;
				Cmd_Msg_T FILE_END;
				memset(&FILE_END, 0, sizeof(FILE_END));
				FILE.error = 0;
				FILE.cmd = 7;
				sendto(sk_UDP, &FILE_END, sizeof(FILE_END), 0, (struct sockaddr*) & addr_client, sizeof(addr_client));
				cout << " - send acknowledgement" << endl;
			}

			memset(&CMD_NUM, 0, sizeof(CMD_NUM));
			break;
		}
		case PROCESS_REMOVE:
		{
			server_state = WAITING;
			cout << "[CMD RECEIVED]: CMD_REMOVE" << endl;

			Cmd_Msg_T FILE_STATUS;
			memset(&FILE_STATUS, 0, sizeof(FILE_STATUS));
			//cout << CMD_NUM.filename << endl;

			existORnot = checkFile(CMD_NUM.filename, "./backup");

			if (existORnot) {
				string filename_s = CMD_NUM.filename;
				string path = "./backup/" + filename_s;
				if (unlink(path.data()) < 0) {
					cout << "deletion error!!!" << endl;
					break;
				}
				else {
					cout << " - " << path << " has been removed." << endl;
					cout << " - send acknowledgement." << endl;
					FILE_STATUS.error = 0;
					FILE_STATUS.cmd = 7;
					sendto(sk_UDP, &FILE_STATUS, sizeof(FILE_STATUS), 0, (struct sockaddr*) & addr_client, sizeof(addr_client));
				}
			}
			else {
				cout << " - file doesn't exist" << endl;
				cout << " - send acknowledgement." << endl;
				FILE_STATUS.error = 1;
				sendto(sk_UDP, &FILE_STATUS, sizeof(FILE_STATUS), 0, (struct sockaddr*) & addr_client, sizeof(addr_client));
			}

			memset(&CMD_NUM, 0, sizeof(CMD_NUM));
			break;
		}
		case SHUTDOWN:
		{
			cout << "[CMD RECEIVED]: CMD_SHUTDOWN" << endl;

			Cmd_Msg_T ST_END;
			memset(&ST_END, 0, sizeof(ST_END));
			ST_END.error = 0;
			ST_END.cmd = 7;

			sendto(sk_UDP, &ST_END, sizeof(ST_END), 0, (struct sockaddr*) & addr_client, sizeof(addr_client));
			memset(&CMD_NUM, 0, sizeof(CMD_NUM));
			exit(1);
		}
		default:
		{
			memset(&CMD_NUM, 0, sizeof(CMD_NUM));
			server_state = WAITING;
			break;
		}
		}
	}
	return 0;
}




//this function check if the backup folder exist
int checkDirectory(string dir)
{
	DIR* dp;
	if ((dp = opendir(dir.c_str())) == NULL) {
		//cout << " - error(" << errno << ") opening " << dir << endl;
		if (mkdir(dir.c_str(), S_IRWXU) == 0)
			cout << " - Note: Folder " << dir << " does not exist. Created." << endl;
		else
			cout << " - Note: Folder " << dir << " does not exist. Cannot created." << endl;
		return errno;
	}
	closedir(dp);
}


//this function is used to get all the filenames from the
//backup directory
int getDirectory(string dir, vector<string>& files)
{
	DIR* dp;
	struct dirent* dirp;
	if ((dp = opendir(dir.c_str())) == NULL) {
		//cout << " - error(" << errno << ") opening " << dir << endl;
		if (mkdir(dir.c_str(), S_IRWXU) == 0)
			cout << " - Note: Folder " << dir << " does not exist. Created." << endl;
		else
			cout << " - Note: Folder " << dir << " does not exist. Cannot created." << endl;
		return errno;
	}

	int j = 0;
	while ((dirp = readdir(dp)) != NULL) {
		//do not list the file "." and ".."
		if ((string(dirp->d_name) != ".") && (string(dirp->d_name) != ".."))
			files.push_back(string(dirp->d_name));
	}
	closedir(dp);
	return 0;
}
//this function check if the file exists
bool checkFile(char* filename, string dir)
{
	string CM_filename_s = filename;
	//check if file exist
	vector <string> files;
	getDirectory(dir, files);
	int num_of_files = files.size();
	bool existORnot = false;

	for (int i = 0;i < num_of_files;i++) {
		if (files[i] == CM_filename_s) {
			existORnot = true;
			break;
		}
	}
	return existORnot;
}

