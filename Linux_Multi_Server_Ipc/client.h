#ifndef EX5_CLIENT_H
#define EX5_CLIENT_H

#include "globals.h"
#include <string>
#include <vector>

static const char *const OPEN_DIR_ERR = "System Error: Failed to open client files directory";
static const char *const OPEN_FILE_ERR = "System Error: Failed to open an info file";
static const char *const SHMAT_ERR = "System Error: Attaching to shared memory segment";
static const char *const SHMDT_ERR = "System Error: Detaching from shared memory segment";
int count_servers(const std::string& client_files_directory, std::vector<live_server_info> &servers);
void print_server_infos(const std::vector<live_server_info>& servers);
void get_message_from_socket(const live_server_info &server, std::string& msg);
void get_message_from_shm(const live_server_info &server, std::string& msg);
void disconnect(const std::vector<live_server_info> &servers);
void run(const std::string& client_files_directory);

#endif