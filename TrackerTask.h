#ifndef TRACKERTASK_H_
#define TRACKERTASK_H_

#include <bits/stdc++.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <boost/algorithm/string.hpp>

#define CHUNK_SIZE 512*1024

using namespace std;

pthread_mutex_t USERS_INFO_MUTEX = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t GROUPS_INFO_MUTEX = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t FILES_INFO_MUTEX = PTHREAD_MUTEX_INITIALIZER;

long long int minAmong(long long int x, long long int y) {
	return x<y?x:y;
}

struct thread_arguments_structure {
	int dataTransferFD;
	char command[256];
};

class UserInfo {
public:
	string ipAddress, password; int portNumber; unordered_set<string> groupsOwned;
	UserInfo(string ipAddress, int portNumber, string password) {
		this->ipAddress = ipAddress;
		this->portNumber = portNumber;
		this->password = password;
	}
};

class GroupInfo {
public:
	unordered_set<string> peers, fileSHAs; string owner; vector<string> pendingUsers;
	GroupInfo(string owner) {
		this->owner = owner;
	}
};

class FileLocation {
public:
	string userId, location; bool* chunksPresent;
	FileLocation(string userId, string location, long long int totalChunks) {
		this->userId = userId;
		this->location = location;
		this->chunksPresent = new bool[totalChunks];
		for(long long int i = 0; i<totalChunks; i++) {
			this->chunksPresent[i] = false;
		}
	}
};

class FileInfo {
public:
	vector<FileLocation> locations; long long int fileSize;
	FileInfo(long long int fileSize) {
		this->fileSize = fileSize;
	}
};

unordered_map<string, UserInfo*> USERS_INFO;
unordered_map<string, GroupInfo*> GROUPS_INFO;
unordered_map<string, FileInfo*> FILES_INFO;
unsigned long long int PIECE_SELECTION = 0;

void sendResponse(string response, int dataTransferFD){
	long long int response_size = response.size();
	if(send(dataTransferFD, &response_size, sizeof(long long int), 0) == -1) { perror("Error: Sending response size"); }

	char * respone_pointer = (char *) response.c_str();
	long long int number_of_characters;
	while(response_size > 0) {
		number_of_characters = send(dataTransferFD, respone_pointer, response_size, 0);
		if(number_of_characters == -1) { perror("Error: Sending response"); pthread_exit(NULL); }
		response_size -= number_of_characters;
		respone_pointer += number_of_characters;
	}
}

void* createUser(void* thread_arguments) {
	struct thread_arguments_structure * thr_arg = (struct thread_arguments_structure *) thread_arguments;
	int data_transfer_fd = thr_arg->dataTransferFD; string command = thr_arg->command;
	int lock_status = -1, unlock_status = -1; string response = "";

	vector<string> result;
	boost::split(result, command, boost::is_any_of(" "));
	if(result.size() != 3) { response = "Error: Invalid number of parameters"; goto end_create_user; }

	lock_status = pthread_mutex_lock(&USERS_INFO_MUTEX);
	if (lock_status) { response = "Error: Locking USERS_INFO_MUTEX"; goto end_create_user; }

	if(USERS_INFO.count(result[1]) > 0) { response = "Error: User already exists"; }
	else { USERS_INFO[result[1]] = new UserInfo("", -1, result[2]); response = "User created successfully! Please login to continue!"; }

	unlock_status = pthread_mutex_unlock(&USERS_INFO_MUTEX);
	if (unlock_status) { response = "Error: Unlocking USERS_INFO_MUTEX"; }

	end_create_user:

	sendResponse(response, data_transfer_fd);

	close(data_transfer_fd);
	pthread_exit(NULL);
}

void* login(void* thread_arguments) {
	struct thread_arguments_structure * thr_arg = (struct thread_arguments_structure *) thread_arguments;
	int data_transfer_fd = thr_arg->dataTransferFD; string command = thr_arg->command;
	int lock_status = -1, unlock_status = -1; string response = "";

	vector<string> result;
	boost::split(result, command, boost::is_any_of(" "));
	if(result.size() != 5) { response = "Error: Invalid number of parameters"; goto end_login; }

	lock_status = pthread_mutex_lock(&USERS_INFO_MUTEX);
	if (lock_status) { response = "Error: Locking USERS_INFO_MUTEX"; goto end_login; }

	if(USERS_INFO.count(result[1]) == 0) { response = "Error: Unknown user"; }
	else if(USERS_INFO[result[1]]->password == result[2]) {
		USERS_INFO[result[1]]->ipAddress = result[3];
		USERS_INFO[result[1]]->portNumber = atoi(result[4].c_str());
		response = result[1];
	}
	else { response = "Error: Wrong password"; }

	unlock_status = pthread_mutex_unlock(&USERS_INFO_MUTEX);
	if (unlock_status) { response = "Error: Unlocking USERS_INFO_MUTEX"; }

	end_login:

	sendResponse(response, data_transfer_fd);

	close(data_transfer_fd);
	pthread_exit(NULL);
}

void* logout(void* thread_arguments) {
	struct thread_arguments_structure * thr_arg = (struct thread_arguments_structure *) thread_arguments;
	int data_transfer_fd = thr_arg->dataTransferFD; string command = thr_arg->command;
	int lock_status = -1, unlock_status = -1; string response = "";

	vector<string> result;
	boost::split(result, command, boost::is_any_of(" "));
	if(result.size() != 2) { response = "Error: Invalid number of parameters"; goto end_logout; }

	lock_status = pthread_mutex_lock(&USERS_INFO_MUTEX);
	if (lock_status) { response = "Error: Locking USERS_INFO_MUTEX"; goto end_logout; }

	if(USERS_INFO.count(result[1]) == 0) { response = "Error: User unknown"; }
	else { USERS_INFO[result[1]]->ipAddress = ""; USERS_INFO[result[1]]->portNumber = -1; response = "Logged out successfully!"; }

	unlock_status = pthread_mutex_unlock(&USERS_INFO_MUTEX);
	if (unlock_status) { response = "Error: Unlocking USERS_INFO_MUTEX"; }

	end_logout:

	sendResponse(response, data_transfer_fd);

	close(data_transfer_fd);
	pthread_exit(NULL);
}

void* createGroup(void* thread_arguments) {
	struct thread_arguments_structure * thr_arg = (struct thread_arguments_structure *) thread_arguments;
	int data_transfer_fd = thr_arg->dataTransferFD; string command = thr_arg->command;
	int lock_status = -1, unlock_status = -1; string response = "";

	vector<string> result;
	boost::split(result, command, boost::is_any_of(" "));
	if(result.size() != 3) { response = "Error: Invalid number of parameters"; goto end_create_group; }

	lock_status = pthread_mutex_lock(&USERS_INFO_MUTEX);
	if (lock_status) { response = "Error: Locking USERS_INFO_MUTEX"; goto end_create_group; }

	lock_status = pthread_mutex_lock(&GROUPS_INFO_MUTEX);
	if (lock_status) { response = "Error: Locking GROUPS_INFO_MUTEX"; goto end_create_group; }

	if(GROUPS_INFO.count(result[1]) == 0) {
		GROUPS_INFO[result[1]] = new GroupInfo(result[2]);
		GROUPS_INFO[result[1]]->peers.insert(result[2]);
		USERS_INFO[result[2]]->groupsOwned.insert(result[1]);
		response = "Group created successfully!";
	} else {
		response = "Error: Group id already exists";
	}

	unlock_status = pthread_mutex_unlock(&USERS_INFO_MUTEX);
	if (unlock_status) { response = "Error: Unlocking USERS_INFO_MUTEX"; }

	unlock_status = pthread_mutex_unlock(&GROUPS_INFO_MUTEX);
	if (unlock_status) { response = "Error: Unlocking GROUPS_INFO_MUTEX"; }

	end_create_group:

	sendResponse(response, data_transfer_fd);

	close(data_transfer_fd);
	pthread_exit(NULL);
}

void* listGroups(void* thread_arguments) {
	struct thread_arguments_structure * thr_arg = (struct thread_arguments_structure *) thread_arguments;
	int data_transfer_fd = thr_arg->dataTransferFD; string command = thr_arg->command;
	int lock_status = -1, unlock_status = -1; string response = "";

	vector<string> result;
	boost::split(result, command, boost::is_any_of(" "));
	if(result.size() != 1) { response = "Error: Invalid number of parameters\n"; goto end_list_group; }

	lock_status = pthread_mutex_lock(&GROUPS_INFO_MUTEX);
	if (lock_status) { response = "Error: Locking GROUPS_INFO_MUTEX\n"; goto end_list_group; }

	if(GROUPS_INFO.empty()) {
		response = "No groups are present currently\n";
	} else {
		for(auto itr=GROUPS_INFO.begin(); itr!=GROUPS_INFO.end(); itr++) {
			response += itr->first + "\n";
		}
	}

	unlock_status = pthread_mutex_unlock(&GROUPS_INFO_MUTEX);
	if (unlock_status) { response = "Error: Unlocking GROUPS_INFO_MUTEX\n"; }

	end_list_group:

	sendResponse(response, data_transfer_fd);

	close(data_transfer_fd);
	pthread_exit(NULL);
}

void* uploadFile(void* thread_arguments) {
	struct thread_arguments_structure * thr_arg = (struct thread_arguments_structure *) thread_arguments;
	int data_transfer_fd = thr_arg->dataTransferFD; string command = thr_arg->command;
	int lock_status = -1, unlock_status = -1; string response = ""; long long int total_chunks;

	vector<string> result;
	boost::split(result, command, boost::is_any_of(" "));
	if(result.size() != 6) { response = "Error: Invalid number of parameters"; goto end_upload_file; }

	lock_status = pthread_mutex_lock(&GROUPS_INFO_MUTEX);
	if (lock_status) { response = "Error: Locking GROUPS_INFO_MUTEX"; goto end_upload_file; }

	lock_status = pthread_mutex_lock(&FILES_INFO_MUTEX);
	if (lock_status) { response = "Error: Locking FILES_INFO_MUTEX"; goto end_upload_file; }

	if (GROUPS_INFO.count(result[2]) == 0) {
		response = "Error: Group does not exist";
	} else if (GROUPS_INFO[result[2]]->peers.find(result[3]) == GROUPS_INFO[result[2]]->peers.end()) {
		response = "Error: You are not a part of this group. Please get yourself added!";
	} else if (FILES_INFO.count(result[5]) == 0) {
		FILES_INFO[result[5]] = new FileInfo(stoll(result[4]));
		total_chunks = (long long int) ceil(((double)stoll(result[4]))/(CHUNK_SIZE));
		FileLocation fl (result[3], result[1], total_chunks);
		for(long long int i=0; i<total_chunks; i++) {
			fl.chunksPresent[i] = true;
		}
		FILES_INFO[result[5]]->locations.push_back(fl);
		GROUPS_INFO[result[2]]->fileSHAs.insert(result[5]);
		response = "File added successfully to the tracker!";
	} else if (FILES_INFO.count(result[5]) == 1) {
		total_chunks = (long long int) ceil(((double)stoll(result[4]))/(CHUNK_SIZE));
		FileLocation fl (result[3], result[1], total_chunks);
		for(long long int i=0; i<total_chunks; i++) {
			fl.chunksPresent[i] = true;
		}
		FILES_INFO[result[5]]->locations.push_back(fl);
		if(GROUPS_INFO[result[2]]->fileSHAs.find(result[5]) == GROUPS_INFO[result[2]]->fileSHAs.end()) {
			GROUPS_INFO[result[2]]->fileSHAs.insert(result[5]);
			response = "File already exists in the tracker! Adding file to the group! Adding you to the seeders list!";
		}
		else {
			response = "File already exists in the tracker and the group! Adding you to the seeders list!";
		}
	}

	unlock_status = pthread_mutex_unlock(&GROUPS_INFO_MUTEX);
	if (unlock_status) { response = "Error: Unlocking GROUPS_INFO_MUTEX"; }

	unlock_status = pthread_mutex_unlock(&FILES_INFO_MUTEX);
	if (unlock_status) { response = "Error: Unlocking FILES_INFO_MUTEX"; }

	end_upload_file:

	sendResponse(response, data_transfer_fd);

	close(data_transfer_fd);
	pthread_exit(NULL);
}

void* listFiles(void* thread_arguments) {
	struct thread_arguments_structure * thr_arg = (struct thread_arguments_structure *) thread_arguments;
	int data_transfer_fd = thr_arg->dataTransferFD; string command = thr_arg->command;
	int lock_status = -1, unlock_status = -1; string response = "";

	vector<string> result;
	boost::split(result, command, boost::is_any_of(" "));
	if(result.size() != 2) { response = "Error: Invalid number of parameters\n"; goto end_list_files; }

	lock_status = pthread_mutex_lock(&GROUPS_INFO_MUTEX);
	if (lock_status) { response = "Error: Locking GROUPS_INFO_MUTEX\n"; goto end_list_files; }

	lock_status = pthread_mutex_lock(&FILES_INFO_MUTEX);
	if (lock_status) { response = "Error: Locking FILES_INFO_MUTEX\n"; goto end_list_files; }

	if (GROUPS_INFO.count(result[1]) == 0) {
		response = "Error: Group does not exist\n";
	} else if (GROUPS_INFO[result[1]]->fileSHAs.size() == 0) {
		response = "No files in this group\n";
	} else {
		for(auto itr=GROUPS_INFO[result[1]]->fileSHAs.begin(); itr!=GROUPS_INFO[result[1]]->fileSHAs.end(); itr++) {
			response.append(*itr);
			response.append(" ");
			response.append(to_string(FILES_INFO[*itr]->fileSize));
			response.append(" ");
			response.append(FILES_INFO[*itr]->locations[0].location);
			response.append("\n");
		}
	}

	unlock_status = pthread_mutex_unlock(&GROUPS_INFO_MUTEX);
	if (unlock_status) { response = "Error: Unlocking GROUPS_INFO_MUTEX\n"; }

	unlock_status = pthread_mutex_unlock(&FILES_INFO_MUTEX);
	if (unlock_status) { response = "Error: Unlocking FILES_INFO_MUTEX\n"; }

	end_list_files:

	sendResponse(response, data_transfer_fd);

	close(data_transfer_fd);
	pthread_exit(NULL);
}

void* joinGroup(void* thread_arguments) {
	struct thread_arguments_structure * thr_arg = (struct thread_arguments_structure *) thread_arguments;
	int data_transfer_fd = thr_arg->dataTransferFD; string command = thr_arg->command;
	int lock_status = -1, unlock_status = -1; string response = "";

	vector<string> result;
	boost::split(result, command, boost::is_any_of(" "));
	if(result.size() != 3) { response = "Error: Invalid number of parameters"; goto end_join_group; }

	lock_status = pthread_mutex_lock(&GROUPS_INFO_MUTEX);
	if (lock_status) { response = "Error: Locking GROUPS_INFO_MUTEX"; goto end_join_group; }

	if(GROUPS_INFO.count(result[1]) == 0) {
		response = "Error: Group does not exist";
	} else if(GROUPS_INFO[result[1]]->peers.find(result[2]) != GROUPS_INFO[result[1]]->peers.end()) {
		response = "Error: You are already a part of this group";
	} else {
		GROUPS_INFO[result[1]]->pendingUsers.push_back(result[2]);
		response = "Your request will be processed by the admin";
	}

	unlock_status = pthread_mutex_unlock(&GROUPS_INFO_MUTEX);
	if (unlock_status) { response = "Error: Unlocking GROUPS_INFO_MUTEX"; }

	end_join_group:

	sendResponse(response, data_transfer_fd);

	close(data_transfer_fd);
	pthread_exit(NULL);
}

void* listRequests(void* thread_arguments) {
	struct thread_arguments_structure * thr_arg = (struct thread_arguments_structure *) thread_arguments;
	int data_transfer_fd = thr_arg->dataTransferFD; string command = thr_arg->command;
	int lock_status = -1, unlock_status = -1; string response = "";

	vector<string> result;
	boost::split(result, command, boost::is_any_of(" "));
	if(result.size() != 3) { response = "Error: Invalid number of parameters\n"; goto end_list_requests; }

	lock_status = pthread_mutex_lock(&GROUPS_INFO_MUTEX);
	if (lock_status) { response = "Error: Locking GROUPS_INFO_MUTEX\n"; goto end_list_requests; }

	if(GROUPS_INFO.count(result[1]) == 0) {
		response = "Error: Group does not exist\n";
	} else if(GROUPS_INFO[result[1]]->owner != result[2]) {
		response = "Error: Only the group admin can view the pending requests\n";
	} else if(GROUPS_INFO[result[1]]->pendingUsers.size() == 0) {
		response = "No pending users\n";
	} else {
		for(long long int i=0; i<GROUPS_INFO[result[1]]->pendingUsers.size(); i++) {
			response += GROUPS_INFO[result[1]]->pendingUsers[i] + "\n";
		}
	}

	unlock_status = pthread_mutex_unlock(&GROUPS_INFO_MUTEX);
	if (unlock_status) { response = "Error: Unlocking GROUPS_INFO_MUTEX\n"; }

	end_list_requests:

	sendResponse(response, data_transfer_fd);

	close(data_transfer_fd);
	pthread_exit(NULL);
}

void* acceptRequest(void* thread_arguments) {
	struct thread_arguments_structure * thr_arg = (struct thread_arguments_structure *) thread_arguments;
	int data_transfer_fd = thr_arg->dataTransferFD; string command = thr_arg->command;
	int lock_status = -1, unlock_status = -1; string response = "";

	vector<string> result;
	boost::split(result, command, boost::is_any_of(" "));
	if(result.size() != 4) { response = "Error: Invalid number of parameters"; goto end_accept_request; }

	lock_status = pthread_mutex_lock(&USERS_INFO_MUTEX);
	if (lock_status) { response = "Error: Locking USERS_INFO_MUTEX"; goto end_accept_request; }

	lock_status = pthread_mutex_lock(&GROUPS_INFO_MUTEX);
	if (lock_status) { response = "Error: Locking GROUPS_INFO_MUTEX"; goto end_accept_request; }

	if(GROUPS_INFO.count(result[1]) == 0) {
		response = "Error: Group does not exist";
	} else if(USERS_INFO.count(result[2]) == 0) {
		response = "Error: User (to be approved) does not exist";
	} else if(GROUPS_INFO[result[1]]->owner != result[3]) {
		response = "Error: Only admin can approve requests for this group";
	} else {
		auto itr = find(GROUPS_INFO[result[1]]->pendingUsers.begin(), GROUPS_INFO[result[1]]->pendingUsers.end(), result[2]);
		if(itr == GROUPS_INFO[result[1]]->pendingUsers.end()) {
			response = "Error: User not present in the pending requests list";
		} else {
			GROUPS_INFO[result[1]]->peers.insert(*itr);
			GROUPS_INFO[result[1]]->pendingUsers.erase(itr);
			response = "Added to the group successfully!";
		}
	}

	unlock_status = pthread_mutex_unlock(&USERS_INFO_MUTEX);
	if (unlock_status) { response = "Error: Unlocking USERS_INFO_MUTEX"; }

	unlock_status = pthread_mutex_unlock(&GROUPS_INFO_MUTEX);
	if (unlock_status) { response = "Error: Unlocking GROUPS_INFO_MUTEX"; }

	end_accept_request:

	sendResponse(response, data_transfer_fd);

	close(data_transfer_fd);
	pthread_exit(NULL);
}

void* updateChunk(void* thread_arguments) {
	struct thread_arguments_structure * thr_arg = (struct thread_arguments_structure *) thread_arguments;
	int data_transfer_fd = thr_arg->dataTransferFD; string command = thr_arg->command;
	int lock_status = -1, unlock_status = -1; string response = ""; long long int seeders_size;

	vector<string> result;
	boost::split(result, command, boost::is_any_of(" "));
	if(result.size() != 4) { response = "Error: Invalid number of parameters"; goto end_update_chunk; }
	// Format "update_chunk <chunk_number> <user_id> <hash_code_file>"

	lock_status = pthread_mutex_lock(&FILES_INFO_MUTEX);
	if (lock_status) { response = "Error: Locking FILES_INFO_MUTEX"; goto end_update_chunk; }

	seeders_size = FILES_INFO[result[3]]->locations.size();
	for(long long int i = 0; i<seeders_size; i++) {
		if(FILES_INFO[result[3]]->locations[i].userId == result[2]) {
			FILES_INFO[result[3]]->locations[i].chunksPresent[stoll(result[1])] = true;
		}
	}
	response = "Chunk information updated successfully!";

	unlock_status = pthread_mutex_unlock(&FILES_INFO_MUTEX);
	if (unlock_status) { response = "Error: Unlocking FILES_INFO_MUTEX"; }

	end_update_chunk:

	sendResponse(response, data_transfer_fd);

	close(data_transfer_fd);
	pthread_exit(NULL);
}

void* selectIpPort(void* thread_arguments) {
	struct thread_arguments_structure * thr_arg = (struct thread_arguments_structure *) thread_arguments;
	int data_transfer_fd = thr_arg->dataTransferFD; string command = thr_arg->command;
	int lock_status = -1, unlock_status = -1; string response = ""; long long int random_selection = PIECE_SELECTION++, seeders_count, seeders_checked;

	vector<string> result;
	boost::split(result, command, boost::is_any_of(" "));
	if(result.size() != 3) { response = "Error: Invalid number of parameters"; goto end_select_ipport; }
	// Format "select_ipport <chunk_number> <hash_code>"

	lock_status = pthread_mutex_lock(&USERS_INFO_MUTEX);
	if (lock_status) { response = "Error: Locking USERS_INFO_MUTEX"; goto end_select_ipport; }

	lock_status = pthread_mutex_lock(&FILES_INFO_MUTEX);
	if (lock_status) { response = "Error: Locking FILES_INFO_MUTEX"; goto end_select_ipport; }

	seeders_count = FILES_INFO[result[2]]->locations.size(); seeders_checked = 0;
	while(FILES_INFO[result[2]]->locations[random_selection%seeders_count].chunksPresent[stoll(result[1])] == false || USERS_INFO[FILES_INFO[result[2]]->locations[random_selection%seeders_count].userId]->portNumber == -1) {
		random_selection++;
		seeders_checked++;
		if(seeders_checked == seeders_count) {
			break;
		}
	}

	if(seeders_checked == seeders_count) {
		response = "Error: No seeders are currently present for this chunk";
	} else {
		string user_id = FILES_INFO[result[2]]->locations[random_selection%seeders_count].userId;
		response = USERS_INFO[user_id]->ipAddress + " " + to_string(USERS_INFO[user_id]->portNumber) + " " + FILES_INFO[result[2]]->locations[random_selection%seeders_count].location;
	}

	unlock_status = pthread_mutex_unlock(&USERS_INFO_MUTEX);
	if (unlock_status) { response = "Error: Unlocking USERS_INFO_MUTEX"; }

	unlock_status = pthread_mutex_unlock(&FILES_INFO_MUTEX);
	if (unlock_status) { response = "Error: Unlocking FILES_INFO_MUTEX"; }

	end_select_ipport:

	sendResponse(response, data_transfer_fd);

	close(data_transfer_fd);
	pthread_exit(NULL);
}

void* initDownloadFile(void* thread_arguments) {
	struct thread_arguments_structure * thr_arg = (struct thread_arguments_structure *) thread_arguments;
	int data_transfer_fd = thr_arg->dataTransferFD; string command = thr_arg->command;
	int lock_status = -1, unlock_status = -1; string response = "";

	vector<string> result;
	boost::split(result, command, boost::is_any_of(" "));
	if(result.size() != 5) { response = "Error: Invalid number of parameters"; goto end_init_download_file; }
	// Format "download_file <group id> <hash code> <destination path> <user id>"

	lock_status = pthread_mutex_lock(&GROUPS_INFO_MUTEX);
	if (lock_status) { response = "Error: Locking GROUPS_INFO_MUTEX"; goto end_init_download_file; }

	if(GROUPS_INFO.count(result[1]) == 0) {
		response = "Error: Group does not exist";
	} else if(GROUPS_INFO[result[1]]->fileSHAs.find(result[2]) == GROUPS_INFO[result[1]]->fileSHAs.end()) {
		response = "Error: File does not exist in the group";
	} else if(GROUPS_INFO[result[1]]->peers.find(result[4]) == GROUPS_INFO[result[1]]->peers.end()) {
		response = "Error: You are not a part of this group";
	} else {
		FileLocation fl (result[4], result[3], (long long int)ceil(((double)FILES_INFO[result[2]]->fileSize)/(CHUNK_SIZE)));
		FILES_INFO[result[2]]->locations.push_back(fl);
		response = to_string(FILES_INFO[result[2]]->fileSize);
	}

	unlock_status = pthread_mutex_unlock(&GROUPS_INFO_MUTEX);
	if (unlock_status) { response = "Error: Unlocking GROUPS_INFO_MUTEX"; }

	end_init_download_file:

	sendResponse(response, data_transfer_fd);

	close(data_transfer_fd);
	pthread_exit(NULL);
}

void* interpretCommand(void* thread_arguments) {
	struct thread_arguments_structure * thr_arg = (struct thread_arguments_structure *) thread_arguments;
	int data_transfer_fd = thr_arg->dataTransferFD, number_of_characters;

	string command; long long int command_size; char buffer[256]; bzero(buffer, 256);

	if(recv(data_transfer_fd, &command_size, sizeof(long long int), 0) == -1) { perror("Error: Receiving command size"); pthread_exit(NULL); }

	command = "";
	while(command_size > 0 && (number_of_characters = recv(data_transfer_fd, buffer, minAmong(command_size, 256), 0)) > 0) {
		command.append(buffer);
		command_size -= number_of_characters;
		bzero(buffer, 256);
	}

	if(command.find("create_user") == 0) {
		pthread_t create_user_thread;
		struct thread_arguments_structure * create_user_thread_arg = (struct thread_arguments_structure *)malloc(sizeof(struct thread_arguments_structure));
		create_user_thread_arg->dataTransferFD = data_transfer_fd;
		strcpy(create_user_thread_arg->command, command.c_str());

		if(pthread_create(&create_user_thread, NULL, createUser, (void *)create_user_thread_arg)) { perror("Error: Creating create user thread"); exit(0); }
		pthread_detach(create_user_thread);
	} else if(command.find("login") == 0) {
		pthread_t login_thread;
		struct thread_arguments_structure * login_thread_arg = (struct thread_arguments_structure *)malloc(sizeof(struct thread_arguments_structure));
		login_thread_arg->dataTransferFD = data_transfer_fd;
		strcpy(login_thread_arg->command, command.c_str());

		if(pthread_create(&login_thread, NULL, login, (void*)login_thread_arg)) { perror("Error: Creating login thread"); exit(0); }
		pthread_detach(login_thread);
	} else if(command.find("logout") == 0) {
		pthread_t logout_thread;
		struct thread_arguments_structure * logout_thread_arg = (struct thread_arguments_structure *)malloc(sizeof(struct thread_arguments_structure));
		logout_thread_arg->dataTransferFD = data_transfer_fd;
		strcpy(logout_thread_arg->command, command.c_str());

		if(pthread_create(&logout_thread, NULL, logout, (void*)logout_thread_arg)) { perror("Error: Creating logout thread"); exit(0); }
		pthread_detach(logout_thread);
	} else if(command.find("create_group") == 0) {
		pthread_t create_group_thread;
		struct thread_arguments_structure * create_group_thread_arg = (struct thread_arguments_structure *)malloc(sizeof(struct thread_arguments_structure));
		create_group_thread_arg->dataTransferFD = data_transfer_fd;
		strcpy(create_group_thread_arg->command, command.c_str());

		if(pthread_create(&create_group_thread, NULL, createGroup, (void*)create_group_thread_arg)) { perror("Error: Creating create group thread"); exit(0); }
		pthread_detach(create_group_thread);
	} else if(command.find("list_groups") == 0) {
		pthread_t list_group_thread;
		struct thread_arguments_structure * list_group_thread_arg = (struct thread_arguments_structure *)malloc(sizeof(struct thread_arguments_structure));
		list_group_thread_arg->dataTransferFD = data_transfer_fd;
		strcpy(list_group_thread_arg->command, command.c_str());

		if(pthread_create(&list_group_thread, NULL, listGroups, (void*)list_group_thread_arg)) { perror("Error: Creating list groups thread"); exit(0); }
		pthread_detach(list_group_thread);
	} else if(command.find("upload_file") == 0) {
		pthread_t upload_file_thread;
		struct thread_arguments_structure * upload_file_thread_arg = (struct thread_arguments_structure *)malloc(sizeof(struct thread_arguments_structure));
		upload_file_thread_arg->dataTransferFD = data_transfer_fd;
		strcpy(upload_file_thread_arg->command, command.c_str());

		if(pthread_create(&upload_file_thread, NULL, uploadFile, (void*)upload_file_thread_arg)) { perror("Error: Creating upload file thread"); exit(0); }
		pthread_detach(upload_file_thread);
	} else if(command.find("list_files") == 0) {
		pthread_t list_files_thread;
		struct thread_arguments_structure * list_files_thread_arg = (struct thread_arguments_structure *)malloc(sizeof(struct thread_arguments_structure));
		list_files_thread_arg->dataTransferFD = data_transfer_fd;
		strcpy(list_files_thread_arg->command, command.c_str());

		if(pthread_create(&list_files_thread, NULL, listFiles, (void*)list_files_thread_arg)) { perror("Error: Creating list files thread"); exit(0); }
		pthread_detach(list_files_thread);
	} else if(command.find("join_group") == 0) {
		pthread_t join_group_thread;
		struct thread_arguments_structure * join_group_thread_arg = (struct thread_arguments_structure *)malloc(sizeof(struct thread_arguments_structure));
		join_group_thread_arg->dataTransferFD = data_transfer_fd;
		strcpy(join_group_thread_arg->command, command.c_str());

		if(pthread_create(&join_group_thread, NULL, joinGroup, (void*)join_group_thread_arg)) { perror("Error: Creating join group thread"); exit(0); }
		pthread_detach(join_group_thread);
	} else if(command.find("list_requests") == 0) {
		pthread_t list_requests_thread;
		struct thread_arguments_structure * list_requests_thread_arg = (struct thread_arguments_structure *)malloc(sizeof(struct thread_arguments_structure));
		list_requests_thread_arg->dataTransferFD = data_transfer_fd;
		strcpy(list_requests_thread_arg->command, command.c_str());

		if(pthread_create(&list_requests_thread, NULL, listRequests, (void*)list_requests_thread_arg)) { perror("Error: Creating list requests thread"); exit(0); }
		pthread_detach(list_requests_thread);
	} else if(command.find("accept_request") == 0) {
		pthread_t accept_request_thread;
		struct thread_arguments_structure * accept_request_thread_arg = (struct thread_arguments_structure *)malloc(sizeof(struct thread_arguments_structure));
		accept_request_thread_arg->dataTransferFD = data_transfer_fd;
		strcpy(accept_request_thread_arg->command, command.c_str());

		if(pthread_create(&accept_request_thread, NULL, acceptRequest, (void*)accept_request_thread_arg)) { perror("Error: Creating accept request thread"); exit(0); }
		pthread_detach(accept_request_thread);
	} else if(command.find("update_chunk") == 0) {
		pthread_t update_chunk_thread;
		struct thread_arguments_structure * update_chunk_thread_arg = (struct thread_arguments_structure *)malloc(sizeof(struct thread_arguments_structure));
		update_chunk_thread_arg->dataTransferFD = data_transfer_fd;
		strcpy(update_chunk_thread_arg->command, command.c_str());

		if(pthread_create(&update_chunk_thread, NULL, updateChunk, (void*)update_chunk_thread_arg)) { perror("Error: Creating update chunk thread"); exit(0); }
		pthread_detach(update_chunk_thread);
	} else if(command.find("select_ipport") == 0) {
		pthread_t select_ipport_thread;
		struct thread_arguments_structure * select_ipport_thread_arg = (struct thread_arguments_structure *)malloc(sizeof(struct thread_arguments_structure));
		select_ipport_thread_arg->dataTransferFD = data_transfer_fd;
		strcpy(select_ipport_thread_arg->command, command.c_str());

		if(pthread_create(&select_ipport_thread, NULL, selectIpPort, (void*)select_ipport_thread_arg)) { perror("Error: Creating select ip port thread"); exit(0); }
		pthread_detach(select_ipport_thread);
	} else if(command.find("download_file") == 0) {
		pthread_t download_file_thread;
		struct thread_arguments_structure * download_file_thread_arg = (struct thread_arguments_structure *)malloc(sizeof(struct thread_arguments_structure));
		download_file_thread_arg->dataTransferFD = data_transfer_fd;
		strcpy(download_file_thread_arg->command, command.c_str());

		if(pthread_create(&download_file_thread, NULL, initDownloadFile, (void*)download_file_thread_arg)) { perror("Error: Creating initialize download file thread"); exit(0); }
		pthread_detach(download_file_thread);
	} else {
		string response = "Error: Invalid command\n";
		sendResponse(response, data_transfer_fd);
		close(data_transfer_fd);
	}

	pthread_exit(NULL);
}

#endif /* TRACKERTASK_H_ */
