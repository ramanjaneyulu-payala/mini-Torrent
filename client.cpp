#include "PeerTask.h"

using namespace std;

pthread_mutex_t IPADDRESS_INFO_MUTEX = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char** argv) {
	if(argc<2) { perror("Error: Port number not given"); exit(0); }
	if(argc<3) { perror("Error: Path of tracker_info.txt not given"); exit(0); }

	char tracker_info[22];
	FILE* fp = fopen(argv[2], "r");
	fscanf(fp, "%[^\n]s", tracker_info);
	fclose(fp);
	vector<string> tracker_info_split;
	boost::split(tracker_info_split, tracker_info, boost::is_any_of(":"));
	TRACKER_IP = tracker_info_split[0];
	TRACKER_PORT = tracker_info_split[1];
	//cout << TRACKER_IP << " " << TRACKER_PORT << "\n";

	pthread_t listening_thread;
	struct thread_arguments_structure* listening_thread_arg = (struct thread_arguments_structure*)malloc(sizeof(struct thread_arguments_structure));
	listening_thread_arg->portNumber = atoi(argv[1]);
	if(pthread_create(&listening_thread, NULL, startListeningPort, (void *) listening_thread_arg)) { perror("Error: Creating listening thread"); exit(0); }
	pthread_detach(listening_thread);

	string command;
	while(1) {
		getline(cin, command);
		if(command.find("download_file") == 0) {
			if(USER == "") {
				cout << "Please login in order to download files\n";
				continue;
			}

			pthread_t download_thread;
			command.append(" ");
			command.append(USER);

			if(pthread_create(&download_thread, NULL, requestFile, (void *) new string(command))) { perror("Error: Creating download thread"); exit(0); }
			pthread_detach(download_thread);
		} else if(command.find("login") == 0) {
			if(USER != "") {
				cout << "Already logged in! Logout first!\n";
				continue;
			}

			vector<string> ip_addresses;
			system("ifconfig > ip_address.txt");
			ifstream ifs("ip_address.txt");
			string line;
			while(!ifs.eof()) {
				getline(ifs, line);
				long long int index = line.find("inet ");
				if(index != string::npos){
					line = line.substr(index+5);
					line = line.substr(0, line.find(" "));
					ip_addresses.push_back(line);
				}
			}
			ifs.close();
			remove("ip_address.txt");

			if(ip_addresses.size() == 1) { perror("Error: Connect to Internet"); continue; }
			command.append(" ");
			command.append(ip_addresses[1]);
			command.append(" ");
			command.append(argv[1]);

			char* response = (char*) requestTracker(command);
			string resp (response);
			if(resp.find(" ") == string::npos) {
				USER = resp;
				cout << "Login successful\n";
			} else {
				cout << resp << "\n";
			}

		} else if(command.find("create_user") == 0) {
			char * response = (char *)requestTracker(command);
			cout << response << "\n";
		} else if(command.find("logout") == 0) {
			if(USER == "") {
				cout << "Already logged out!\n";
				continue;
			}

			command.append(" ");
			command.append(USER);
			char * response = (char *)requestTracker(command);
			cout << response << "\n";
			USER = "";
		} else if(command.find("create_group") == 0) {
			if(USER == "") {
				cout << "Please login to create group\n";
				continue;
			}

			command.append(" ");
			command.append(USER);
			char * response = (char *)requestTracker(command);
			cout << response << "\n";
		} else if(command.find("list_groups") == 0) {
			char * response = (char *)requestTracker(command);
			cout << response;
		} else if(command.find("upload_file") == 0) {
			if(USER == "") {
				cout << "Please login to upload file metadata information\n";
				continue;
			}
			command.append(" ");
			command.append(USER);

			vector<string> command_split;
			boost::split(command_split, command, boost::is_any_of(" "));

			FILE * fp = fopen(command_split[1].c_str(), "r");
			if(fp == NULL) { perror("Error: Invalid file"); continue; }
			fseek(fp, 0, SEEK_END);
			long long int file_size = ftell(fp);
			rewind(fp);
			command.append(" ");
			command.append(to_string(file_size));

			SHA_CTX ctx;
			SHA1_Init(&ctx);
			char buffer[CHUNK_SIZE];
			int number_of_characters;
			while(file_size > 0 && (number_of_characters = fread(buffer, sizeof(char), CHUNK_SIZE, fp)) > 0) {
				SHA1_Update(&ctx, buffer, number_of_characters);
				file_size -= number_of_characters;
			}
			unsigned char hash[SHA_DIGEST_LENGTH];
			unsigned char md_digest[2*SHA_DIGEST_LENGTH];
			SHA1_Final(hash, &ctx);
			for(int i=0, j=0; i<SHA_DIGEST_LENGTH; i++, j+=2) {
				sprintf((char *)&md_digest[j], "%02x", hash[i]);
			}
			command.append(" ");
			command.append((char *)md_digest);
			fclose(fp);

			char * response = (char *)requestTracker(command);
			cout << response << "\n";
		} else if(command.find("list_files") == 0) {
			char * response = (char *)requestTracker(command);
			cout << response;
		} else if(command.find("join_group") == 0) {
			if(USER == "") {
				cout << "Please login to join a group\n";
				continue;
			}
			command.append(" ");
			command.append(USER);

			char * response = (char *)requestTracker(command);
			cout << response << "\n";
		} else if(command.find("list_requests") == 0) {
			if(USER == "") {
				cout << "Please login to view the pending requests\n";
				continue;
			}
			command.append(" ");
			command.append(USER);

			char * response = (char *)requestTracker(command);
			cout << response;
		} else if(command.find("accept_request") == 0) {
			if(USER == "") {
				cout << "Please login to accept the pending requests\n";
				continue;
			}
			command.append(" ");
			command.append(USER);

			char * response = (char *)requestTracker(command);
			cout << response << "\n";
		} else {
			cout << "Invalid command\n";
		}
	}

	return 0;
}
