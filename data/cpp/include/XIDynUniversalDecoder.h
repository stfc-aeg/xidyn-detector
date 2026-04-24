/*
 * XIDynDecoder.h
 *
 *  Created on: 12 September 2024
 *      Author: Dominic Banks, STFC Detector Systems Software Group
 */

#ifndef INCLUDE_XIDYN_UNIVERSAL_DECODER_H_
#define INCLUDE_XIDYN_UNIVERSAL_DECODER_H_

#include <network/PacketProtocolDecoder.h>
#include <rte_byteorder.h>
#include <rte_memcpy.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>
#include <boost/shared_ptr.hpp>
#include <map>
#include <string>
#include "dpdk_version_compatibiliy.h"

// Mode enumeration
enum class XIDynMode {
    XIDYN_1X1_SINGLE_CHIP_COLUMN,
    XIDYN_1X1_SINGLE_CHIP,
    XIDYN_2X2_SINGLE_COLUMN,
    XIDYN_2X2_DOUBLE_COLUMN,
    XIDYN_2X2_TRIPLE_COLUMN
};

// Configuration for each mode
struct ModeConfiguration {
    std::size_t packets_per_frame;
    std::size_t payload_size;
    std::size_t frame_outer_chunk_size;
    FrameProcessor::DataType bit_depth;
    bool needs_reordering;
    int x_dimension;
    int y_dimension;
    int columns;
};

// Packet header structures
struct __rte_packed_begin X10GPacketHeader : PacketHeader
{
    rte_be64_t frame_number;
    rte_be64_t padding[6];
    rte_be32_t packet_number;
    uint8_t markers;
    uint8_t _unused_1;
    uint8_t padding_bytes;
    uint8_t readout_lane;
} __rte_packed_end;

// Frame header structure
struct __rte_packed_begin X10GRawFrameHeader : RawFrameHeader
{
    uint64_t frame_number;
    uint32_t packets_received;
    uint32_t sof_marker_count;
    uint32_t eof_marker_count;
    uint64_t frame_start_time;
    uint64_t frame_complete_time;
    uint32_t frame_time_delta;
    uint64_t image_size;
    uint8_t packet_state[1];  // Flexible array member
} __rte_packed_end;

class XIDynUniversalDecoder : public PacketProtocolDecoder
{
public:
    // String to mode mapping
    static const std::map<std::string, XIDynMode>& get_mode_string_map() {
        static const std::map<std::string, XIDynMode> mode_string_map = {
            {"1x1_single_chip_column", XIDynMode::XIDYN_1X1_SINGLE_CHIP_COLUMN},
            {"1x1_single_chip", XIDynMode::XIDYN_1X1_SINGLE_CHIP},
            {"2x2_single_column", XIDynMode::XIDYN_2X2_SINGLE_COLUMN},
            {"2x2_double_column", XIDynMode::XIDYN_2X2_DOUBLE_COLUMN},
            {"2x2_triple_column", XIDynMode::XIDYN_2X2_TRIPLE_COLUMN}
        };
        return mode_string_map;
    };
    
    // Constructor
    XIDynUniversalDecoder(XIDynMode initial_mode = XIDynMode::XIDYN_2X2_SINGLE_COLUMN) :
        PacketProtocolDecoder(
            get_mode_configs().at(initial_mode).packets_per_frame,
            get_mode_configs().at(initial_mode).payload_size,
            get_mode_configs().at(initial_mode).frame_outer_chunk_size
        ),
        current_mode_(initial_mode)
    {
        configure_for_mode(initial_mode);
    }
    
    virtual ~XIDynUniversalDecoder() { }
    
    // Mode management
    void set_mode(XIDynMode new_mode) {
        if (new_mode != current_mode_) {
            current_mode_ = new_mode;
            configure_for_mode(new_mode);
        }
    }
    
    XIDynMode get_mode() const { return current_mode_; }
    
    std::string get_mode_string() const {
        static const std::map<XIDynMode, std::string> mode_to_string = {
            {XIDynMode::XIDYN_1X1_SINGLE_CHIP_COLUMN, "1x1_single_column_chip"},
            {XIDynMode::XIDYN_1X1_SINGLE_CHIP, "1x1_single_chip"},
            {XIDynMode::XIDYN_2X2_SINGLE_COLUMN, "2x2_single_column"},
            {XIDynMode::XIDYN_2X2_DOUBLE_COLUMN, "2x2_double_column"},
            {XIDynMode::XIDYN_2X2_TRIPLE_COLUMN, "2x2_triple_column"}
        };
        
        auto it = mode_to_string.find(current_mode_);
        return (it != mode_to_string.end()) ? it->second : "unknown";
    }
    
    // Override virtual functions from PacketProtocolDecoder
    virtual const std::size_t get_frame_header_size(void) const {
        std::size_t packet_marker_size = sizeof(X10GRawFrameHeader().packet_state);
        std::size_t packet_header_size = sizeof(X10GRawFrameHeader) +
            (packet_marker_size * packets_per_frame_ - 1);
        return packet_header_size;
    }
    
    virtual const std::size_t get_packet_header_size(void) const {
        // Both header types are the same size
        return sizeof(X10GPacketHeader);
    }
    
    // Get packet payload offset based on mode
    const std::size_t get_packet_payload_offset(void) const {
        return sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr) +  sizeof(struct rte_udp_hdr) + get_packet_header_size();
    }

    // Frame header management
    void set_frame_number(RawFrameHeader* frame_hdr, uint64_t frame_number) {
        reinterpret_cast<X10GRawFrameHeader*>(frame_hdr)->frame_number = frame_number;
    }
    
    const uint64_t get_frame_number(RawFrameHeader* frame_hdr) const {
        return reinterpret_cast<X10GRawFrameHeader*>(frame_hdr)->frame_number;
    }
    
    void set_frame_start_time(RawFrameHeader* frame_hdr, uint64_t frame_start_time) {
        reinterpret_cast<X10GRawFrameHeader*>(frame_hdr)->frame_start_time = frame_start_time;
    }
    
    const uint64_t get_frame_start_time(RawFrameHeader* frame_hdr) const {
        return reinterpret_cast<X10GRawFrameHeader*>(frame_hdr)->frame_start_time;
    }
    
    void set_frame_complete_time(RawFrameHeader* frame_hdr, uint64_t frame_complete_time) {
        reinterpret_cast<X10GRawFrameHeader*>(frame_hdr)->frame_complete_time = frame_complete_time;
    }
    
    const uint64_t get_frame_complete_time(RawFrameHeader* frame_hdr) const {
        return reinterpret_cast<X10GRawFrameHeader*>(frame_hdr)->frame_complete_time;
    }
    
    const uint64_t get_image_size(RawFrameHeader* frame_hdr) const {
        return reinterpret_cast<X10GRawFrameHeader*>(frame_hdr)->image_size;
    }
    
    void set_image_size(RawFrameHeader* frame_hdr, uint64_t image_size) const {
        reinterpret_cast<X10GRawFrameHeader*>(frame_hdr)->image_size = image_size;
    }
    
    // Packet management
    bool set_packet_received(RawFrameHeader* frame_hdr, uint32_t packet_number) {
        if (packet_number >= packets_per_frame_) {
            return false;
        }
        
        X10GRawFrameHeader* x10g_hdr = reinterpret_cast<X10GRawFrameHeader*>(frame_hdr);
        x10g_hdr->packet_state[packet_number] = 1;
        x10g_hdr->packets_received++;
        return true;
    }
    
    const uint32_t get_packets_received(RawFrameHeader* frame_hdr) const {
        return reinterpret_cast<X10GRawFrameHeader*>(frame_hdr)->packets_received;
    }
    
    const uint32_t get_packets_dropped(RawFrameHeader* frame_hdr) const {
        return packets_per_frame_ - reinterpret_cast<X10GRawFrameHeader*>(frame_hdr)->packets_received;
    }
    
    const uint8_t get_packet_state(RawFrameHeader* frame_hdr, uint32_t packet_number) const {
        return reinterpret_cast<X10GRawFrameHeader*>(frame_hdr)->packet_state[packet_number];
    }
    
    // Mode-dependent packet header extraction
    const uint64_t get_frame_number(PacketHeader* packet_hdr) const {
        return reinterpret_cast<X10GPacketHeader*>(packet_hdr)->frame_number;
    }
    
    const uint32_t get_packet_number(PacketHeader* packet_hdr) const {
        return reinterpret_cast<X10GPacketHeader*>(packet_hdr)->packet_number;
    }
    
    // Frame reordering
    SuperFrameHeader* reorder_frame(SuperFrameHeader* frame_hdr, SuperFrameHeader* reordered_frame) {
        if (mode_config_.needs_reordering) {
            return reorder_2x2_column_mode(frame_hdr, reordered_frame);
        }
        
        // For histogram and raw modes, no reordering needed
        return frame_hdr;
    }
    
    SuperFrameHeader* reorder_frame(SuperFrameHeader* frame_hdr, boost::shared_ptr<FrameProcessor::Frame> reordered_frame) {
        // This overload typically isn't used for reordering
        return frame_hdr;
    }
    
    // Helper methods
    const std::size_t get_packets_per_frame() const { return packets_per_frame_; }
    const std::size_t get_payload_size() const { return payload_size_; }
    
    std::string get_bit_depth_string() const {
        switch(frame_bit_depth_) {
            case FrameProcessor::DataType::raw_16bit: return "16bit";
            case FrameProcessor::DataType::raw_32bit: return "32bit";
            default: return "unknown";
        }
    }
    
    // Frame dimension methods
    virtual std::vector<std::size_t> get_frame_dimensions(void) const override {
        std::vector<std::size_t> dims;
        dims.push_back(frame_y_resolution_);
        dims.push_back(frame_x_resolution_);

        return dims;
    }
    
private:
    XIDynMode current_mode_;
    ModeConfiguration mode_config_;
    
    // Mode configurations
    static const std::map<XIDynMode, ModeConfiguration>& get_mode_configs() {
        static const std::map<XIDynMode, ModeConfiguration> mode_configs = {
            //                                   packets  payload  chunk  bit_depth                           reorder   x    y   columns
            {XIDynMode::XIDYN_1X1_SINGLE_CHIP_COLUMN, {2,  4608,    1,     FrameProcessor::DataType::raw_16bit, false, 32,  144, 1}},
            {XIDynMode::XIDYN_1X1_SINGLE_CHIP,   {8,       6912,    1,     FrameProcessor::DataType::raw_16bit, false, 192, 144, 1}},
            {XIDynMode::XIDYN_2X2_SINGLE_COLUMN, {9,       8192,    1,     FrameProcessor::DataType::raw_16bit, true,  128, 288, 1}},
            {XIDynMode::XIDYN_2X2_DOUBLE_COLUMN, {18,      8192,    1,     FrameProcessor::DataType::raw_16bit, true,  256, 288, 2}},
            {XIDynMode::XIDYN_2X2_TRIPLE_COLUMN, {27,      8192,    1,     FrameProcessor::DataType::raw_16bit, false,  384, 288, 3}},

        };
        return mode_configs;
    }
    
    void configure_for_mode(XIDynMode mode) {
        mode_config_ = get_mode_configs().at(mode);
        
        // Update base class members
        packets_per_frame_ = mode_config_.packets_per_frame;
        payload_size_ = mode_config_.payload_size;
        frames_per_super_frame_ = mode_config_.frame_outer_chunk_size;
        frame_bit_depth_ = mode_config_.bit_depth;
        
        // Frame dimensions are fixed for all modes
        frame_x_resolution_ = mode_config_.x_dimension;
        frame_y_resolution_ = mode_config_.y_dimension;
    }
    
    SuperFrameHeader* reorder_2x2_column_mode(SuperFrameHeader* frame_hdr, SuperFrameHeader* reordered_frame) {
        // Copy the header
        rte_memcpy(reordered_frame, frame_hdr, 
                   get_super_frame_header_size() +
                   (get_frame_header_size() * frames_per_super_frame_));
        
        // Get pointers to the pixel data
        uint16_t* packed_data = reinterpret_cast<uint16_t*>(get_image_data_start(frame_hdr));
        uint16_t* output_memory = reinterpret_cast<uint16_t*>(get_image_data_start(reordered_frame));

        int packets_per_column = 9;
        int rows_per_packet = 32;
        int pixels_per_row = 128;
        int pixels_per_packet = rows_per_packet * pixels_per_row; // 4096
        int pixels_per_column = packets_per_column * pixels_per_packet; // 36864

        for (int frame = 0; frame < frames_per_super_frame_; frame++) {
            for (int column = 0; column < mode_config_.columns; column++) {
                for (int packet = 0; packet < packets_per_column; packet++) {
                    for (int row = 0; row < rows_per_packet; row++) {
                        uint16_t* input_memory_row =
                            (packed_data) + (column * pixels_per_column) +
                            (packet * pixels_per_packet) + (row * pixels_per_row);

                        if (row % 2 == 0) {

                            uint16_t* output_memory_top_row = 
                                (output_memory) + (pixels_per_row * column) + 
                                (packet * mode_config_.columns * (pixels_per_packet / 2)) +
                                ((row / 2) * (pixels_per_row * mode_config_.columns));

                            for (int pixel = 0; pixel < pixels_per_row; pixel++) {
                                output_memory_top_row[pixel] = input_memory_row[pixel];
                            };

                        } else {

                            uint16_t* output_memory_bottom_row =
                                (output_memory) + ((pixels_per_column - pixels_per_row) * (mode_config_.columns)) +
                                (pixels_per_row * column) - (packet * ((pixels_per_packet / 2) * mode_config_.columns)) -
                                ((row / 2) * (pixels_per_row * mode_config_.columns));

                            for (int pixel = 0; pixel < pixels_per_row; pixel++) {
                                output_memory_bottom_row[pixel] = input_memory_row[pixel];
                            }
                        }
                    }
                }
            }
        }
        
        return reordered_frame;
    }
};

#endif // INCLUDE_XIDYN_UNIVERSAL_DECODER_H_