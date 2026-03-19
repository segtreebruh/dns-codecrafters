#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h> // inet_pton
#include "dns.h"


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