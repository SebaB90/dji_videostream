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
#ifndef __IMAGE_PROCESSOR_HTTP_STREAM_H__
#define __IMAGE_PROCESSOR_HTTP_STREAM_H__

#include <memory>
#include <string>
#include <atomic>

#include "opencv2/opencv.hpp"
#include "image_processor.h"
#include "liveview/sample_liveview.h"
#include "logger.h"

namespace edge_app {

class ImageHttpStreamProcessor : public ImageProcessor {
   public:
    ImageHttpStreamProcessor(const std::string& name, const std::string& stream_url, 
                             std::shared_ptr<void> userdata)
        : name_(name), stream_url_(stream_url), writer_initialized_(false) {
        if (userdata) {
            liveview_sample_ = std::static_pointer_cast<LiveviewSample>(userdata);
        }
        INFO("ImageHttpStreamProcessor created for URL: %s", stream_url_.c_str());
    }

    ~ImageHttpStreamProcessor() override {
        if (writer_.isOpened()) {
            writer_.release();
        }
    }

    int32_t Init() override {
        // Writer will be initialized on first frame when we know the frame size
        return 0;
    }

    void Process(const std::shared_ptr<Image> image) override {
        if (!image || image->empty()) {
            WARN("Received empty image");
            return;
        }

        // Initialize writer on first frame
        if (!writer_initialized_) {
            int width = image->cols;
            int height = image->rows;
            double fps = 30.0;
            
            // Build GStreamer pipeline for HTTP streaming via WHIP protocol
            // This is compatible with MediaMTX and similar servers
            std::string gst_pipeline = 
                "appsrc ! videoconvert ! video/x-raw,format=I420 ! "
                "x264enc tune=zerolatency bitrate=2000 speed-preset=ultrafast ! "
                "video/x-h264,profile=baseline ! "
                "flvmux streamable=true ! "
                "rtmpsink location=\"" + stream_url_ + "\"";
            
            // Try GStreamer pipeline first (better for HTTP/RTMP streaming)
            bool success = writer_.open(gst_pipeline, cv::CAP_GSTREAMER, 0, fps, 
                                         cv::Size(width, height), true);
            
            if (!success) {
                // Fallback: Try FFmpeg backend with RTMP format
                // Note: OpenCV VideoWriter with FFmpeg can output to RTMP URLs
                success = writer_.open(stream_url_, 
                                       cv::CAP_FFMPEG,
                                       cv::VideoWriter::fourcc('F', 'L', 'V', '1'),
                                       fps, 
                                       cv::Size(width, height), 
                                       true);
            }
            
            if (!success) {
                // Try with H264 codec
                success = writer_.open(stream_url_, 
                                       cv::CAP_FFMPEG,
                                       cv::VideoWriter::fourcc('a', 'v', 'c', '1'),
                                       fps, 
                                       cv::Size(width, height), 
                                       true);
            }
            
            if (!success) {
                // Try with X264 codec (lowercase)
                success = writer_.open(stream_url_, 
                                       cv::CAP_FFMPEG,
                                       cv::VideoWriter::fourcc('X', '2', '6', '4'),
                                       fps, 
                                       cv::Size(width, height), 
                                       true);
            }
            
            if (!success) {
                ERROR("Failed to open video writer for URL: %s (tried GStreamer and FFmpeg backends)", 
                      stream_url_.c_str());
                ERROR("Make sure the streaming server is running and accepts connections at this URL");
                return;
            }
            
            INFO("Video writer initialized: %dx%d @ %.1f fps -> %s", 
                 width, height, fps, stream_url_.c_str());
            writer_initialized_ = true;
        }

        // Add OSD information if available
        std::string h = std::to_string(image->size().width);
        std::string w = std::to_string(image->size().height);
        std::string osd = h + "x" + w;
        if (liveview_sample_) {
            auto kbps = liveview_sample_->GetStreamBitrate();
            osd += std::string(",") + std::to_string(kbps) + std::string("kbps");
        }
        
        // Create a copy of the image to add OSD
        cv::Mat output_frame = image->clone();
        cv::putText(output_frame, osd, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 1,
                    cv::Scalar(0, 0, 255), 3);

        // Write frame to stream
        if (writer_.isOpened()) {
            writer_.write(output_frame);
        }
    }

   private:
    std::string name_;
    std::string stream_url_;
    cv::VideoWriter writer_;
    std::atomic<bool> writer_initialized_;
    std::shared_ptr<LiveviewSample> liveview_sample_;
};

}  // namespace edge_app

#endif
