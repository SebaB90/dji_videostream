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
#include "image_processor_httpstream.h"
#include "logger.h"

namespace edge_app {

ImageHttpStreamProcessor::ImageHttpStreamProcessor(const std::string& name, const std::string& url)
    : name_(name), stream_url_(url), initialized_(false) {
    INFO("ImageHttpStreamProcessor created for URL: %s", url.c_str());
}

ImageHttpStreamProcessor::~ImageHttpStreamProcessor() {
    CleanupEncoder();
}

int32_t ImageHttpStreamProcessor::Init() {
    INFO("ImageHttpStreamProcessor Init called");
    return 0;
}

int32_t ImageHttpStreamProcessor::InitEncoder(int width, int height) {
    int ret;
    
    INFO("Initializing HTTP stream encoder for %dx%d to %s", width, height, stream_url_.c_str());
    
    // Register all formats and codecs (for FFmpeg 3.x/4.x compatibility)
    av_register_all();
    avformat_network_init();
    
    // Allocate format context for MPEGTS over HTTP
    ret = avformat_alloc_output_context2(&format_ctx_, nullptr, "mpegts", stream_url_.c_str());
    if (ret < 0 || !format_ctx_) {
        char errbuf[128];
        av_strerror(ret, errbuf, sizeof(errbuf));
        ERROR("Could not create output context: %s", errbuf);
        return -1;
    }
    
    // Find the H264 encoder
    AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec) {
        ERROR("H264 encoder not found");
        return -1;
    }
    
    // Create stream
    stream_ = avformat_new_stream(format_ctx_, codec);
    if (!stream_) {
        ERROR("Could not create stream");
        return -1;
    }
    
    // Allocate codec context
    codec_ctx_ = avcodec_alloc_context3(codec);
    if (!codec_ctx_) {
        ERROR("Could not allocate codec context");
        return -1;
    }
    
    // Set codec parameters
    codec_ctx_->width = width;
    codec_ctx_->height = height;
    codec_ctx_->time_base = {1, 30};  // 30 fps
    codec_ctx_->framerate = {30, 1};
    codec_ctx_->gop_size = 30;
    codec_ctx_->max_b_frames = 0;
    codec_ctx_->pix_fmt = AV_PIX_FMT_YUV420P;
    codec_ctx_->bit_rate = 2000000;  // 2 Mbps
    
    // Set preset for fast encoding
    av_opt_set(codec_ctx_->priv_data, "preset", "ultrafast", 0);
    av_opt_set(codec_ctx_->priv_data, "tune", "zerolatency", 0);
    
    if (format_ctx_->oformat->flags & AVFMT_GLOBALHEADER) {
        codec_ctx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
    
    // Open codec
    ret = avcodec_open2(codec_ctx_, codec, nullptr);
    if (ret < 0) {
        char errbuf[128];
        av_strerror(ret, errbuf, sizeof(errbuf));
        ERROR("Could not open codec: %s", errbuf);
        return -1;
    }
    
    // Copy codec parameters to stream
    ret = avcodec_parameters_from_context(stream_->codecpar, codec_ctx_);
    if (ret < 0) {
        ERROR("Could not copy codec parameters");
        return -1;
    }
    
    stream_->time_base = codec_ctx_->time_base;
    
    // Open output
    if (!(format_ctx_->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&format_ctx_->pb, stream_url_.c_str(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            char errbuf[128];
            av_strerror(ret, errbuf, sizeof(errbuf));
            ERROR("Could not open output URL %s: %s", stream_url_.c_str(), errbuf);
            return -1;
        }
    }
    
    // Write header
    ret = avformat_write_header(format_ctx_, nullptr);
    if (ret < 0) {
        char errbuf[128];
        av_strerror(ret, errbuf, sizeof(errbuf));
        ERROR("Could not write header: %s", errbuf);
        return -1;
    }
    
    // Allocate frame
    frame_ = av_frame_alloc();
    if (!frame_) {
        ERROR("Could not allocate frame");
        return -1;
    }
    
    frame_->format = codec_ctx_->pix_fmt;
    frame_->width = width;
    frame_->height = height;
    
    ret = av_frame_get_buffer(frame_, 0);
    if (ret < 0) {
        ERROR("Could not allocate frame buffer");
        return -1;
    }
    
    // Allocate packet
    packet_ = av_packet_alloc();
    if (!packet_) {
        ERROR("Could not allocate packet");
        return -1;
    }
    
    // Initialize color space converter (BGR to YUV420P)
    sws_ctx_ = sws_getContext(width, height, AV_PIX_FMT_BGR24,
                               width, height, AV_PIX_FMT_YUV420P,
                               SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
    if (!sws_ctx_) {
        ERROR("Could not initialize color converter");
        return -1;
    }
    
    current_width_ = width;
    current_height_ = height;
    pts_ = 0;
    initialized_ = true;
    
    INFO("HTTP stream encoder initialized successfully");
    return 0;
}

void ImageHttpStreamProcessor::CleanupEncoder() {
    initialized_ = false;
    
    if (format_ctx_ && format_ctx_->pb) {
        av_write_trailer(format_ctx_);
    }
    
    if (sws_ctx_) {
        sws_freeContext(sws_ctx_);
        sws_ctx_ = nullptr;
    }
    
    if (packet_) {
        av_packet_free(&packet_);
        packet_ = nullptr;
    }
    
    if (frame_) {
        av_frame_free(&frame_);
        frame_ = nullptr;
    }
    
    if (codec_ctx_) {
        avcodec_free_context(&codec_ctx_);
        codec_ctx_ = nullptr;
    }
    
    if (format_ctx_) {
        if (!(format_ctx_->oformat->flags & AVFMT_NOFILE) && format_ctx_->pb) {
            avio_closep(&format_ctx_->pb);
        }
        avformat_free_context(format_ctx_);
        format_ctx_ = nullptr;
    }
    
    stream_ = nullptr;
    current_width_ = 0;
    current_height_ = 0;
}

void ImageHttpStreamProcessor::Process(const std::shared_ptr<Image> image) {
    if (!image || image->empty()) {
        return;
    }
    
    int width = image->cols;
    int height = image->rows;
    
    // Initialize encoder if not done or if resolution changed
    if (!initialized_ || width != current_width_ || height != current_height_) {
        CleanupEncoder();
        if (InitEncoder(width, height) != 0) {
            ERROR("Failed to initialize encoder");
            return;
        }
    }
    
    // Make frame writable
    int ret = av_frame_make_writable(frame_);
    if (ret < 0) {
        ERROR("Could not make frame writable");
        return;
    }
    
    // Convert BGR to YUV420P
    const uint8_t* src_data[1] = { image->data };
    int src_linesize[1] = { static_cast<int>(image->step[0]) };
    
    sws_scale(sws_ctx_, src_data, src_linesize, 0, height,
              frame_->data, frame_->linesize);
    
    frame_->pts = pts_++;
    
    // Encode frame
    ret = avcodec_send_frame(codec_ctx_, frame_);
    if (ret < 0) {
        char errbuf[128];
        av_strerror(ret, errbuf, sizeof(errbuf));
        ERROR("Error sending frame for encoding: %s", errbuf);
        return;
    }
    
    while (ret >= 0) {
        ret = avcodec_receive_packet(codec_ctx_, packet_);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            ERROR("Error during encoding");
            break;
        }
        
        // Rescale packet timestamp
        av_packet_rescale_ts(packet_, codec_ctx_->time_base, stream_->time_base);
        packet_->stream_index = stream_->index;
        
        // Write packet
        ret = av_interleaved_write_frame(format_ctx_, packet_);
        if (ret < 0) {
            char errbuf[128];
            av_strerror(ret, errbuf, sizeof(errbuf));
            ERROR("Error writing frame: %s", errbuf);
        }
        
        av_packet_unref(packet_);
    }
}

}  // namespace edge_app
