#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h> // inet_pton

int main() {
    // Flush after every std::cout / std::cerr
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    // Disable output buffering
    setbuf(stdout, NULL);

    // You can use print statements as follows for debugging, they'll be visible when running tests.
    std::cout << "Logs from your program will appear here!" << std::endl;

    int udpSocket;
    struct sockaddr_in clientAddress;

    /*
    Create an udp socket. 
    int socket(int domain, int type, int protocol);
    https://pubs.opengroup.org/onlinepubs/009696599/functions/socket.html

    @return: nonnegative integer representing socket file descriptor. -1 if fail.

    "File descriptor": imagine each process has a file description table. 
    It comprises of "stuff" that a process needs to run. 
    This function will return the index of the newly created socket within that table, 
    or -1 if fail.
    */
    udpSocket = socket(AF_INET, SOCK_DGRAM, 0); // Ipv4, udp
    if (udpSocket == -1) {
        std::cerr << "Socket creation failed: " << strerror(errno) << "..." << std::endl;
        return 1;
    }

    // Since the tester restarts your program quite often, setting REUSE_PORT
    // ensures that we don't run into 'Address already in use' errors
    int reuse = 1;
    if (setsockopt(udpSocket, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0) {
        std::cerr << "SO_REUSEPORT failed: " << strerror(errno) << std::endl;
        return 1;
    }

    /* 
    Binds udp socket to address. 
    sockaddr_in is builtin struct. 
    Bind to 0.0.0.0:2053.
    */
    sockaddr_in serv_addr = { .sin_family = AF_INET, // Ipv4
                                // (h)ost-(to)-(n)etwork-(s)hort 2053
                                .sin_port = htons(2053), 
                                // (h)ost-(to)-(n)etwork-(l)ong
                                // .sin_addr = { htonl(INADDR_ANY) }, // 0.0.0.0
                            };

    inet_pton(AF_INET, "127.0.01", &serv_addr.sin_addr);

    /*
    int bind(int socket, const struct sockaddr *address,
       socklen_t address_len);
    https://pubs.opengroup.org/onlinepubs/009695399/functions/bind.html
    @returns: 0 if success, -1 if error
    
    udpSocket.bind((0.0.0.0, 2053))
    */
    if (bind(udpSocket, reinterpret_cast<struct sockaddr*>(&serv_addr), sizeof(serv_addr)) != 0) {
        std::cerr << "Bind failed: " << strerror(errno) << std::endl;
        return 1;
    }

    int bytesRead;
    char buffer[512];
    socklen_t clientAddrLen = sizeof(clientAddress);

    while (true) {
        // Receive data

        // bytesRead = udpSocket.recv(512)
        bytesRead = recvfrom(udpSocket, buffer, sizeof(buffer), 0, reinterpret_cast<struct sockaddr*>(&clientAddress), &clientAddrLen);
        if (bytesRead == -1) {
            perror("Error receiving data");
            break;
        }

        // mark final byte as end
        // buffer overflow????
        buffer[bytesRead] = '\0';
        std::cout << "Received " << bytesRead << " bytes: " << buffer << std::endl;

        // Create an empty response
        // each char variable can only hold 8bit
        // so for example: id = 1234 = 0x04d2 - 16bit
        // have to split into two: 0x04, 0xd2 (each 8 bit)

        // each line is 16 bit (2 8bit elements)
        
        // src/main.cpp:100:19: error: narrowing conversion of ‘210’ from ‘int’ to ‘char’ [-Wnarrowing]
        // 100 |             0x04, 0xd2,

        // reason: char[] has sign, so only accept [-127, 127]
        // use unsigned char[] instead
        unsigned char response[12] = {
            // id = 1234 = 0000 0100 1101 0010 = 0x04d2 = 0x04 0xd2
            0x04, 0xd2,     
            
            // flags = 1 0000 0 0 0 0 000 0000 = 1000 0000 0000 0000 = 0x8000 = 0x80 0x00
            0x80, 0x00, 

            // everything else is zero
            0x00, 0x00,     // qdcount
            0x00, 0x00,     // ancount
            0x00, 0x00,     // nscount
            0x00, 0x00      // arcount
        };

        // Send response
        // udpSocket.sendto(b"\0", (0.0.0.0, 2053))
        if (sendto(udpSocket, response, 13, 0, reinterpret_cast<struct sockaddr*>(&clientAddress), sizeof(clientAddress)) == -1) {
            perror("Failed to send response");
        }
    }

    // udpSocket.close()
    close(udpSocket);

    return 0;
}
