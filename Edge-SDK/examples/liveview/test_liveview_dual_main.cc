#include <unistd.h>
#include <memory>

#include "logger.h"
#include "sample_liveview.h"

using namespace edge_sdk;
using namespace edge_app;

ErrorCode ESDKInit();

int main(int argc, char **argv) {

    if (argc < 3) {
        ERROR("Usage: %s [ZOOM_QUALITY] [IR_QUALITY]", argv[0]);
        return -1;
    }

    if (ESDKInit() != kOk) {
        ERROR("ESDK init failed");
        return -1;
    }

    int zoom_quality = atoi(argv[1]);
    int ir_quality   = atoi(argv[2]);

    INFO("Starting dual liveview: Zoom + IR");


    /*********************************************************
     *  ZOOM STREAM
     *********************************************************/
    auto zoom_sample = std::make_shared<LiveviewSample>("ZoomView");

    StreamDecoder::Options zoom_dec_opt = {.name = "ffmpeg"};
    auto zoom_decoder = CreateStreamDecoder(zoom_dec_opt);

    ImageProcessor::Options zoom_proc_opt = {
        .name = "display",
        .alias = "ZoomView",      // <-- UNICO e NON “PayloadCamera”
        .userdata = zoom_sample
    };

    auto zoom_processor = CreateImageProcessor(zoom_proc_opt);

    if (0 != InitLiveviewSample(
        zoom_sample,
        Liveview::kCameraTypePayload,
        (Liveview::StreamQuality)zoom_quality,
        zoom_decoder,
        zoom_processor))
    {
        ERROR("Zoom stream init failed");
    } else {
        zoom_sample->Start();
        zoom_sample->SetCameraSource((Liveview::CameraSource)2);   // 2 = zoom
    }


    /*********************************************************
     *  IR STREAM
     *********************************************************/
    auto ir_sample = std::make_shared<LiveviewSample>("IRView");

    StreamDecoder::Options ir_dec_opt = {.name = "ffmpeg"};
    auto ir_decoder = CreateStreamDecoder(ir_dec_opt);

    ImageProcessor::Options ir_proc_opt = {
        .name = "display1",
        .alias = "IRView",        // <-- UNICO e NON “PayloadCamera”
        .userdata = ir_sample
    };

    auto ir_processor = CreateImageProcessor(ir_proc_opt);

    if (0 != InitLiveviewSample(
        ir_sample,
        Liveview::kCameraTypePayload,
        (Liveview::StreamQuality)ir_quality,
        ir_decoder,
        ir_processor))
    {
        ERROR("IR stream init failed");
    } else {
        ir_sample->Start();
        ir_sample->SetCameraSource((Liveview::CameraSource)3);     // 3 = IR
    }


    /*********************************************************
     * LOOP
     *********************************************************/
    while (true) sleep(3);

    return 0;
}
