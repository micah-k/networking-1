#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>    // socket, bind
#include <sys/socket.h>   // socket, bind, listen, inet_ntoa
#include <netinet/in.h>   // htonl, htons, inet_ntoa
#include <arpa/inet.h>    // inet_ntoa
#include <netdb.h>        // gethostbyname
#include <unistd.h>       // read, write, close
#include <strings.h>      // bzero
#include <netinet/tcp.h>  // SO_REUSEADDR
#include <sys/uio.h>      // writev
#include <sys/time.h>
#include <pthread.h>

#define BUFSIZE 1500

using namespace std;

struct threadData
{
  int sd;
  int repetition;
};

//
// IF_FALSE_RETURN
//
// Macro to convieniently add basic error handling
//
#define IF_FALSE_RETURN(test, msg) \
  if (!(test)) \
  { \
    printf("%s\n", msg); \
    return -1; \
  }

// Usage
//
// Quick documentation when the caller passes the wrong number of parameters
//
void Usage()
{
  printf("server <port> <repetition>\n"
         "\n"
         "<port>        The server IP port for this experiment\n"
         "\n"
         "<repetition>  Number of repetitions for this experiment\n"
         "\n");
}

// ConvertParameterToInt
//
// several of our parameters are expected to be well formed numbers
// since no negative values are expected, return -1 to indicate an error.
//
int ConvertParameterToInt(char* value)
{
  char* endptr = NULL;
  long int result = strtol(value, &endptr, 0);
  if (*value == '\0' || *endptr != '\0')
    return -1;

  return (int)result;
}

void* your_function(void* whatever)
{
  threadData* data = (threadData*)whatever;
  
  int sd = data->sd;
  int repetition = data->repetition;

  delete data;

  char databuf[BUFSIZE];

  // Start a timer by calling gettimeofday.
  timeval start;
  gettimeofday(&start, NULL);

  int count = 0;
  for (int reps = 0; reps < repetition; reps++)
  {
    // Repeat reading data from the client into databuf[BUFSIZE]. 
    // Note that the read system call may return without reading 
    // the entire data if the network is slow.
    int remaining = BUFSIZE;

    while(remaining > 0)
    {
      int bytesRead = read(sd, databuf, remaining);
      if (bytesRead == -1)
      {
        printf("read failed\n");
        exit(1);
      }

      count += 1;
      remaining -= bytesRead;
    }
  }

  // Stop the timer by calling gettimeofday

  timeval end;
  gettimeofday(&end, NULL);

  // Send the number of read calls made, as an acknowledgement.
  int written = 0;
  while (written < sizeof(count))
  {
    int bytesWritten = write(sd, &count, sizeof(count));
    if (bytesWritten == -1)
    {
      printf("write failed\n");
      exit(1);
    }

    written += bytesWritten;
  }


  if (close(sd) != 0)
  {
      printf("close failed\n");
      exit(1);
  }

  //Print out the statistics as shown below:
  printf("data-receiving time = %ld usec\n",
    (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec));
}

int main(int argc, char** argv)
{
  //Make sure we got the right number of parameters or display usage
  if (argc != 3)
  {
    Usage();
    return 1;
  }

  // parse parameters
  int port = ConvertParameterToInt(argv[1]);
  IF_FALSE_RETURN(port > 1024, "port must be a positive number greater than 1024");

  int repetition = ConvertParameterToInt(argv[2]);
  IF_FALSE_RETURN(repetition > 0, "repetition must be a positive number");

  // Declare and initialize sockaddr_in structure
  sockaddr_in acceptSockAddr;
  bzero((char*)&acceptSockAddr, sizeof(acceptSockAddr));
  acceptSockAddr.sin_family      = AF_INET;
  acceptSockAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  acceptSockAddr.sin_port        = htons(port);

  // Open a stream-oriented socket with the Internet address family.
  int serverSd = socket(AF_INET, SOCK_STREAM, 0);
  IF_FALSE_RETURN(serverSd != -1, "socket failed to create file descriptor");

  // Set the SO_REUSEADDR option. 
  //
  // Note: this option is useful to prompt OS to release the server port
  // as soon as your server process is terminated.
  const int on = 1;
  IF_FALSE_RETURN(setsockopt(serverSd, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(int)) != -1, "setsockopt failed");

  // Bind this socket to its local address.
  IF_FALSE_RETURN(bind(serverSd, (sockaddr*)&acceptSockAddr, sizeof(acceptSockAddr)) != -1, "bind failed");

  // Instruct the operating system to listen to up to 8 connection requests from clients at a time
  IF_FALSE_RETURN(listen(serverSd, 8) != -1, "listen failed");

  printf("Server: listening on port %d\n", port);

  // Receive a request from a client by calling accept that will return a new socket specific to this connection request.
  sockaddr_in newSockAddr;
  socklen_t newSockAddrSize = sizeof(newSockAddr);
  int newSd = accept(serverSd, (sockaddr*)&newSockAddr, &newSockAddrSize);
  printf("Server: accepted connection\n");
  while (newSd != -1)
  {
    //Create a new thread
    threadData* data = new threadData();
    data->sd = newSd;
    data->repetition = repetition;

    pthread_t thread;
    if (pthread_create(&thread, NULL, your_function, data) != 0)
    {
      delete data;
      printf("pthread_create failed\n");
      return -1;
    }
    printf("Server: created thread\n");

    // Loop back to the accept command and wait for a new connection
    newSd = accept(serverSd, (sockaddr*)&newSockAddr, &newSockAddrSize);
    printf("Server: accepted another connection\n");
  }

  IF_FALSE_RETURN(newSd != -1, "accept failed");
  return 0;
}

