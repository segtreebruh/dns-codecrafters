#ifndef DNS_H
#define DNS_H

#include <iostream>
#include <cstring>
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

DNSHeader parseHeader(const DNSHeader& reqHeader);
DNSQuestion parseQuestion(const char* buffer, size_t readOffset);
DNSAnswer parseAnswer(const DNSQuestion& question);
size_t createResponse(char* buffer, char* response);

#endif