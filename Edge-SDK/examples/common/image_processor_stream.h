/**
 ********************************************************************
 *
 * @copyright (c) 2023 DJI. All rights reserved.
 *
 * All information contained herein is, and remains, the property of DJI.
 * The intellectual and technical concepts contained herein are proprietary
 * to DJI and may be covered by U.S. and foreign patents, patents in process,
 * and protected by trade secret or copyright law.  Dissemination of this
 * information, including but not limited to data and other proprietary
 * material(s) incorporated within the information, in any form, is strictly
 * prohibited without the express written consent of DJI.
 *
 * If you receive this source code without DJI's authorization, you may not
 * further disseminate the information, and you must immediately remove the
 * source code and notify DJI of its removal. DJI reserves the right to pursue
 * legal actions against you for any loss(es) or damage(s) caused by your
 * failure to do so.
 *
 *********************************************************************
 */
#ifndef __IMAGE_PROCESSOR_STREAM_H__
#define __IMAGE_PROCESSOR_STREAM_H__

#include <memory>
#include <string>
#include <atomic>
#include <mutex>

#include "opencv2/opencv.hpp"
#include "image_processor.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

namespace edge_app {

class ImageStreamProcessor : public ImageProcessor {
   public:
    ImageStreamProcessor(const std::string& name, const std::string& stream_url);

    ~ImageStreamProcessor() override;

    int32_t Init() override;

    void Process(const std::shared_ptr<Image> image) override;

   private:
    int32_t InitEncoder(int width, int height);
    void CleanupEncoder();

    std::string name_;
    std::string stream_url_;
    
    // FFmpeg encoding context
    AVFormatContext* format_ctx_ = nullptr;
    AVCodecContext* codec_ctx_ = nullptr;
    AVStream* stream_ = nullptr;
    SwsContext* sws_ctx_ = nullptr;
    AVFrame* frame_ = nullptr;
    AVPacket* packet_ = nullptr;
    
    int frame_count_ = 0;
    int width_ = 0;
    int height_ = 0;
    std::atomic<bool> initialized_{false};
    std::mutex encoder_mutex_;
};

}  // namespace edge_app

#endif
