/*
 * XIDynPlugin.h
 *
 *  Created on: 12 September 2024
 *      Author: Dominic Banks, STFC Detector Systems Software Group
 */


#ifndef INCLUDE_XIDYNPLUGIN_H_
#define INCLUDE_XIDYNPLUGIN_H_

#include <string>
#include <memory>
#include <log4cxx/logger.h>
#include <log4cxx/basicconfigurator.h>
#include <log4cxx/propertyconfigurator.h>
#include <log4cxx/helpers/exception.h>
using namespace log4cxx;
using namespace log4cxx::helpers;

#include <DpdkFrameProcessorPlugin.h>
#include "XIDynUniversalDecoder.h"
#include "ClassLoader.h"


  /** Detector Plugin
   *
   * The XIDynPlugin class implements a DPDK-aware plugin capable of receiving data
   * frame packets from upstream DPDK packet processing cores and injecting them into the
   * frameProcessor frame data flow.
   */
  
namespace FrameProcessor
{
    class XIDynPlugin : public DpdkFrameProcessorPlugin
    {
    public:
        XIDynPlugin();
        virtual ~XIDynPlugin();
        
        void configure(OdinData::IpcMessage& config, OdinData::IpcMessage& reply);
        void requestConfiguration(OdinData::IpcMessage& reply);
        void status(OdinData::IpcMessage& status);
        bool reset_statistics(void);
        void process_frame(boost::shared_ptr<Frame> frame);
        
    private:
        LoggerPtr logger_;
        XIDynMode current_mode_;                           //!< Current decoder mode
        std::unique_ptr<XIDynUniversalDecoder> decoder_;   //!< Decoder instance
        bool decoder_initialized_;                               //!< Flag to track if decoder has been created
        FrameCallback frame_callback_;
        // Configuration storage
        std::unique_ptr<OdinData::IpcMessage> cached_config_;
    };
    
    REGISTER(FrameProcessorPlugin, XIDynPlugin, "XIDynPlugin");
    
} /* namespace FrameProcessor */

#endif /* INCLUDE_XIDYNPLUGIN_H_ */