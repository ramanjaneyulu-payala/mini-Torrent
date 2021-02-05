#ifndef PEERTASK_H_
#define PEERTASK_H_

#include <bits/stdc++.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <boost/algorithm/string.hpp>
#include <openssl/sha.h>

#define CHUNK_SIZE 512*1024

using namespace std;

string TRACKER_IP = "";
string TRACKER_PORT = "";
string USER = "";

struct thread_arguments_structure {
	char sourceFilePath[256], destinationFilePath[256], ipAddress[15];
	int portNumber, dataTransferFD; long long int chunkNumber, totalChunks;
};



long long int minAmong(long long int x, long long int y) {
	return x<y?x:y;
}

void* requestTracker(string command){
	int socket_fd; struct sockaddr_in server_details;

	do{
		socket_fd = socket(AF_INET, SOCK_STREAM, 0);
		if (socket_fd == -1) perror("Error: Opening Socket");
	} while (socket_fd == -1);

	bzero((char *) &server_details, sizeof(server_details));
	server_details.sin_family = AF_INET;
	server_details.sin_port = htons(atoi(TRACKER_PORT.c_str()));
	server_details.sin_addr.s_addr = inet_addr(TRACKER_IP.c_str());

	if(connect(socket_fd, (struct sockaddr *)&server_details, sizeof(server_details)) == -1) { perror("Error: Connecting with tracker"); pthread_exit(NULL); }

	long long int command_size = command.size();
	if(send(socket_fd, &command_size, sizeof(long long int), 0) == -1) { perror("Error: Sending command size to tracker"); exit(0); }
	if(send(socket_fd, command.c_str(), command_size, 0) == -1) { perror("Error: Sending command to tracker"); exit(0); }

	long long int response_size;
	if(recv(socket_fd, &response_size, sizeof(long long int), 0) == -1) { perror("Error: Receiving response size from tracker"); pthread_exit(NULL); }

	char buffer[256]; bzero(buffer, 256); int number_of_characters;
	void * response = malloc(response_size+1);
	bzero(response, response_size+1);
	void * response_copy = response;
	while(response_size > 0 && (number_of_characters = recv(socket_fd, buffer, minAmong(256, response_size), 0)) > 0) {
		memcpy(response_copy, buffer, number_of_characters);
		response_copy += number_of_characters;
		response_size -= number_of_characters;
		bzero(buffer, 256);
	}

	close(socket_fd);
	return response;
}

void* receiveFileChunk(void* thread_arguments) {
	struct thread_arguments_structure * thr_arg = (struct thread_arguments_structure*)thread_arguments;
	int socket_fd, number_of_characters, port_number = thr_arg->portNumber; struct sockaddr_in server_details;
	string ip_address = thr_arg->ipAddress, source_file_path = thr_arg->sourceFilePath, destination_file_path = thr_arg->destinationFilePath;
	long long int source_file_path_size = source_file_path.size(), chunk_number = thr_arg->chunkNumber;

	do{
		socket_fd = socket(AF_INET, SOCK_STREAM, 0);
		if (socket_fd == -1) perror("Error: Opening Socket");
	} while (socket_fd == -1);

	bzero((char *) &server_details, sizeof(server_details));
	server_details.sin_family = AF_INET;
	server_details.sin_port = htons(port_number);
	server_details.sin_addr.s_addr = inet_addr(ip_address.c_str());

	//cout << "\n" << source_file_path << " " << chunk_number << "\n";

	cout << "Taking chunk " << chunk_number << " from " << ip_address << ":" << port_number << "\n";

	if(connect(socket_fd, (struct sockaddr *)&server_details, sizeof(server_details)) == -1) { perror("Error: Connecting with peer"); pthread_exit(NULL); }

	if(send(socket_fd, &source_file_path_size, sizeof(long long int), 0) == -1) { perror("Error: Sending source file path size"); pthread_exit(NULL); }
	if(send(socket_fd, source_file_path.c_str(), source_file_path_size, 0) == -1) { perror("Error: Sending source file path"); pthread_exit(NULL); }
	if(send(socket_fd, &chunk_number, sizeof(long long int), 0) == -1) { perror("Error: Sending chunk number"); pthread_exit(NULL); }

	long long int chunk_size;
	if(recv(socket_fd, &chunk_size, sizeof(long long int), 0) == -1) { perror("Error: Receiving chunk size"); pthread_exit(NULL); }
	//cout << chunk_size << " " << chunk_number << "\n";

	FILE * fp = fopen(destination_file_path.c_str(), "r+");
	fseek(fp, chunk_number*CHUNK_SIZE, SEEK_SET);

	SHA_CTX ctx;
	SHA1_Init(&ctx);

	char buffer[CHUNK_SIZE]; bzero(buffer, CHUNK_SIZE);
	while(chunk_size > 0 && (number_of_characters = recv(socket_fd, buffer, chunk_size, 0)) > 0) {
		fwrite(buffer, sizeof(char), number_of_characters, fp);
		SHA1_Update(&ctx, buffer, number_of_characters);
		chunk_size -= number_of_characters;
		bzero(buffer, CHUNK_SIZE);
	}

	unsigned char hash[SHA_DIGEST_LENGTH];
	unsigned char md_digest[2*SHA_DIGEST_LENGTH];
	SHA1_Final(hash, &ctx);
	for(int i=0, j=0; i<SHA_DIGEST_LENGTH; i++, j+=2) {
		sprintf((char *)&md_digest[j], "%02x", hash[i]);
	}
	string md_digest_string ((char *) md_digest);

	char received_md_digest[40];
	if(recv(socket_fd, received_md_digest, 40, 0) == -1) { perror("Error: Receiving md_digest data"); pthread_exit(NULL); }

	if(strncmp(md_digest_string.c_str(), received_md_digest, 40) == 0) {
		cout << "Chunk " << chunk_number << " received correctly\n";
	} else {
		cout << "Chunk " << chunk_number << " received incorrectly (sha1sum not matching)\n";
	}

	close(socket_fd);
	fclose(fp);

	pthread_exit(NULL);
}

void* requestFile(void* thread_arguments) {
	string thr_arg = *((string *)thread_arguments); int socket_fd; struct sockaddr_in server_details;

	vector<string> result;
	boost::split(result, thr_arg, boost::is_any_of(" "));
	if(result.size() != 5) { perror("Error: Invalid number of parameters"); pthread_exit(NULL); }

	string destination_file_path = result[3];
	do{
		socket_fd = socket(AF_INET, SOCK_STREAM, 0);
		if (socket_fd == -1) perror("Error: Opening Socket");
	} while (socket_fd == -1);

	bzero((char *) &server_details, sizeof(server_details));
	server_details.sin_family = AF_INET;
	server_details.sin_port = htons(stoi(TRACKER_PORT));
	server_details.sin_addr.s_addr = inet_addr(TRACKER_IP.c_str());

	if(connect(socket_fd, (struct sockaddr *)&server_details, sizeof(server_details)) == -1) { perror("Error: Connecting with tracker"); pthread_exit(NULL); }
	long long int file_size = stoll((char *)requestTracker(thr_arg));

	long long int total_chunks = (long long int)ceil((double)file_size/(CHUNK_SIZE));

	FILE *fp = fopen(destination_file_path.c_str(), "w");
	fseek(fp, file_size-1, SEEK_SET);
	fputc('\0', fp);
	fclose(fp);

	//cout << ip_address << " " << source_file_path << " " << destination_file_path << " " << source_file_path_size << " " << port_number << endl;

	pthread_t receive_threads[total_chunks]; string command, response; vector<string> ip_port_result;
	for(long long int i=0; i<total_chunks; i++) {
		command = "select_ipport " + to_string(i) + " " + result[2];
		response = (char *)requestTracker(command);
		boost::split(ip_port_result, response, boost::is_any_of(" "));
		if(ip_port_result.size() != 3) { perror("Error: Incorrect ip port details from tracker"); pthread_exit(NULL); }
		// Format "<ip addr> <port no> <source path>"

		pthread_t receive_chunk_thread;
		struct thread_arguments_structure * receive_file_thread_arg = (struct thread_arguments_structure*)malloc(sizeof(struct thread_arguments_structure));
		//bzero((char *) &receive_file_thread_arg, sizeof(receive_file_thread_arg));
		strcpy(receive_file_thread_arg->ipAddress, ip_port_result[0].c_str());
		receive_file_thread_arg->portNumber = stoi(ip_port_result[1]);
		strcpy(receive_file_thread_arg->sourceFilePath, ip_port_result[2].c_str());
		strcpy(receive_file_thread_arg->destinationFilePath, destination_file_path.c_str());
		receive_file_thread_arg->chunkNumber = i;

		//cout << "\nPKB: " << receive_file_thread_arg->chunkNumber << "\n";
		if(pthread_create(&receive_chunk_thread, NULL, receiveFileChunk, (void *)receive_file_thread_arg)) { perror("Error: Creating receiving thread"); pthread_exit(NULL); }
		receive_threads[i] = receive_chunk_thread;
	}

	for(long long int i = 0; i<total_chunks; i++) {
		pthread_join(receive_threads[i], NULL);
	}

	fp = fopen(destination_file_path.c_str(), "r");
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
	string md_digest_string ((char *) md_digest);
	if(md_digest_string == result[2])
		cout << "File " << destination_file_path << " has been downloaded successfully\n";
	else
		cout << "File " << destination_file_path << " has been downloaded incorrectly (sha1sum not matching)\n";

	pthread_exit(NULL);
}

void* sendFileChunk(void* thread_arguments) {
	struct thread_arguments_structure* thr_arg = (struct thread_arguments_structure*)thread_arguments;
	string file_path = thr_arg->sourceFilePath; int data_transfer_fd = thr_arg->dataTransferFD; long long int chunk_number = thr_arg->chunkNumber;

	//cout << "PKB: " << file_path << "\n";

	FILE* fp = fopen(file_path.c_str(), "r");
	if(fp == NULL) { perror("Error: Unable to open file"); pthread_exit(NULL); }

	char buffer[CHUNK_SIZE];
	bzero(buffer, CHUNK_SIZE);
	fseek(fp, 0, SEEK_END);

	long long int chunk_size = minAmong(ftell(fp) - chunk_number*CHUNK_SIZE, CHUNK_SIZE);
	//rewind(fp);
	fseek(fp, chunk_number*CHUNK_SIZE, SEEK_SET);

	if(send(data_transfer_fd, &chunk_size, sizeof(long long int), 0) == -1) { perror("Error: Sending chunk size"); pthread_exit(NULL); }

	SHA_CTX ctx;
	SHA1_Init(&ctx);

	long long int number_of_characters;
	while(chunk_size > 0 && (number_of_characters = fread(buffer, sizeof(char), chunk_size, fp)) > 0) {
		if(send(data_transfer_fd, buffer, number_of_characters, 0) == -1) { perror("Error: Sending chunk data"); pthread_exit(NULL); }
		SHA1_Update(&ctx, buffer, number_of_characters);
		bzero(buffer, CHUNK_SIZE);
		chunk_size -= number_of_characters;
	}

	unsigned char hash[SHA_DIGEST_LENGTH];
	unsigned char md_digest[2*SHA_DIGEST_LENGTH];
	SHA1_Final(hash, &ctx);
	for(int i=0, j=0; i<SHA_DIGEST_LENGTH; i++, j+=2) {
		sprintf((char *)&md_digest[j], "%02x", hash[i]);
	}
	string md_digest_string ((char *) md_digest);

	if(send(data_transfer_fd, md_digest_string.c_str(), 2*SHA_DIGEST_LENGTH, 0) == -1) { perror("Error: Sending md_digest data"); pthread_exit(NULL); }

	fclose(fp);
	close(data_transfer_fd);

	pthread_exit(NULL);
}

void* startListeningPort(void* thread_arguments) {
	struct thread_arguments_structure* thr_arg = (struct thread_arguments_structure*)thread_arguments;
	int socket_fd, data_transfer_fd, port_number = thr_arg->portNumber, number_of_characters;
	unsigned int sockaddr_struct_length;

	do{
		socket_fd = socket(AF_INET, SOCK_STREAM, 0);
		if (socket_fd == -1) perror("Error: Opening Socket");
	} while (socket_fd == -1);

	struct sockaddr_in server_details, client_details;
	bzero((char *) &server_details, sizeof(server_details));
	server_details.sin_family = AF_INET;
	server_details.sin_addr.s_addr = INADDR_ANY;
	server_details.sin_port = htons(port_number);

	if (bind(socket_fd, (struct sockaddr *) &server_details, sizeof(server_details)) == -1) { perror("Error: Binding"); pthread_exit(NULL); }

	listen(socket_fd, INT_MAX);
	sockaddr_struct_length = sizeof(struct sockaddr_in);

	// Here file_name denotes the entire path of the file and data_size denotes the number of characters in file_name
	string file_name; long long int data_size, chunk_number; char buffer[256]; bzero(buffer, 256);

	while (1) {
		data_transfer_fd = accept(socket_fd, (struct sockaddr *) &client_details, &sockaddr_struct_length);
		if(data_transfer_fd == -1) { perror("Error: Accepting"); pthread_exit(NULL); }

		if(recv(data_transfer_fd, &data_size, sizeof(long long int), 0) == -1) { perror("Error: Receiving file path size"); pthread_exit(NULL); }
		//cout << data_size << "\n";

		file_name = "";
		while(data_size > 0 && (number_of_characters = recv(data_transfer_fd, buffer, minAmong(data_size, 256), 0)) > 0) {
			file_name.append(buffer);
			data_size -= number_of_characters;
			bzero(buffer, 256);
		}

		//cout << file_name << "\n";
		if(number_of_characters == -1) { perror("Error: Receiving file path"); pthread_exit(NULL); }

		if(recv(data_transfer_fd, &chunk_number, sizeof(long long int), 0) == -1) { perror("Error: Receiving chunk number"); pthread_exit(NULL); }
		//cout << chunk_number << "\n";

		cout << "Sending " << chunk_number << " chunk of file " << file_name << "\n";

		pthread_t send_chunk_thread;
		struct thread_arguments_structure * send_chunk_thread_arg = (struct thread_arguments_structure *)malloc(sizeof(struct thread_arguments_structure));
		strcpy(send_chunk_thread_arg->sourceFilePath, file_name.c_str());
		send_chunk_thread_arg->dataTransferFD = data_transfer_fd;
		send_chunk_thread_arg->chunkNumber = chunk_number;
		if(pthread_create(&send_chunk_thread, NULL, sendFileChunk, (void *) send_chunk_thread_arg)) { perror("Error: Creating send chunk thread"); pthread_exit(NULL); }
		pthread_detach(send_chunk_thread);
	}

	pthread_exit(NULL);
}

#endif /* PEERTASK_H_ */
