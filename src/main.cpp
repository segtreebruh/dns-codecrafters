#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <vector>

#include "dns.cpp"

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

    sockaddr_in serv_addr = { .sin_family = AF_INET, // Ipv4
                                .sin_port = htons(2053), 
                                .sin_addr = { htonl(INADDR_ANY) }, // 0.0.0.0
                            };

    if (bind(udpSocket, reinterpret_cast<struct sockaddr*>(&serv_addr), sizeof(serv_addr)) != 0) {
        std::cerr << "Bind failed: " << strerror(errno) << std::endl;
        return 1;
    }

    int bytesRead;
    char buffer[512];
    socklen_t clientAddrLen = sizeof(clientAddress);

    while (true) {
        char response[512] = {0};
        bytesRead = recvfrom(udpSocket, buffer, sizeof(buffer), 0, reinterpret_cast<struct sockaddr*>(&clientAddress), &clientAddrLen);
        if (bytesRead == -1) {
            perror("Error receiving data");
            break;
        }

        buffer[bytesRead] = '\0';
        std::cout << "Received " << bytesRead << " bytes: " << buffer << std::endl;

        std::cerr << "raw bytes: ";
        for (int i = 0; i < bytesRead; i++) {
            std::cerr << std::hex << (int)(unsigned char)buffer[i] << " ";
        }
        std::cerr << std::endl;
        
        // DO NOT DO THIS
        // DO NOT USE reinterpret_cast FOR DYNAMICALLY ALLOCATED VARIABLES
        // const DNSQuery* dnsQuery = reinterpret_cast<DNSQuery*>(buffer);
        // std::cerr << "query" << endl;

        size_t responseLen = createResponse(buffer, response);

        if (sendto(udpSocket, response, responseLen, 0, reinterpret_cast<struct sockaddr*>(&clientAddress), sizeof(clientAddress)) == -1) {
            perror("Failed to send response");
        }
    }

    // udpSocket.close()
    close(udpSocket);

    return 0;
}
