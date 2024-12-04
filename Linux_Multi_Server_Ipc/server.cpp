#include <iostream>
#include "server.h"
#include <sys/shm.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <cstring>
#include <arpa/inet.h>

int create_newServer (const server_setup_information &setup_info);
int sharedMemoCreation (const char *shm_pathname, int shm_proj_id, int
server_fd);

void start_communication (const server_setup_information &setup_info,
                          live_server_info &server)
{
  // Step 1: Create the server socket and bind it to a specific port
  server.server_fd = create_newServer (setup_info);

  // Step 2: Create and initialize the shared memory segment
  server.shmid = sharedMemoCreation (setup_info.shm_pathname.c_str (),
                                     setup_info.shm_proj_id,
                                     server.server_fd);
}

int create_newServer (const server_setup_information &setup_info)
{

  char myname[MAXHOSTNAME + 1];
  struct sockaddr_in sa{};
  struct hostent *hp;

  // Create the socket
  int server_fd = socket (AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0)
  {
    std::cerr << SOCKET_CREATION_ERR << std::endl;
    exit (1);
  }

  // Get the server hostname
  if (gethostname (myname, MAXHOSTNAME) == -1)
  {
    close (server_fd);
    std::cerr << GETHOSTNAME_ERR << std::endl;
    exit (1);
  }

  // Get host information
  hp = gethostbyname (myname);
  if (hp == nullptr)
  {
    close (server_fd);
    std::cerr << HOSTBYNAME_ERR << std::endl;
    exit (1);
  }

  // Initialize sockaddr_in structure
  memset (&sa, 0, sizeof (struct sockaddr_in));
  sa.sin_family = hp->h_addrtype;
  memcpy (&sa.sin_addr, hp->h_addr, hp->h_length);
  sa.sin_port = htons (setup_info.port);

  // Bind the socket to the port
  if (bind (server_fd, (struct sockaddr *) &sa,
            sizeof (struct sockaddr_in)) < 0)
  {
    std::cerr << BINDING_ERR << std::endl;
    close (server_fd);
    exit (1);
  }

  // Listen for incoming connections
  if (listen (server_fd, 3) == -1)
  {
    std::cerr << LISTEN_ERR << std::endl;
    close (server_fd);
    exit (1);
  }
  return server_fd;
}

int sharedMemoCreation (const char *shm_pathname, int shm_proj_id, int
server_fd)
{
  // Generate a unique key for the shared memory segment
  key_t shm_key = ftok (shm_pathname, shm_proj_id);

  // Attempt to allocate a shared memory segment
  int shmid = shmget (shm_key, SHARED_MEMORY_SIZE,
                      0666 | IPC_CREAT);

  if (shmid == -1)
  {
    // Close the server socket if shared memory creation fails
    close (server_fd);
    std::cerr << MEM_ALLOC_ERR << std::endl;
    return -1;
  }
  return shmid;
}

void freeResources (const live_server_info &server)
{
  // Close the server's socket if it is open
  if (server.server_fd != -1)
  {
    close (server.server_fd);
  }
  // Deallocate the shared memory segment if it is allocated
  if (server.shmid != -1)
  {
    shmctl (server.shmid, IPC_RMID, nullptr);
  }
  // Close the client's socket if it is open
  if (server.client_fd != -1)
  {
    close (server.client_fd);
  }
}

char *getIP (const live_server_info &server)
{
  char myname[MAXHOSTNAME + 1];
  struct hostent *hp;

  // Get the server's hostname
  if (gethostname (myname, MAXHOSTNAME) == -1)
  {
    std::cerr << GETHOSTNAME_ERR << std::endl;
    freeResources (server);
    exit (1);
  }

  // Resolve the hostname to an IP address
  hp = gethostbyname (myname);
  if (hp == nullptr)
  {
    freeResources (server);
    std::cerr << HOSTBYNAME_ERR << std::endl;
    exit (1);
  }

  // Convert the resolved IP address to a string and return it
  return inet_ntoa (*((struct in_addr *) hp->h_addr));
}

void create_info_file (const server_setup_information &setup_info,
                       live_server_info &server)
{
  // Generate the file path
  std::string file_path = setup_info.info_file_directory +
                          "/" + setup_info.info_file_name;

  // Open the file for writing
  std::ofstream get_info_file (file_path);
  if (!get_info_file.is_open ())
  {
    freeResources (server);
    std::cerr << FILE_CREATION_ERR << std::endl;
    exit (1);
  }

  // Write server details to the file
  std::stringstream file_content;
  file_content << getIP (server) << std::endl;  // IP address
  file_content << setup_info.port << std::endl; // Port number
  file_content << setup_info.shm_pathname
               << std::endl; // Shared memory pathname
  file_content << setup_info.shm_proj_id
               << std::endl; // Shared memory project ID
  get_info_file << file_content.str ();
  get_info_file.close ();

  // Store the file path in the server's runtime information
  server.info_file_path = file_path;
}

void get_connection (live_server_info &server)
{

  // Set the timeout to 120 seconds
  const int TIMEOUT = 120;

  // Set up the timeout for select
  struct timeval timeout = {.tv_sec = TIMEOUT, .tv_usec = 0};

  // Set up the fd_sets for select
  fd_set read_fds;
  FD_ZERO (&read_fds);
  FD_SET (server.server_fd, &read_fds);

  // Wait for incoming connections with the given timeout
  int select_result = select (server.server_fd + 1, &read_fds,
                              nullptr, nullptr, &timeout);

  if (select_result == -1)
  {
    std::cerr << SELECT_ERR << std::endl;
    freeResources (server);
    exit (1);
  }
  if (select_result == 0)
  {
    server.client_fd = -1; // Timeout occurred, no connection established
    return;
  }
  // Incoming connection detected, call accept
  int client_fd = accept (server.server_fd, nullptr, nullptr);
  if (client_fd == -1)
  {
    std::cerr << ACCEPT_ERR << std::endl;
    freeResources (server);
    exit (1);
  }
  server.client_fd = client_fd;   // Save the client's socket file descriptor

}

void write_to_socket (const live_server_info &server, const std::string &msg)
{
  // Send the message via the server socket to the connected client
  ssize_t bytes_written = write (server.client_fd, msg.c_str (),
                                 msg.size ());
  if (bytes_written == -1)
  {
    std::cerr << WRITE_ERR << std::endl;
    freeResources (server);
    exit (1);
  }
}

void write_to_shm (const live_server_info &server, const std::string &msg)
{

  // Attach to the shared memory segment
  char *shm_ptr = static_cast<char *>(shmat (server.shmid, (void *) 0, 0));
  if (shm_ptr == (void *) -1)
  {
    std::cerr << ATTACH_MEM_ERR
              << std::endl;
    freeResources (server);
    exit (1);
  }

  // Write the message to the shared memory segment
  strncpy (shm_ptr,
           msg.c_str (), msg.length ());
  shm_ptr[msg.length ()] = '\0';

  // Detach from the shared memory segment
  shmdt (shm_ptr);
}
void shutdown (const live_server_info &server)
{
  // Close the server's socket if it is open
  if (server.server_fd != -1)
  {
    close (server.server_fd);
  }

  // Close the client's socket if it is open
  if (server.client_fd != -1)
  {
    close (server.client_fd);
  }

  // Deallocate the shared memory segment if it is allocated
  if (server.shmid != -1)
  {
    if (shmctl (server.shmid, IPC_RMID, nullptr) == -1)
    {
      std::cerr << MEM_DEALLOCATION_ERR
                << std::endl;
    }
  }

  // Delete the info file if it exists
  if (!server.info_file_path.empty ())
  {
    if (remove (server.info_file_path.c_str ()) != 0)
    {
      std::cerr << FILE_DELETION_ERR << std::endl;
    }
  }
}
void run (const server_setup_information &setup_info, const std::string &
shm_msg, const std::string &socket_msg)
{

  live_server_info server_info = {};
  start_communication (setup_info, server_info);
  create_info_file (setup_info, server_info);
  write_to_shm (server_info, shm_msg);
  get_connection (server_info);

  if (server_info.client_fd != -1)
  {
    write_to_socket (server_info, socket_msg);
  }

  sleep (10); // Simulate server runtime
  shutdown (server_info); // Cleanup resources
}
