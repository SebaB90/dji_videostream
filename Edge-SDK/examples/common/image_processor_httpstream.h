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
#ifndef __IMAGE_PROCESSOR_HTTPSTREAM_H__
#define __IMAGE_PROCESSOR_HTTPSTREAM_H__

#include <memory>
#include <string>
#include <atomic>
#include "opencv2/opencv.hpp"
#include "image_processor.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
}

namespace edge_app {

class ImageHttpStreamProcessor : public ImageProcessor {
   public:
    ImageHttpStreamProcessor(const std::string& name, const std::string& url);
    ~ImageHttpStreamProcessor() override;

    int32_t Init() override;
    void Process(const std::shared_ptr<Image> image) override;

   private:
    int32_t InitEncoder(int width, int height);
    void CleanupEncoder();

    std::string name_;
    std::string stream_url_;
    
    AVFormatContext* format_ctx_ = nullptr;
    AVCodecContext* codec_ctx_ = nullptr;
    AVStream* stream_ = nullptr;
    SwsContext* sws_ctx_ = nullptr;
    AVFrame* frame_ = nullptr;
    AVPacket* packet_ = nullptr;
    
    int64_t pts_ = 0;
    int current_width_ = 0;
    int current_height_ = 0;
    std::atomic<bool> initialized_;
};

}  // namespace edge_app

#endif  // __IMAGE_PROCESSOR_HTTPSTREAM_H__
