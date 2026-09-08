#ifndef _X10GPROTOCOL_H_
#define _X10GPROTOCOL_H_

#include <stdint.h>

#include <rte_byteorder.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BYTES_PER_CHANNEL 16 * 2
#define MBUF_POOL_NAME "MBUF_POOL"
#define MBUF_POOL_SIZE (100 * 5000 * 2)
//#define RTE_IPV4_VHL_DEF 0x45
#define UDP_HDR_SIZE 8
#define xiDyn_HDR_SIZE 64
#define MAX_PACKET_ID 65535

#define MAX_BUFFERS 10
#define MAX_FRAMES 100000
#define MAX_PACKETS 10000
#define UDP_OVERHEAD 96
#define PREPARED_FRAMES 5000

#define RX_RING_SIZE 1024
#define TX_RING_SIZE 32768

#define JUMBO_FRAME_ELEMENT_SIZE 0x2600

struct __rte_packed_begin xiDyn_hdr {
    rte_be64_t frame_number;
    rte_be64_t padding[6];
    rte_be32_t packet_number;
    uint8_t markers;
    uint8_t _unused_1;
    uint8_t padding_bytes;
    uint8_t readout_lane;
}__rte_packed_end;

struct xiDyn_data{
	uint16_t payload;
};


struct __rte_packed_begin config_struct {
    uint64_t buffers; 
    uint64_t frames;
    uint64_t channels;
	uint64_t chips;
	uint64_t packets_per_frame;
    uint64_t interval;
	uint64_t starting_frame_number;
	uint64_t number_of_frames;
	char destination_ip_address[20];
	char destination_mac_address[20];
    uint16_t destination_port;
	char source_ip_address[20];
	char source_mac_address[20];
    uint16_t source_port;
	uint64_t test_pattern_mode;
	uint64_t columns;
	uint64_t drop_packets;
	uint64_t drop_frames;
	uint64_t pixel_value;
	uint64_t max_pixel_value;
	uint64_t min_pixel_value;
	bool _12_bit_mode;
	bool verbose;
	
} __rte_packed_end;


// Defaults

#define DEFAULT_INTERVAL 1000
#define DEFAULT_STARTING_FRAME_NUMBER 0
#define DEFAULT_NUMBER_OF_FRAMES 1000
#define DEFAULT_DEST_IP_ADDR "10.100.0.6"
#define DEFAULT_DEST_MAC_ADDR "58:a2:e1:c1:fe:e8"
#define DEFAULT_DEST_PORT 1234
#define DEFAULT_SOURCE_IP_ADDR "10.100.0.7"
#define DEFAULT_SOURCE_MAC_ADDR "9c:63:c0:db:bb:dc"
#define DEFAULT_SOURCE_PORT 1234
#define DEFAULT_TEST_PATTERN_MODE 1
#define DEFAULT_DROP_PACKETS 0
#define DEFAULT_DROP_FRAMES 0
#define DEFAULT_PIXEL_VALUE 0x5A5A
#define DEFAULT_PIXEL_MIN_VALUE 0
#define DEFAULT_PIXEL_MAX_VALUE 65335
#define DEFAULT_BIT_MODE false // false == 16-bit, true == 12-bit 
#define DEFAULT_CHANNELS 1
#define DEFAULT_CHIPS 4
#define DEFAULT_COLUMNS 6


#ifdef __cplusplus
}
#endif

#endif // _X10GPROTOCOL_H_