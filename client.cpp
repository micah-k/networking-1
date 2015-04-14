#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>    // socket, bind
#include <sys/socket.h>   // socket, bind, listen, inet_ntoa
#include <netinet/in.h>   // htonl, htons, inet_ntoa
#include <arpa/inet.h>    // inet_ntoa
#include    <netdb.h>     // gethostbyname
#include    <unistd.h>    // read, write, close
#include   <strings.h>    // bzero
#include <netinet/tcp.h>  // SO_REUSEADDR
#include <sys/uio.h>      // writev
#include <sys/time.h>
#include <pthread.h>

using namespace std;

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
  printf("client <port> <repetition> <nbufs> <bufsize> <serverIp> <type> [test]"
         "\n"
         "<port>        The server IP port for this experiment\n"
         "\n"
         "<repetition>  Number of repetitions for this experiment\n"
         "\n"
         "<nbufs>       Number of data buffers for this experiment\n"
         "\n"
         "<bufsize>     Size of each data buffer for this experiment\n"
         "\n"
         "<serverIp>    The server IP name for this experiment\n"
         "\n"
         "<type>        Type of transfer for this experiment\n"
         "\n"
         "              1 - multiple writes\n"
         "\n"
         "              2 - writev\n"
         "\n"
         "              3 - single write\n"
         "\n"
         "[test]        optional test number\n");
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

// CreateSocket
//
// Creates socket file descriptor and sockaddr_in
//
int CreateSocket(char* name, int port, sockaddr_in* sockAddr)
{
  struct hostent* host = gethostbyname( name );
  IF_FALSE_RETURN(host != NULL, "gethostbyname failed to resolve name");

  bzero( (char*)sockAddr, sizeof( sockAddr ) );

  sockAddr->sin_family = AF_INET;
  sockAddr->sin_addr.s_addr = inet_addr( inet_ntoa( *(struct in_addr*)*host->h_addr_list ) );
  sockAddr->sin_port = htons( port );

  int sd = socket( AF_INET, SOCK_STREAM, 0 );
  IF_FALSE_RETURN(sd != -1, "socket failed to create file descriptor");
  return sd;
}

//
// FillBuffer
//
// Fill buffer with random data
//
void FillBuffer(char* databuf, int bufsize)
{
  for (int i = 0; i < bufsize; i++)
      databuf[i] = (char)random();
}

int main(int argc, char** argv)
{
  //Make sure we got the right number of parameters or display usage
  if (argc < 7 || argc > 8)
  {
    Usage();
    return 1;
  }

  // parse parameters
  int port = ConvertParameterToInt(argv[1]);
  IF_FALSE_RETURN(port > 1024, "port must be a positive number greater than 1024");

  int repetition = ConvertParameterToInt(argv[2]);
  IF_FALSE_RETURN(repetition > 0, "repetition must be a positive number");

  int nbufs = ConvertParameterToInt(argv[3]);
  IF_FALSE_RETURN(nbufs > 0, "nbufs must be a positive number");

  int bufsize = ConvertParameterToInt(argv[4]);
  IF_FALSE_RETURN(bufsize != 0, "bufsize must be a positive number");

  IF_FALSE_RETURN(nbufs * bufsize == 1500, "nbufs * bufsize must be 1500");

  char* serverIp = argv[5];

  int type = ConvertParameterToInt(argv[6]);
  IF_FALSE_RETURN(type >= 1 && type <= 3, "type must be 1, 2, or 3");

  int testno = 1;
  if (argc == 8)
    testno = ConvertParameterToInt(argv[7]);

  // Open a new socket and establish a connection to a server.
  sockaddr_in sendSockAddr;
  int sd = CreateSocket(serverIp, port, &sendSockAddr);
  if (sd == -1)
    return 1;

  IF_FALSE_RETURN(connect(sd, (sockaddr*)&sendSockAddr, sizeof(sendSockAddr)) == 0, "connect failed");

  //Allocate databuf[nbufs][bufsize].
  char databuf[nbufs][bufsize];
  FillBuffer((char*)databuf, nbufs * bufsize);

  struct timeval start;
  struct timeval lap;
  struct timeval end;

  // Start a timer by calling gettimeofday.
  gettimeofday(&start, NULL);

  // Repeat the repetition times of data transfers,
  for (int reps = 0; reps < repetition; reps++)
  {
    // each based on type such as 1: multiple writes, 2: writev, or 3: single write
    switch(type)
    {
    case 1: // multiple writes
      {
        for (int j = 0; j < nbufs; j++)
          IF_FALSE_RETURN(write(sd, databuf[j], bufsize) != -1, "write failed");
      }
      break;
    case 2: // writev
      {
        struct iovec vector[nbufs];
        for (int j = 0; j < nbufs; j++)
        {
          vector[j].iov_base = databuf[j];
          vector[j].iov_len = bufsize;
        }
        IF_FALSE_RETURN(writev(sd, vector, nbufs) != -1, "writev failed");
      }
      break;
    case 3: // single write
      IF_FALSE_RETURN(write(sd, databuf, nbufs * bufsize) != -1, "write failed");
      break;
    }
  }

  // Lap the timer by calling gettimeofday, where lap - start = data-sending time.
  gettimeofday(&lap, NULL);

  // Receive from the server an integer acknowledgement that shows how many times the server called read( ).
  int count = 0;
  int reads = 0;
  while (reads < sizeof(count))
  {
    int bytesRead = read(sd, &count, sizeof(count));
    IF_FALSE_RETURN(bytesRead != -1, "read failed");
    reads += bytesRead;
  }

  // Stop the timer by calling gettimeofday, where stop - start = round-trip time.
  gettimeofday(&end, NULL);

  // Print out the statistics as shown below:
  printf("Test %d: data-sending time = %ld usec, round-trip time = %ld usec, #reads = %d\n",
    testno,
    (lap.tv_sec - start.tv_sec) * 1000000 + (lap.tv_usec - start.tv_usec),
    (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec),
    count);

  // Close the socket
  IF_FALSE_RETURN(close(sd) == 0, "close failed");

  return 0;
}

