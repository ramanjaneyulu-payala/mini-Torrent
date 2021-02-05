#include "TrackerTask.h"
using namespace std;

int main(int argc, char** argv) {
	if(argc<2) { perror("Error: Port number not given"); exit(0); }

	int socket_fd, port_number = atoi(argv[1]), data_transfer_fd;

	do{
		socket_fd = socket(AF_INET, SOCK_STREAM, 0);
		if (socket_fd == -1) perror("Error: Opening Socket");
	} while (socket_fd == -1);

	struct sockaddr_in server_details, client_details;
	bzero((char *) &server_details, sizeof(server_details));
	server_details.sin_family = AF_INET;
	server_details.sin_addr.s_addr = INADDR_ANY;
	server_details.sin_port = htons(port_number);

	if (bind(socket_fd, (struct sockaddr *) &server_details, sizeof(server_details)) == -1) { perror("Error: Binding"); exit(0); }

	listen(socket_fd, INT_MAX);
	unsigned int sockaddr_struct_length = sizeof(struct sockaddr_in);

	while(1) {
		data_transfer_fd = accept(socket_fd, (struct sockaddr *) &client_details, &sockaddr_struct_length);
		if(data_transfer_fd == -1) { perror("Error: Accepting"); exit(0); }

		pthread_t command_interpret_thread;
		struct thread_arguments_structure * command_interpret_thread_arg = (struct thread_arguments_structure *) malloc(sizeof(struct thread_arguments_structure));
		command_interpret_thread_arg->dataTransferFD = data_transfer_fd;
		if(pthread_create(&command_interpret_thread, NULL, interpretCommand, (void*) command_interpret_thread_arg)) { perror("Error: Creating command interpret thread"); exit(0); }
		pthread_detach(command_interpret_thread);
	}

	return 0;
}
