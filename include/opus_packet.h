#ifndef OPUS_PACKET_H
#define OPUS_PACKET_H

struct OpusPacket {
    unsigned char bytes[480];
    unsigned int size;
};

#endif // OPUS_PACKET_H