#include <stdint.h>
#include <sys/queue.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <inttypes.h>
#include <getopt.h>
#include <math.h>

#include <stdint.h>
#include <inttypes.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>
#include <pthread.h>
#include <string.h>
#include <rte_memzone.h>
#include <rte_errno.h>
#include <rte_hexdump.h>
#include <stdbool.h>
#include <getopt.h>
#include <rte_byteorder_64.h>

#include "frameheader.h"

struct config_struct config_;

int l2_len = sizeof(struct rte_ether_hdr);
int l3_len = sizeof(struct rte_ipv4_hdr);
int len_4 = sizeof (struct rte_udp_hdr);


struct rte_ring *buffer_to_update[MAX_BUFFERS];
struct rte_ring *buffer_updated[MAX_BUFFERS];

uint64_t counter = 0;

static const struct rte_eth_conf port_conf_default = {
	.rxmode = { .max_lro_pkt_size = JUMBO_FRAME_ELEMENT_SIZE,
				.offloads =  RTE_ETH_TX_OFFLOAD_IPV4_CKSUM |
						RTE_ETH_TX_OFFLOAD_UDP_CKSUM }	
};

uint64_t GbToDelay(double x) {
    return (uint64_t)round(15868.90 / x - 0.0762 * x + 4.56);
}

uint32_t roundUpTo2PowerMinus1(uint32_t n) {
    if (n == 0) {
        return 0;
    }

    // Find the position of the most significant bit set
    uint32_t msb_pos = 31 - rte_bsf32(~n);

    // Construct the number 2^(msb_pos + 1) - 1
    return (1U << (msb_pos + 1)) - 1;
}

static inline int
port_init(struct rte_mempool *mbuf_pool)
{
	struct rte_eth_conf port_conf = port_conf_default;
	const uint16_t rx_rings = 1, tx_rings = 1;
	int retval;
	uint16_t q;


	retval = rte_eth_dev_configure(0, rx_rings, tx_rings, &port_conf);
	if (retval != 0)
		return retval;

	/* Allocate and set up 1 RX queue per Ethernet port. */
	for (q = 0; q < rx_rings; q++) {
		retval = rte_eth_rx_queue_setup(0, q, RX_RING_SIZE,
				rte_eth_dev_socket_id(0), NULL, mbuf_pool);
		if (retval < 0)
			return retval;
	}

	/* Allocate and set up 1 TX queue per Ethernet port. */
	for (q = 0; q < tx_rings; q++) {
		retval = rte_eth_tx_queue_setup(0, q, TX_RING_SIZE,
				rte_eth_dev_socket_id(0), NULL);
		if (retval < 0)
			return retval;
	}

	/* Start the Ethernet port. */
	retval = rte_eth_dev_start(0);
	if (retval < 0)
		return retval;

	

	struct rte_ether_addr addr;
	retval = rte_eth_macaddr_get(0, &addr);
	if (retval != 0)
		return retval;

    rte_eth_dev_set_mtu(0, JUMBO_FRAME_ELEMENT_SIZE); // Set MTU to max data size (minus Ethernet header)

	printf("Port %u MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
			   " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "\n",
			0,
			addr.addr_bytes[0], addr.addr_bytes[1],
			addr.addr_bytes[2], addr.addr_bytes[3],
			addr.addr_bytes[4], addr.addr_bytes[5]);

	return 0;
}

int main(int argc, char **argv) {
    // Initialize the Environment Abstraction Layer (EAL)
    int ret = rte_eal_init(argc, argv);
    if (ret < 0) {
        rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");
    }

    argc -= ret;
	argv += ret;

    config_.interval = DEFAULT_INTERVAL;
	config_.starting_frame_number = DEFAULT_STARTING_FRAME_NUMBER;
	config_.number_of_frames = DEFAULT_NUMBER_OF_FRAMES;
	strncpy(config_.destination_ip_address, DEFAULT_DEST_IP_ADDR, sizeof(config_.destination_ip_address));
	strncpy(config_.destination_mac_address, DEFAULT_DEST_MAC_ADDR, sizeof(config_.destination_mac_address));
	config_.destination_port = DEFAULT_DEST_PORT;
	strncpy(config_.source_ip_address, DEFAULT_SOURCE_IP_ADDR, sizeof(config_.source_ip_address));
	strncpy(config_.source_mac_address, DEFAULT_SOURCE_MAC_ADDR, sizeof(config_.source_mac_address));
	config_.source_port = DEFAULT_SOURCE_PORT;
	config_.test_pattern_mode = DEFAULT_TEST_PATTERN_MODE;
	config_.drop_frames = DEFAULT_DROP_FRAMES;
	config_.drop_packets = DEFAULT_DROP_PACKETS;
    config_.pixel_value = DEFAULT_PIXEL_VALUE;
    config_.min_pixel_value = DEFAULT_PIXEL_MIN_VALUE;
    config_.max_pixel_value = DEFAULT_PIXEL_MAX_VALUE;
    config_._12_bit_mode = DEFAULT_BIT_MODE;
    config_.channels = DEFAULT_CHANNELS;
    config_.columns = DEFAULT_COLUMNS;
    config_.chips = DEFAULT_CHIPS;
    config_.verbose = false;

    // Parse user config from command line
    int opt;
    int option_index;

    static struct option long_option[] = {
        {"interval", required_argument, NULL, 'i'},
        {"start_frame", required_argument, NULL, 's'},
        {"frames", required_argument, NULL, 'f'},
        {"dest_ip", required_argument, NULL, 'd'},
        {"dest_mac", required_argument, NULL, 'm'},
        {"src_ip", required_argument, NULL, 'x'},
        {"src_mac", required_argument, NULL, 'y'},
        {"drop_packet", required_argument, NULL, 'p'},
        {"drop_frame", required_argument, NULL, 'r'},
        {"src_port", required_argument, NULL, 'u'},
        {"dst_port", required_argument, NULL, 'v'},
        {"test_pattern", required_argument, NULL, 't'},
        {"columns", required_argument, NULL, 'n'},
        {"chips", required_argument, NULL, 'z'},
        {"help", no_argument, NULL, 'h'},
        {"bandwith_test", no_argument, NULL, 'b'},
        {"channels", required_argument, NULL, 'c'},
        {"pixel_value", required_argument, NULL, 'a'},
        {"pixel_max", required_argument, NULL, 'e'},
        {"pixel_min", required_argument, NULL, 'g'},
        {"12_bit", no_argument, NULL, 'l'},
        {"verbose", no_argument, NULL, 'k'},
        {NULL, 0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "", long_option, &option_index)) != -1) {
        switch (opt) {
            case 'i':
                printf("Found interval argument: %s\n", optarg);
                config_.interval = atoi(optarg);
                break;
            case 's':
                printf("Found start_frame argument: %s\n", optarg);
                config_.starting_frame_number = atoi(optarg);
                break;
            case 'f':
                printf("Found frames argument: %s\n", optarg);
                config_.frames = atoi(optarg);
                break;
            case 'd':
                printf("Found dest_ip argument: %s\n", optarg);
                strncpy(config_.destination_ip_address, optarg, sizeof(config_.destination_ip_address));
                break;
            case 'm':
                printf("Found dest_mac argument: %s\n", optarg);
                strncpy(config_.destination_mac_address, optarg, sizeof(config_.destination_mac_address));
                break;
            case 'x':
                printf("Found src_ip argument: %s\n", optarg);
                strncpy(config_.source_ip_address, optarg, sizeof(config_.source_ip_address));
                break;
            case 'y':
                printf("Found src_mac argument: %s\n", optarg);
                strncpy(config_.source_mac_address, optarg, sizeof(config_.source_mac_address));
                break;
            case 'p':
                printf("Found drop_packet argument: %s\n", optarg);
                config_.drop_packets = atoi(optarg);
                break;
            case 'r':
                printf("Found drop_frame argument: %s\n", optarg);
                config_.drop_frames = atoi(optarg);
                break;
            case 'u':
                printf("Found src_port argument: %s\n", optarg);
                config_.source_port = atoi(optarg);
                break;
            case 'v':
                printf("Found dst_port argument: %s\n", optarg);
                config_.destination_port = atoi(optarg);
                break;
            case 't':
                config_.test_pattern_mode = atoi(optarg);
                printf("Found test pattern argument: %ld\n", config_.test_pattern_mode);
                break;
            case 'b':
                printf("Found bandwith_test argument\n");
                config_.interval = GbToDelay(1);
                break;
            case 'c':
				printf("Found channels argument\n");
                config_.channels = atoi(optarg);
                break;
            case 'n':
                printf("Found columns argument\n");
                config_.columns = atoi(optarg);
                break;
            case 'z':
                printf("Found chip argument\n");
                switch (atoi(optarg)){
                    case 1:
                        config_.chips = 1;
                        break;
                    case 4:
                        config_.chips = 4;
                        break;
                    default:
                        printf("Invalid chip argument, defaulting to 1\n");
                        config_.chips = 1;
                        break;
                }
                break;
            case 'a':
				printf("Found pixel value argument\n");
                config_.pixel_value = atoi(optarg);
                break;
            case 'e':
				printf("Found pixel maximum value argument\n");
                config_.max_pixel_value = atoi(optarg);
                break;
            case 'g':
				printf("Found pixel minimum value argument\n");
                config_.min_pixel_value = atoi(optarg);
                break;
            case 'l':
				printf("Found 12 bit mode argument\n");
                config_._12_bit_mode = true;
                break;
            case 'k':
				printf("Found verbose mode argument\n");
                config_.verbose = true;
                break;
            case 'h':
                // Display help message
                printf("\n\n\n");
				printf("--interval : Time delay in seconds between frames \n");
				printf("--start_frame : Frame number to start sending from \n");
				printf("--frames : number of frames to send \n");
				printf("--dest_ip : Destination Ip address in the format xxx.xxx.xxx.xxx \n");
				printf("--dest_mac : Destination MAC address in the format XX:XX:XX:XX:XX:XX \n");
				printf("--dest_port : Destination port to use 0 - 65535 \n");
				printf("--src_ip : Source Ip address in the format xxx.xxx.xxx.xxx \n");
				printf("--src_mac : Source MAC address in the format XX:XX:XX:XX:XX:XX \n");
				printf("--src_port : Source port to use 0 - 65535 \n");
				printf("--drop_packet : A value between 0-1000 of the chance to drop a packet \n");
				printf("--drop_frame : A value between 0-1000 of the chance to drop packets in that frame \n");
				printf("--test_pattern : 1 - repeating test pattern, 0 - simulated beam \n");
                printf("--chips : number of chips to simulate (1 or 4) \n");
                printf("--columns : number of columns to simulate (1 or 6) \n");
				printf("--help : Display this message \n\n\n");

                rte_eal_cleanup();
                exit(0);
            default:
                return -1;
        }
    }

    uint64_t data_len;

    uint64_t base_channel_size = config_._12_bit_mode ? 240 : 320;

    // if (config_.channels <= 12) {
    //     config_.packets_per_frame = 1;
    //     data_len = 2 * base_channel_size * config_.channels;
    // } else {
    //     config_.packets_per_frame = 2;
    //     data_len = base_channel_size * config_.channels;
    // }

    // config_.packets_per_frame = 9; //27;

    // uint32_t IMAGE_WIDTH, IMAGE_HEIGHT;
    uint64_t FRAME_PIXELS;
    uint64_t num_pixels;
    uint32_t IMAGE_WIDTH, IMAGE_HEIGHT;

    if (config_.chips == 4) {

        num_pixels = 4096; //config_.channels * 320 / config_.packets_per_frame;
        data_len = num_pixels * sizeof(uint16_t); // 8192;

        IMAGE_WIDTH = 384;
        IMAGE_HEIGHT = 288;
        FRAME_PIXELS = IMAGE_WIDTH * IMAGE_HEIGHT;  // 110592
        printf("COLUMNS: %ld, CHANNELS: %ld\n", config_.columns);

        config_.packets_per_frame = 9 * config_.channels;

    } else if (config_.chips == 1) {

        if (config_.columns == 6) {

            num_pixels = 3456; // 18(rows per packet) * 192 (image width)
            data_len = num_pixels * sizeof(uint16_t); // 55296

            IMAGE_WIDTH = 192;
            IMAGE_HEIGHT = 144;
            FRAME_PIXELS = IMAGE_WIDTH * IMAGE_HEIGHT; // 27648

            config_.packets_per_frame = 8;

        } else if(config_.columns == 1) {

            num_pixels = 2304; // 72(rows per packet) * 32 (image width)
            data_len = num_pixels * sizeof(uint16_t); // 36864

            IMAGE_WIDTH = 32;
            IMAGE_HEIGHT = 144;
            FRAME_PIXELS = IMAGE_WIDTH * IMAGE_HEIGHT; // 4608

            config_.packets_per_frame = 2;

        } else {
            printf("Invalid columns argument, defaulting to 6\n");
            exit(1);
        }
    };

    printf("data len: %ld", data_len);

    struct rte_ether_hdr *eth_hdr;
	struct rte_ipv4_hdr *ip_hdr;
    struct rte_udp_hdr *udp_hdr;
    struct xiDyn_hdr *xiDyn_h;
	uint16_t *xiDyn_data;


	printf("PPF: %ld, buf size: %ld\n", config_.packets_per_frame, PREPARED_FRAMES * config_.packets_per_frame);
	
    struct rte_mempool *mbuf_pool = rte_pktmbuf_pool_create(MBUF_POOL_NAME, MBUF_POOL_SIZE, RTE_MEMPOOL_CACHE_MAX_SIZE, RTE_MBUF_PRIV_ALIGN, JUMBO_FRAME_ELEMENT_SIZE, rte_socket_id());

    printf("Made mbuf pool\n");

    if (mbuf_pool == NULL)
        rte_exit(EXIT_FAILURE, "Cannot create mbuf pool, check permissions\n");

    if (port_init(mbuf_pool) != 0)
        rte_exit(EXIT_FAILURE, "Cannot init port %" PRIu8 "\n", 0);

    printf("Port init\n");

    struct rte_mbuf *PacketBuffs[PREPARED_FRAMES][config_.packets_per_frame];

    printf("declare rte_mbuf\n");

    rte_be64_t frame_counter = 0;
    uint32_t buf;
    

    // Seed random number generator
    srand(time(NULL));

    // Calculate the total number of mbufs to allocate
    unsigned total_mbufs = PREPARED_FRAMES * config_.packets_per_frame;

    printf("Allocating pkts: %d \n", total_mbufs);

    // Allocate the bulk of mbufs
    int rett = rte_pktmbuf_alloc_bulk(mbuf_pool, &PacketBuffs[0][0], total_mbufs);
    if (rett != 0) {
        // Somethings broke
    }

    // 12-bit or 16-bit max value
    uint16_t max_value = config_._12_bit_mode ? 0x0FFF : 0xFFFF; 
    uint16_t *frame_buffer = malloc(FRAME_PIXELS * sizeof(uint16_t));
    if (!frame_buffer) {
        rte_exit(EXIT_FAILURE, "Failed to allocate frame buffer\n");
    }

    for (uint64_t frames = 0; frames < PREPARED_FRAMES; frames++) {
        for (rte_be32_t packets = 0; packets < config_.packets_per_frame; packets++) {
            if (PacketBuffs[frames][packets] == NULL) {
                printf("ERROR: Failed to allocate in mbuf\n");
                rte_eal_cleanup();
                exit(EXIT_FAILURE);
            }
            uint16_t total_packet_length = l2_len + l3_len + len_4 + data_len + xiDyn_HDR_SIZE;

            PacketBuffs[frames][packets]->pkt_len = total_packet_length;
            PacketBuffs[frames][packets]->data_len = total_packet_length;

            struct rte_ether_hdr *eth_hdr = rte_pktmbuf_mtod(PacketBuffs[frames][packets], struct rte_ether_hdr *);
            struct rte_ipv4_hdr *ip_hdr = (struct rte_ipv4_hdr *)((char *)eth_hdr + l2_len);
            struct rte_udp_hdr *udp_hdr = (struct rte_udp_hdr *)((char *)ip_hdr + l3_len);
            struct xiDyn_hdr *xiDyn_h = (struct xiDyn_hdr *)((char *)udp_hdr + len_4);
            uint16_t *xiDyn_data = (uint16_t *)((char *)xiDyn_h + sizeof(struct xiDyn_hdr));

            xiDyn_h->frame_number = frame_counter;
            xiDyn_h->packet_number = packets;

            rte_ether_unformat_addr(config_.source_mac_address, (void*)&eth_hdr->src_addr);
            rte_ether_unformat_addr(config_.destination_mac_address, (void*)&eth_hdr->dst_addr);

            inet_pton(AF_INET, config_.destination_ip_address, &buf);
            ip_hdr->dst_addr = buf;
            inet_pton(AF_INET, config_.source_ip_address, &buf);
            ip_hdr->src_addr = buf;

            eth_hdr->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
            udp_hdr->dst_port = rte_bswap16(config_.destination_port);
            udp_hdr->src_port = rte_bswap16(config_.source_port);
            udp_hdr->dgram_len = rte_bswap16(data_len + UDP_HDR_SIZE + sizeof(struct xiDyn_hdr));

            ip_hdr->fragment_offset = 0;
            ip_hdr->ihl = 5;
            ip_hdr->next_proto_id = 17;
            ip_hdr->packet_id = (uint16_t)(rand() % (MAX_PACKET_ID + 1)); // Generate random packet ID
            ip_hdr->time_to_live = 128;
            ip_hdr->total_length = rte_cpu_to_be_16(total_packet_length - l2_len);
            ip_hdr->type_of_service = 0;
            ip_hdr->version = 4;
            ip_hdr->version_ihl = RTE_IPV4_VHL_DEF;

            uint64_t pixel_index = 0;
            uint16_t pixel_value = 0;

            // Cast xiDyn_data to a byte pointer for 12-bit mode
            

            // uint64_t num_pixels = 384 * 288 / num_packets; //4096; //config_.channels * 320 / config_.packets_per_frame;
            uint16_t *temp_array = (uint16_t *)malloc(num_pixels * sizeof(uint16_t)* config_.packets_per_frame);
            while (pixel_index < num_pixels * config_.packets_per_frame) {

                switch (config_.test_pattern_mode) {
                    // Set it all to same number
                    case 0:
                        pixel_value = config_.pixel_value;
                        break;
                    // Incrementing values
                    case 1:
                        pixel_value = (pixel_index % (max_value + 1));
                        break;
                    // Set it all to frame number
                    case 2:
                        pixel_value = frame_counter % (max_value + 1);
                        break;
                    // Random values
                    case 3:
                        pixel_value = (rand() % 
                            (config_.max_pixel_value - config_.min_pixel_value + 1))
                            + config_.min_pixel_value;
                        break;
                    case 4:
                        pixel_value = packets;
                        break;
                    default:
                        pixel_value = 0;
                        break;
                }

                temp_array[pixel_index++] = pixel_value;
            }

            #define COLUMN_WIDTH 128

            if (config_.chips == 4) {
                for (int row = 0; row < 32; row++) {
                    if (row % 2 == 0) {
                        // rte_memcpy((uint16_t *)xiDyn_data + (row * 128), temp_array + (row * IMAGE_WIDTH), COLUMN_WIDTH * sizeof(uint16_t));
                        for (int idx = 0; idx < 128; idx++) {
                            xiDyn_data[(row * 128) + idx] = temp_array[(int)((row + (32 * (packets % 9))) / 2) * 128 + idx];
                        }
                    } else {
                        for (int idx = 0; idx < 128; idx++) {
                            xiDyn_data[(row * 128) + idx] = temp_array[(IMAGE_HEIGHT - 1 - (int)((row  + (32 * (packets % 9))) / 2)) * 128 + idx];
                        }
                        // rte_memcpy((uint16_t *)xiDyn_data + (row * 128), temp_array + ((IMAGE_HEIGHT - 1 - row) * IMAGE_WIDTH), COLUMN_WIDTH * sizeof(uint16_t));
                    }
                }
            } else if (config_.chips == 1) {
                rte_memcpy(xiDyn_data, temp_array, data_len);
            };

            // if (col % 9 == 8) {
            //     col++;
            // }


            // Copy the data into the packet as is
            // rte_memcpy(xiDyn_data, temp_array, data_len);

            // rte_memcpy(xiDyn_data, frame_buffer, data_len);

            // Free the temp 16-bit array now we don't need it anymore
            free(temp_array);
        }
        frame_counter++;
        if(config_.verbose)
            printf("finished creating frame %ld\n", frame_counter);
    }
    


    printf("Press any key to start sending packets...\n");
    getchar();

    uint64_t ticks_per_sec = rte_get_tsc_hz();
    rte_be64_t total_frames_sent = 0, total_dropped_frames = 0, total_dropped_packets = 0;
    int nb_tx;
    uint16_t port_id = 0;
    uint64_t last = rte_get_tsc_cycles();

    while (total_frames_sent < config_.frames) {
        bool drop_frame = (rte_rand() % 1000) < config_.drop_frames;

        if (!drop_frame) {
            for (int pkt_index = 0; pkt_index < config_.packets_per_frame; pkt_index++) {
                bool drop_packet = (rte_rand() % 1000) < config_.drop_packets;
                if (!drop_packet) {
                    nb_tx = 0;
                    while (nb_tx != 1) {
                        nb_tx = rte_eth_tx_burst(port_id, 0, &PacketBuffs[total_frames_sent % PREPARED_FRAMES][pkt_index], 1);
                    }
                } else {
                    total_dropped_packets++;
                }
            }
        } else {
            total_dropped_frames++;
        }

        rte_delay_us(config_.interval);

        total_frames_sent++;

        // Prepare the next frame (if necessary)
        if (total_frames_sent < config_.frames) {
            for (int pkt_index = 0; pkt_index < config_.packets_per_frame; pkt_index++) {
                struct rte_ether_hdr *eth_hdr = rte_pktmbuf_mtod(PacketBuffs[(total_frames_sent % PREPARED_FRAMES)][pkt_index], struct rte_ether_hdr*);
                struct xiDyn_hdr *xiDyn_h = (struct xiDyn_hdr*)((char*)eth_hdr + l2_len + l3_len + len_4);

                xiDyn_h->frame_number = total_frames_sent; // Set frame number for next frame

                // uint16_t *xiDyn_data = (uint16_t *)((char *)xiDyn_h + 64);
                // for (int index = 0; index < 80; index++) {
                //     *xiDyn_data = (uint16_t)total_frames_sent; // Update data for next frame
                //     xiDyn_data++;
                // }
            }
        }

        // Print progress every 1 million frames
        if (total_frames_sent % 1000000 == 0) {
            printf("Sent frame: %ld\n", total_frames_sent);
        }

        if (total_frames_sent >= config_.frames) {
            float time_taken = (float)(rte_get_tsc_cycles() - last) / ticks_per_sec;
            printf("Sent %ld frames in %f seconds! (%f frames per second) At data rate of %f Gb/s\n", total_frames_sent, time_taken, (float)total_frames_sent / time_taken, (float)(total_frames_sent * config_.packets_per_frame * data_len * 8) / (time_taken * 1e9));
            printf("Total frames dropped: %ld | Expected frames dropped: %ld\n", total_dropped_frames, (uint64_t)( config_.frames * ((float)config_.drop_frames / 1000.0)));
            printf("Total packets dropped: %ld | Expected packets dropped: %ld\n", total_dropped_packets, (uint64_t)( config_.frames * ((float)config_.drop_packets / 1000.0)));
            break;  
        }
    }

// Exit the application


    rte_eal_mp_wait_lcore();

    return 0;
}
