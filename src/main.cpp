#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h> // inet_pton
#include <vector>

struct DNSHeader {
    uint16_t id;
    uint16_t flags; 
    uint16_t qdcount; 
    uint16_t ancount; 
    uint16_t nscount; 
    uint16_t arcount;

    DNSHeader(): id(0), flags(0), qdcount(0), ancount(0), nscount(0), arcount(0) {}
};

struct DNSQuestion {
    std::vector<uint8_t> qname;
    uint16_t type;
    uint16_t cls;
    bool isCompressed;
    size_t sz;

    DNSQuestion(): qname(std::vector<uint8_t>()), type(0), cls(0), isCompressed(false), sz(0) {}
};

struct DNSAnswer {
    std::vector<uint8_t> qname;
    uint16_t type;
    uint16_t cls;
    uint32_t ttl;
    uint16_t len;
    uint32_t data;

    DNSAnswer(): qname(std::vector<uint8_t>()), type(0), cls(0), ttl(0), len(0), data(0) {}
    DNSAnswer(DNSQuestion& q): qname(q.qname), type(q.type), cls(q.cls), ttl(0), len(0), data(0) {}

    size_t sz() { return 2 + 2 * 3 + 4 * 2; }
};

DNSHeader parseHeader(DNSHeader& reqHeader) {
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

    header.flags |= htons((qr << 15) | (opcode << 11) | (aa << 10) | (tc << 9) | (rd << 8) | (ra << 7) | (z << 4) | (rcode));
    header.qdcount = reqHeader.qdcount;
    header.ancount = reqHeader.qdcount;
    header.nscount = htons(0);
    header.arcount = htons(0);

    return header;
}

DNSQuestion parseQuestion(const char* buffer, size_t readOffset) {
    // readOffset: offset from beginning of buffer of current question
    DNSQuestion question;
    
    const char* ptr = buffer + readOffset;
    
    while (true) {
        // face 11 bit: go back 
        // 0xc0: 1100 0000
        if ((*ptr & 0xc0) == 0xc0) {
            // copy 16 bit from ptr to raw
            uint16_t raw;
            memcpy(&raw, ptr, sizeof(uint16_t));
            // 0011 1111 1111 1111
            // delete first two bits
            uint16_t offset = ntohs(raw) & 0x3FFF;
            ptr = buffer + offset;
            question.isCompressed = true;
            question.sz += 2;
            continue;
        }

        question.qname.push_back(*ptr);
        if (!question.isCompressed) question.sz += 1;
        ptr++;
        if (question.qname.back() == 0x00) break;
    }

    question.type = htons(1);
    question.cls = htons(1);
    question.sz += 4;

    return question;
}

DNSAnswer parseAnswer(DNSQuestion& question) {
    DNSAnswer answer(question);

    answer.ttl = htonl(60);
    answer.len = htons(4);
    answer.data = htonl(0x08080808); // 8.8.8.8

    return answer;
}

size_t createResponse(char* buffer, char* response) {
    DNSHeader* reqHeader = reinterpret_cast<DNSHeader*>(buffer);
    DNSHeader header = parseHeader(*reqHeader);
    int numQuestions = ntohs(header.qdcount);

    std::vector<DNSQuestion> questions;
    size_t questionsOffset = sizeof(DNSHeader);
    for (int i = 0; i < numQuestions; i++) {
        DNSQuestion question = parseQuestion(buffer, questionsOffset);
        questions.push_back(question);
        questionsOffset += question.sz;
    }

    std::vector<DNSAnswer> answers;
    for (auto& q: questions) {
        DNSAnswer answer = parseAnswer(q);
        answers.push_back(answer);
    }

    // store offsets of each question wrt response at the start of each question
    std::vector<uint16_t> qOffsets;
    char* ptr = response;
    memcpy(ptr, &header, sizeof(DNSHeader));
    ptr += sizeof(DNSHeader);

    for (auto& q: questions) {
        qOffsets.push_back(ptr - response);
        memcpy(ptr, q.qname.data(), q.qname.size()); ptr += q.qname.size();
        memcpy(ptr, &q.type, sizeof(q.type)); ptr += sizeof(q.type);
        memcpy(ptr, &q.cls, sizeof(q.cls)); ptr += sizeof(q.cls);
    }

    for (int i = 0; i < answers.size(); i++) {
        // in compressed answers, name shrink to exactly two bytes 
        // "encrypt" name field into pointer field starting with 11 and 14 bit for offset
        uint16_t ptr_field = htons(0xC000 | qOffsets[i]);
        memcpy(ptr, &ptr_field, sizeof(ptr_field)); ptr += sizeof(ptr_field);

        auto& a = answers[i];
        memcpy(ptr, &a.type, sizeof(a.type)); ptr += sizeof(a.type);
        memcpy(ptr, &a.cls,  sizeof(a.cls));  ptr += sizeof(a.cls);
        memcpy(ptr, &a.ttl,  sizeof(a.ttl));  ptr += sizeof(a.ttl);
        memcpy(ptr, &a.len,  sizeof(a.len));  ptr += sizeof(a.len);
        memcpy(ptr, &a.data, sizeof(a.data)); ptr += sizeof(a.data);
    }
    
    return ptr - response;
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

        size_t responseLen = createResponse(buffer, response);

        if (sendto(udpSocket, response, responseLen, 0, reinterpret_cast<struct sockaddr*>(&clientAddress), sizeof(clientAddress)) == -1) {
            perror("Failed to send response");
        }
    }

    // udpSocket.close()
    close(udpSocket);

    return 0;
}
