#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

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
                                .sin_addr = { htonl(INADDR_ANY) }, // 0.0.0.0
                            };

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
        char response[1] = { '\0' };

        // Send response
        // udpSocket.sendto(b"\0", (0.0.0.0, 2053))
        if (sendto(udpSocket, response, sizeof(response), 0, reinterpret_cast<struct sockaddr*>(&clientAddress), sizeof(clientAddress)) == -1) {
            perror("Failed to send response");
        }
    }

    // udpSocket.close()
    close(udpSocket);

    return 0;
}
