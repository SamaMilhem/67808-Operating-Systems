#include <dirent.h>
#include <iostream>
#include "client.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>
#include <arpa/inet.h>
#include <armadillo>
#include <sys/ipc.h>
#include <sys/shm.h>

void print_summary_report (int total_servers, int total_vm, int total_host,
                           int total_container, std::vector<std::string> &messages);

void socketConnection (live_server_info &server_info, const std::string
&ip_add, unsigned short port)
{
  // Create a socket
  server_info.server_fd = -1;
  int client_fd = socket (AF_INET, SOCK_STREAM, 0);
  if (client_fd == -1)
  {
    server_info.client_fd = -1;
    return;
  }

  // Initialize server address structure
  sockaddr_in server_addr{};
  memset (&server_addr, 0, sizeof (server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons (port);

  // Convert IP address to binary form
  if (inet_pton (AF_INET, ip_add.c_str (), &server_addr.sin_addr) > 0)
  {
    struct timeval timeout = {.tv_sec=5, .tv_usec=0};
    fd_set write_fds;
    FD_ZERO (&write_fds);
    FD_SET (client_fd, &write_fds);

    // Wait for the socket to become writable
    int select_val = select (client_fd + 1, nullptr,
                             &write_fds, nullptr,
                             &timeout);
    if (select_val > 0)
    {
      // Attempt to connect
      if (connect (client_fd, (struct sockaddr *) &server_addr,
                   sizeof (server_addr)) == -1)
      {
        close (client_fd);
        server_info.client_fd = -1;
        return;
      }

      // Cleanup if connection fails
      server_info.client_fd = client_fd;
      return;
    }
  }
  close (client_fd);
  server_info.client_fd = -1;
}

void sharedMemoAt (live_server_info &server_info, const std::string
&shm_pathname, int shm_proj_id)
{
  // Initialize shared memory ID to -1
  server_info.shmid = -1;

  // Generate a unique key for the shared memory segment
  int shmid = shmget (ftok (shm_pathname.c_str (),
                            shm_proj_id), SHARED_MEMORY_SIZE, 0666);
  // If shared memory exists
  if (shmid != -1)
  {
    // Attempt to attach to the shared memory segment
    void *shared_memory = shmat (shmid, nullptr, 0);
    if (shared_memory != (void *) -1)
    {
      // Successful attachment, update the server_info structure
      server_info.shmid = shmid;
      return;
    }

    // If attachment fails, reset shared memory ID
    server_info.shmid = -1;
  }
}

int count_servers (const std::string &client_files_directory,
                   std::vector<live_server_info> &servers)
{

  DIR *dir = opendir (client_files_directory.c_str ());
  if (!dir)
  {

    std::cerr << OPEN_DIR_ERR
              << std::endl;
    exit (1);
  }

  dirent *entry;
  while ((entry = readdir (dir)) != nullptr)
  {
    // Ignore "." and ".." entries
    if (std::string (entry->d_name) == "." ||
        std::string (entry->d_name) == "..")
      continue;
    // Construct the full path of the info file
    std::string info_file_path =
        client_files_directory + "/" + entry->d_name;
    std::ifstream get_file (info_file_path);

    if (!get_file.is_open ())
    {
      std::cerr << OPEN_FILE_ERR
                << std::endl;
      exit (1);
    }

    // Read server details from the info file
    live_server_info server_info;
    server_info.info_file_path = info_file_path;
    std::string ip_add;
    unsigned short port;
    std::string shm_pathname;
    int shm_proj_id;
    get_file >> ip_add >> port >> shm_pathname >> shm_proj_id;
    get_file.close ();

    // Initialize connection details
    socketConnection (server_info, ip_add, port);
    sharedMemoAt (server_info, shm_pathname, shm_proj_id);

    // Add the server to the list
    servers.push_back (server_info);
  }

  closedir (dir);

  // Sort the servers vector based on the info_file_path
  std::sort (servers.begin (), servers.end (),
             [] (const live_server_info &a, const live_server_info &b)
             { return a.info_file_path < b.info_file_path; });

  return (int) servers.size ();
}

void print_server_infos (const std::vector<live_server_info> &servers)
{
  int total_servers = (int) servers.size ();
  int total_vm = 0;
  int total_host = 0;
  int total_container = 0;
  std::vector<std::string> messages = std::vector<std::string> ();

  for (const auto &server: servers)
  {
    // Check for shared memory segment
    if (server.shmid != -1)
    {
      std::string shm_msg;
      get_message_from_shm (server, shm_msg);

      if (!shm_msg.empty ())
      {
        messages.push_back (shm_msg);
      }
    }

    // Check for socket connection
    if (server.client_fd != -1)
    {
      std::string socket_msg;
      get_message_from_socket (server, socket_msg);

      if (!socket_msg.empty ())
      {
        messages.push_back (socket_msg);
      }
    }

    // Categorize the server
    if (server.shmid != -1 && server.client_fd != -1)
    {
      total_host++;
    }
    else if (server.shmid != -1 && server.client_fd == -1)
    {
      total_container++;
    }
    else if (server.shmid == -1 && server.client_fd == -1)
    {
      total_vm++;
    }
  }

  print_summary_report (total_servers, total_vm, total_host, total_container, messages);
}

void print_summary_report (int total_servers, int total_vm, int total_host,
                           int total_container, std::vector<std::string> &messages)
{

  std::cout << "Total Servers: " << total_servers << std::endl;
  std::cout << "Host: " << total_host << std::endl;
  std::cout << "Container: " << total_container << std::endl;
  std::cout << "VM: " << total_vm << std::endl;
  std::cout << "Messages: ";

  for (const auto &msg: messages)
  {
    std::cout << msg << " ";
  }
  std::cout << std::endl;
}

void get_message_from_socket (const live_server_info &server, std::string &msg)
{
  const int max_buffer_size = SHARED_MEMORY_SIZE;
  char buffer[max_buffer_size];
  int bytes_read;

  // Read data from the socket until the end of the message or an error occurs
  while ((bytes_read =
              read (server.client_fd, buffer, max_buffer_size - 1)) > 0)
  {
    buffer[bytes_read] = '\0'; // Null-terminate the received message
    msg += buffer; // Append the received data to the msg string
  }
}

void get_message_from_shm (const live_server_info &server, std::string &msg)
{
  // Attach to the shared memory segment
  void *shared_memory = shmat (server.shmid, nullptr, 0);
  if (shared_memory == (void *) -1)
  {
    // Close the socket associated with the server
    if (server.client_fd != -1)
    {
      // Close the socket associated with the server
      close (server.client_fd);
    }
    std::cerr << SHMAT_ERR
              << std::endl;
    exit (1);
  }

  // Read the message from the shared memory segment
  char *buffer = static_cast<char *>(shared_memory);
  msg = ""; // Initialize the message string

  // Read characters from shared memory until a null terminator is encountered
  for (int i = 0; i < SHARED_MEMORY_SIZE && buffer[i] != '\0'; i++)
  {
    msg += buffer[i];
  }

  // Detach from the shared memory segment
  if (shmdt (shared_memory) == -1)
  {
    if (server.client_fd != -1)
    {
      // Close the socket associated with the server
      close (server.client_fd);
    }
    std::cerr << SHMDT_ERR
              << std::endl;
    exit (1);
  }
}

void disconnect (const std::vector<live_server_info> &servers)
{
  for (const auto &server: servers)
  {
    if (server.client_fd != -1)
    {
      // Close the socket associated with the server
      close (server.client_fd);
    }
  }
}

void run (const std::string &client_files_directory)
{
  std::vector<live_server_info> servers;
  count_servers (client_files_directory, servers);
  print_server_infos (servers);
  disconnect (servers);
}
