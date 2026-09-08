#include "XIDynPlugin.h"
#include "version.h"

namespace FrameProcessor
{
    XIDynPlugin::XIDynPlugin() :
        DpdkFrameProcessorPlugin(),
        current_mode_(XIDynMode::XIDYN_2X2_SINGLE_COLUMN),
        decoder_initialized_(false)
    {
        logger_ = Logger::getLogger("FP.XIDynPlugin");
        logger_->setLevel(Level::getAll());
        LOG4CXX_INFO(logger_, "XIDynPlugin version " << this->get_version_long() << " loaded");


    }
    
    XIDynPlugin::~XIDynPlugin()
    {
        LOG4CXX_TRACE(logger_, "XIDynPlugin destructor.");
    }
    
    void XIDynPlugin::configure(OdinData::IpcMessage& config, OdinData::IpcMessage& reply)
    {
        LOG4CXX_INFO(logger_, "Configuring XIDynPlugin plugin");
        LOG4CXX_INFO(logger_, "Config:" << config.encode());

        // Update the cached config with any new parameters
        if (!cached_config_) {
            cached_config_ = std::make_unique<OdinData::IpcMessage>(config.encode());
            LOG4CXX_DEBUG(logger_, "Initialized config cache");
        } else {
            // Merge incoming config into cached config
            cached_config_->update(config);
            LOG4CXX_DEBUG(logger_, "Updated config cache");
        }
        // Always ensure update_config is false in cache so mode changes trigger full recreation
        cached_config_->set_param("update_config", false);

        // Check for mode parameter
        bool mode_changed = false;
        if (config.has_param("mode")) {
            std::string mode_str = config.get_param<std::string>("mode");
            auto mode_it = XIDynUniversalDecoder::get_mode_string_map().find(mode_str);

            if (mode_it != XIDynUniversalDecoder::get_mode_string_map().end()) {
                XIDynMode requested_mode = mode_it->second;

                // Check if we need to change mode
                if (!decoder_initialized_ || requested_mode != current_mode_) {
                    LOG4CXX_INFO(logger_, "Setting decoder mode to: " << mode_str);

                    // Create new decoder with requested mode
                    decoder_ = std::make_unique<XIDynUniversalDecoder>(requested_mode);
                    current_mode_ = requested_mode;
                    decoder_initialized_ = true;
                    mode_changed = true;

                    LOG4CXX_INFO(logger_, "Decoder created with mode: " << mode_str);
                    LOG4CXX_DEBUG(logger_, "Packets per frame: " << decoder_->get_packets_per_frame());
                    LOG4CXX_DEBUG(logger_, "Payload size: " << decoder_->get_payload_size());
                } else {
                    // Mode specified but unchanged - skip passing to base class
                    LOG4CXX_DEBUG(logger_, "Mode unchanged: " << mode_str << " - skipping base class configure");
                    return;
                }
            } else {
                LOG4CXX_ERROR(logger_, "Invalid mode specified: " << mode_str);
                std::string error_msg = "Invalid mode: " + mode_str +
                    ". Valid modes: histogram_4096, histogram_2048, histogram_1024, " +
                    "histogram_512, histogram_256, raw_12bit, unpacked_12_16bit";
                reply.set_param("error", error_msg);
                return;
            }
        } else if (!decoder_initialized_) {
            // No mode specified and no decoder exists - use default
            LOG4CXX_INFO(logger_, "No mode specified, using default: unpacked_12_16bit");
            decoder_ = std::make_unique<XIDynUniversalDecoder>(XIDynMode::XIDYN_2X2_SINGLE_COLUMN);
            current_mode_ = XIDynMode::XIDYN_2X2_SINGLE_COLUMN;
            decoder_initialized_ = true;
        }

        // Set up frame callback and call base class configure
        FrameCallback frame_callback_ = boost::bind(&XIDynPlugin::process_frame,
                                                  this, boost::placeholders::_1);

        // If mode changed, use full cached config; otherwise pass through the incoming config
        if (mode_changed && cached_config_) {
            LOG4CXX_INFO(logger_, "Mode changed - using full cached configuration");
            LOG4CXX_INFO(logger_, "Cached config contents: " << cached_config_->encode());
            OdinData::IpcMessage full_config(cached_config_->encode());
            // Ensure update_config is false so DpdkCoreManager gets recreated
            full_config.set_param("update_config", false);
            DpdkFrameProcessorPlugin::configure(full_config, reply, decoder_.get(), frame_callback_);
        } else {
            DpdkFrameProcessorPlugin::configure(config, reply, decoder_.get(), frame_callback_);
        }
    }
    
    void XIDynPlugin::requestConfiguration(OdinData::IpcMessage& reply)
    {
        LOG4CXX_DEBUG(logger_, "Configuration requested for XIDynPlugin plugin");
        
        if (decoder_) {
            reply.set_param(get_name() + "/mode", decoder_->get_mode_string());
            reply.set_param(get_name() + "/packets_per_frame", 
                          static_cast<int>(decoder_->get_packets_per_frame()));
            reply.set_param(get_name() + "/payload_size", 
                          static_cast<int>(decoder_->get_payload_size()));
        }
        
        DpdkFrameProcessorPlugin::requestConfiguration(reply);
    }
    
    void XIDynPlugin::status(OdinData::IpcMessage& status)
    {
        LOG4CXX_DEBUG(logger_, "Status requested for XIDynPlugin plugin");
        
        // Report current mode and decoder configuration
        if (decoder_) {
            status.set_param(get_name() + "/mode", decoder_->get_mode_string());
            status.set_param(get_name() + "/packets_per_frame", 
                           static_cast<int>(decoder_->get_packets_per_frame()));
            status.set_param(get_name() + "/payload_size", 
                           static_cast<int>(decoder_->get_payload_size()));
            status.set_param(get_name() + "/bit_depth", decoder_->get_bit_depth_string());
            status.set_param(get_name() + "/decoder_initialized", decoder_initialized_);
        }

        // Add available modes as an array
        for (const auto& mode_pair : XIDynUniversalDecoder::get_mode_string_map()) {
            status.set_param(get_name() + "/available_modes[]", mode_pair.first);
        }
        
        DpdkFrameProcessorPlugin::status(status);
    }
    
    bool XIDynPlugin::reset_statistics(void)
    {
        LOG4CXX_INFO(logger_, "Statistics reset requested for XIDynPlugin plugin");
        bool reset_ok = DpdkFrameProcessorPlugin::reset_statistics();
        return reset_ok;
    }
    
    void XIDynPlugin::process_frame(boost::shared_ptr<Frame> frame)
    {
        this->push(frame);
    }
    
} /* namespace FrameProcessor */