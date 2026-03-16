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

    DNSHeader(): id(0), flags(0), qdcount(0), ancount(0), nscount(0), arcount(0) {}
};

size_t parseHeader(char* response, const char* buffer) {
    DNSHeader reqHeader;
    memcpy(&reqHeader, buffer, sizeof(DNSHeader));

    DNSHeader header;

    header.id = reqHeader.id;
    uint16_t qr = 1;
    // opcode: 14-11st bit 
    // shift right 11, then 14-11st bit at 4 rightmost bit
    // 0xF = 1111
    // & 0xF to clear all garbage bit to the left (15th bit)
    // while preserving 4 rightmost bit 
    uint16_t opcode = (ntohs(reqHeader.flags) >> 11) & 0xF;
    uint16_t aa = 0;
    uint16_t tc = 0;
    // rd: 8th bit
    // & 0x1 to preserve 0th bit
    uint16_t rd = (ntohs(reqHeader.flags) >> 8) & 0x1;
    uint16_t ra = 0;
    uint16_t z = 0;
    uint16_t rcode = (opcode == 0) ? 0 : 4;

    header.flags |= (qr << 15) | (opcode << 11) | (aa << 10) | (tc << 9) | (rd << 8) | (ra << 7) | (z << 4) | (rcode);
    header.flags = htons(header.flags);
    header.qdcount = htons(reqHeader.qdcount);
    header.ancount = htons(reqHeader.ancount);
    header.nscount = htons(reqHeader.nscount);
    header.arcount = htons(reqHeader.arcount);

    memcpy(response, &header, sizeof(header));

    return sizeof(DNSHeader);
}

size_t parseQuestion(char* response, const char* buffer) {
    const char* ptr = buffer;

    while (*ptr != 0x00) ptr++;
    ptr++; // skip 0x00

    memcpy(response, &buffer, ptr - buffer);
    response += ptr - buffer;

    uint16_t type = htons(1);
    uint16_t cls = htons(1);

    memcpy(response, &type, sizeof(type));
    response += sizeof(type);
    memcpy(response, &cls, sizeof(cls));

    return ptr - buffer + sizeof(type) + sizeof(cls);
}

size_t parseAnswer(char* response, const char* qRes, size_t qLen) {
    memcpy(response, qRes, qLen);
    response += qLen;

    // htonl since 32bit (int)
    uint32_t ttl = htonl(60);
    uint16_t len = htons(4);
    uint32_t data = htonl(0x08080808); // 8.8.8.8

    memcpy(response, &ttl, sizeof(ttl));
    response += sizeof(ttl);
    memcpy(response, &len, sizeof(len));
    response += sizeof(len);
    memcpy(response, &data, sizeof(data));

    return qLen + sizeof(ttl) + sizeof(len) + sizeof(data);
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

        size_t headerLen = parseHeader(response, buffer);
        size_t questionLen = parseQuestion(response + headerLen, buffer);
        const char* qRes = response + headerLen;
        size_t answerLen = parseAnswer(response + headerLen + questionLen, qRes, questionLen);

        size_t responseLen = headerLen + questionLen + answerLen;

        if (sendto(udpSocket, response, responseLen, 0, reinterpret_cast<struct sockaddr*>(&clientAddress), sizeof(clientAddress)) == -1) {
            perror("Failed to send response");
        }
    }

    // udpSocket.close()
    close(udpSocket);

    return 0;
}
