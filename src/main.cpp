#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h> // inet_pton

struct DNSHeader {
    uint16_t id;
    uint16_t flags; 
    uint16_t qdcount; 
    uint16_t ancount; 
    uint16_t nscount; 
    uint16_t arcount;

    DNSHeader() {
        id = 0;
        flags = 0;
        qdcount = 0;
        nscount = 0;
        arcount = 0;
    }
};

struct DNSQuestion {
    unsigned char* name;
    uint16_t type;
    uint16_t dnsclass;
};

struct DNSAnswer {
    unsigned char* name;
    uint16_t type;
    uint16_t dnsclass;
    unsigned int ttl;
    uint16_t length_rdata;
    int rdata; 
};

void createDnsHeader(char* response, const DNSHeader& reqHeader) {
    DNSHeader resHeader;

    resHeader.id = ntohs(reqHeader.id);
    // flags: 16 bit
    uint16_t qr = 1;
    // opcode: 14-11st bit, copied from reqHeader
    // shift right by 11, then opcode will be at the last 4 bit
    // 0xF = 0000 1111
    // & 0xF clears all garbage bit 
    uint16_t opcode = ntohs((reqHeader.flags) >> 11) & 0xF;
    uint16_t aa = 0;
    uint16_t tc = 0;
    // rd: 8th bit, copied from reqHeader
    // 1 bit only, so & 0x1
    uint16_t rd = ntohs((reqHeader.flags) >> 8) & 0x1;
    uint16_t ra = 0;
    uint16_t z = 0;
    uint16_t rcode = (opcode == 0) ? 0 : 4;

    resHeader.flags |= (qr << 15) | (opcode << 11) | (aa << 10) | (tc << 9) | (rd << 8) | (ra << 7) | (z << 4) | (rcode);
    resHeader.qdcount = reqHeader.qdcount;
    resHeader.ancount = reqHeader.ancount;
    resHeader.nscount = reqHeader.nscount;
    resHeader.arcount = reqHeader.arcount;

    memcpy(response, &resHeader, sizeof(resHeader));
}


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

    // inet_pton(AF_INET, "127.0.01", &serv_addr.sin_addr);

    if (bind(udpSocket, reinterpret_cast<struct sockaddr*>(&serv_addr), sizeof(serv_addr)) != 0) {
        std::cerr << "Bind failed: " << strerror(errno) << std::endl;
        return 1;
    }

    int bytesRead;
    char buffer[512];
    char response[512];
    socklen_t clientAddrLen = sizeof(clientAddress);

    while (true) {
        bytesRead = recvfrom(udpSocket, buffer, sizeof(buffer), 0, reinterpret_cast<struct sockaddr*>(&clientAddress), &clientAddrLen);
        if (bytesRead == -1) {
            perror("Error receiving data");
            break;
        }

        buffer[bytesRead] = '\0';
        std::cout << "Received " << bytesRead << " bytes: " << buffer << std::endl;

        const DNSHeader* reqHeader = reinterpret_cast<DNSHeader*>(buffer);
        createDnsHeader(response, *reqHeader);

        if (sendto(udpSocket, response, 13, 0, reinterpret_cast<struct sockaddr*>(&clientAddress), sizeof(clientAddress)) == -1) {
            perror("Failed to send response");
        }
    }

    // udpSocket.close()
    close(udpSocket);

    return 0;
}
