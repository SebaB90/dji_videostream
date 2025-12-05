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
#include "image_processor_stream.h"

#include "logger.h"

namespace edge_app {

ImageStreamProcessor::ImageStreamProcessor(const std::string& name, const std::string& stream_url)
    : name_(name), stream_url_(stream_url) {
    INFO("Creating stream processor: %s -> %s", name_.c_str(), stream_url_.c_str());
}

ImageStreamProcessor::~ImageStreamProcessor() {
    CleanupEncoder();
}

int32_t ImageStreamProcessor::Init() {
    INFO("Stream processor Init called, will initialize encoder on first frame");
    return 0;
}

int32_t ImageStreamProcessor::InitEncoder(int width, int height) {
    std::lock_guard<std::mutex> lock(encoder_mutex_);
    
    if (initialized_) {
        return 0;
    }

    width_ = width;
    height_ = height;

    INFO("Initializing stream encoder: %dx%d -> %s", width, height, stream_url_.c_str());

    // Initialize FFmpeg
    avformat_network_init();

    // Determine format based on URL scheme
    const char* format_name = nullptr;
    if (stream_url_.find("rtsp://") == 0) {
        format_name = "rtsp";
    } else if (stream_url_.find("rtmp://") == 0) {
        format_name = "flv";
    } else if (stream_url_.find("http://") == 0 || stream_url_.find("https://") == 0) {
        // For HTTP(S) URLs, try RTSP as it's commonly used with MediaMTX/WHEP servers
        format_name = "rtsp";
    } else {
        ERROR("Unsupported URL scheme. Supported: rtsp://, rtmp://, http://");
        return -1;
    }

    // Allocate format context for output
    int ret = avformat_alloc_output_context2(&format_ctx_, nullptr, format_name, stream_url_.c_str());
    if (ret < 0 || !format_ctx_) {
        char errbuf[256];
        av_strerror(ret, errbuf, sizeof(errbuf));
        ERROR("Could not create output context for %s: %s", stream_url_.c_str(), errbuf);
        return -1;
    }

    // Find encoder
    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec) {
        ERROR("H264 encoder not found");
        return -1;
    }

    // Create stream
    stream_ = avformat_new_stream(format_ctx_, codec);
    if (!stream_) {
        ERROR("Failed to create stream");
        return -1;
    }

    // Allocate codec context
    codec_ctx_ = avcodec_alloc_context3(codec);
    if (!codec_ctx_) {
        ERROR("Failed to allocate codec context");
        return -1;
    }

    // Set codec parameters
    codec_ctx_->width = width;
    codec_ctx_->height = height;
    codec_ctx_->time_base = {1, 30};  // 30 fps
    codec_ctx_->framerate = {30, 1};
    codec_ctx_->pix_fmt = AV_PIX_FMT_YUV420P;
    codec_ctx_->bit_rate = 2000000;  // 2 Mbps
    codec_ctx_->gop_size = 30;
    codec_ctx_->max_b_frames = 0;

    // Set preset for low latency
    av_opt_set(codec_ctx_->priv_data, "preset", "ultrafast", 0);
    av_opt_set(codec_ctx_->priv_data, "tune", "zerolatency", 0);

    if (format_ctx_->oformat->flags & AVFMT_GLOBALHEADER) {
        codec_ctx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    // Open encoder
    ret = avcodec_open2(codec_ctx_, codec, nullptr);
    if (ret < 0) {
        char errbuf[256];
        av_strerror(ret, errbuf, sizeof(errbuf));
        ERROR("Failed to open encoder: %s", errbuf);
        return -1;
    }

    // Copy codec parameters to stream
    ret = avcodec_parameters_from_context(stream_->codecpar, codec_ctx_);
    if (ret < 0) {
        ERROR("Failed to copy codec parameters");
        return -1;
    }

    stream_->time_base = codec_ctx_->time_base;

    // Open output URL
    if (!(format_ctx_->oformat->flags & AVFMT_NOFILE)) {
        AVDictionary* opts = nullptr;
        av_dict_set(&opts, "rtsp_transport", "tcp", 0);
        
        ret = avio_open2(&format_ctx_->pb, stream_url_.c_str(), AVIO_FLAG_WRITE, nullptr, &opts);
        av_dict_free(&opts);
        
        if (ret < 0) {
            char errbuf[256];
            av_strerror(ret, errbuf, sizeof(errbuf));
            ERROR("Failed to open output URL %s: %s", stream_url_.c_str(), errbuf);
            return -1;
        }
    }

    // Write header
    AVDictionary* opts = nullptr;
    av_dict_set(&opts, "rtsp_transport", "tcp", 0);
    ret = avformat_write_header(format_ctx_, &opts);
    av_dict_free(&opts);
    
    if (ret < 0) {
        char errbuf[256];
        av_strerror(ret, errbuf, sizeof(errbuf));
        ERROR("Failed to write header: %s", errbuf);
        return -1;
    }

    // Allocate frame
    frame_ = av_frame_alloc();
    if (!frame_) {
        ERROR("Failed to allocate frame");
        return -1;
    }
    frame_->format = codec_ctx_->pix_fmt;
    frame_->width = width;
    frame_->height = height;
    
    ret = av_frame_get_buffer(frame_, 0);
    if (ret < 0) {
        ERROR("Failed to allocate frame buffer");
        return -1;
    }

    // Allocate packet
    packet_ = av_packet_alloc();
    if (!packet_) {
        ERROR("Failed to allocate packet");
        return -1;
    }

    // Create scaler context for BGR to YUV conversion
    sws_ctx_ = sws_getContext(
        width, height, AV_PIX_FMT_BGR24,
        width, height, AV_PIX_FMT_YUV420P,
        SWS_BILINEAR, nullptr, nullptr, nullptr);
    
    if (!sws_ctx_) {
        ERROR("Failed to create scaler context");
        return -1;
    }

    initialized_ = true;
    INFO("Stream encoder initialized successfully: %s", stream_url_.c_str());
    return 0;
}

void ImageStreamProcessor::CleanupEncoder() {
    std::lock_guard<std::mutex> lock(encoder_mutex_);
    
    if (!initialized_) {
        return;
    }

    INFO("Cleaning up stream encoder");

    if (format_ctx_) {
        av_write_trailer(format_ctx_);
        
        if (!(format_ctx_->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&format_ctx_->pb);
        }
        avformat_free_context(format_ctx_);
        format_ctx_ = nullptr;
    }

    if (codec_ctx_) {
        avcodec_free_context(&codec_ctx_);
        codec_ctx_ = nullptr;
    }

    if (frame_) {
        av_frame_free(&frame_);
        frame_ = nullptr;
    }

    if (packet_) {
        av_packet_free(&packet_);
        packet_ = nullptr;
    }

    if (sws_ctx_) {
        sws_freeContext(sws_ctx_);
        sws_ctx_ = nullptr;
    }

    initialized_ = false;
}

void ImageStreamProcessor::Process(const std::shared_ptr<Image> image) {
    if (!image || image->empty()) {
        return;
    }

    int width = image->cols;
    int height = image->rows;

    // Initialize encoder on first frame with actual dimensions
    if (!initialized_) {
        if (InitEncoder(width, height) < 0) {
            ERROR("Failed to initialize encoder, skipping frame");
            return;
        }
    }

    // Check if dimensions changed
    if (width != width_ || height != height_) {
        WARN("Frame dimensions changed from %dx%d to %dx%d, reinitializing encoder",
             width_, height_, width, height);
        CleanupEncoder();
        if (InitEncoder(width, height) < 0) {
            ERROR("Failed to reinitialize encoder");
            return;
        }
    }

    std::lock_guard<std::mutex> lock(encoder_mutex_);

    // Make frame writable
    int ret = av_frame_make_writable(frame_);
    if (ret < 0) {
        ERROR("Failed to make frame writable");
        return;
    }

    // Convert BGR to YUV420P
    const uint8_t* src_data[1] = {image->data};
    int src_linesize[1] = {static_cast<int>(image->step[0])};
    
    sws_scale(sws_ctx_, src_data, src_linesize, 0, height,
              frame_->data, frame_->linesize);

    frame_->pts = frame_count_++;

    // Encode frame
    ret = avcodec_send_frame(codec_ctx_, frame_);
    if (ret < 0) {
        char errbuf[256];
        av_strerror(ret, errbuf, sizeof(errbuf));
        ERROR("Error sending frame for encoding: %s", errbuf);
        return;
    }

    while (ret >= 0) {
        ret = avcodec_receive_packet(codec_ctx_, packet_);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            ERROR("Error receiving packet from encoder");
            break;
        }

        // Rescale packet timestamp
        av_packet_rescale_ts(packet_, codec_ctx_->time_base, stream_->time_base);
        packet_->stream_index = stream_->index;

        // Write packet
        ret = av_interleaved_write_frame(format_ctx_, packet_);
        if (ret < 0) {
            char errbuf[256];
            av_strerror(ret, errbuf, sizeof(errbuf));
            WARN("Error writing packet: %s", errbuf);
        }

        av_packet_unref(packet_);
    }
}

}  // namespace edge_app
