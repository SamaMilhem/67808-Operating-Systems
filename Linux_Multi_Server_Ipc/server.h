#ifndef EX5_SERVER_H
#define EX5_SERVER_H

#include "../67808-Operating-Systems/Linux_Multi_Server_Ipc/globals.h"

static const char *const SOCKET_CREATION_ERR = "System Error: Socket "
                                               "creation failed";
static const char *const GETHOSTNAME_ERR = "System Error: gethostname failed";
static const char *const HOSTBYNAME_ERR = "System Error: Host creation failed";
static const char *const BINDING_ERR = "System Error: Socket binding failed";
static const char *const LISTEN_ERR = "System Error: Socket listening failed";
static const char *const MEM_ALLOC_ERR = "System Error: Failed to allocate "
                                         "shared memory segment.";
static const char *const FILE_CREATION_ERR = "System Error: Unable to create"
                                             " info file";
static const char *const SELECT_ERR = "System Error: Select syscall failed";
static const char *const ACCEPT_ERR = "System Error: Accept syscall failed";
static const char *const WRITE_ERR = "System Error: Writing data to socket "
                                     "failed";
static const char *const ATTACH_MEM_ERR = "System Error: Attaching to shared"
                                          " memory failed";
static const char *const MEM_DEALLOCATION_ERR = "System Error: Failed to "
                                                "deallocate shared memory segment";
static const char *const FILE_DELETION_ERR = "System Error: Failed to delete"
                                             " the info file";
/***
 * information used to start up a server
 */
struct server_setup_information
{
    unsigned short port;
    std::string shm_pathname;
    int shm_proj_id;
    std::string info_file_name;
    std::string info_file_directory;
};

void
start_communication (const server_setup_information &setup_info, live_server_info &server);
void
create_info_file (const server_setup_information &setup_info, live_server_info &server);
void get_connection (live_server_info &server);
void write_to_socket (const live_server_info &server, const std::string &msg);
void write_to_shm (const live_server_info &server, const std::string &msg);
void shutdown (const live_server_info &server);
void
run (const server_setup_information &setup_info, const std::string &shm_msg, const std::string &socket_msg);

#endif //EX5_SERVER_H