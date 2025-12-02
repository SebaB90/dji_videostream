/**
 * Pseudo dual-stream Wide + IR per Matrice 3TD (Dock 2)
 * Split screen in una finestra singola.
 * Switch ogni 10 secondi.
 */

#include <unistd.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <chrono>

#include "error_code.h"
#include "logger.h"
#include "sample_liveview.h"
#include "stream_decoder.h"
#include "image_processor.h"

#include <opencv2/opencv.hpp>

using namespace edge_sdk;
using namespace edge_app;

ErrorCode ESDKInit();

static std::atomic<int> g_current_lens{1};   // 1 = wide, 3 = IR
static std::atomic<bool> g_run{true};

// Ignora i frame subito dopo lo switch
static std::atomic<bool> g_accept_frames{false};
static const int IGNORE_FRAMES_AFTER_SWITCH = 12;

// ImageProcessor: unica finestra split
class WideIrProcessor : public ImageProcessor {
public:
    WideIrProcessor(const std::string& name) : name_(name) {}

    void Process(const std::shared_ptr<Image> image) override {
        if (!g_accept_frames.load()) return;

        auto mat_ptr = std::static_pointer_cast<cv::Mat>(image);
        if (!mat_ptr || mat_ptr->empty()) return;

        cv::Mat frame = *mat_ptr;
        int lens = g_current_lens.load();

        static cv::Mat last_wide, last_ir;
        static std::mutex m;

        {
            std::lock_guard<std::mutex> lk(m);

            if (lens == 1) {
                last_wide = frame.clone();
            } else if (lens == 3) {
                last_ir = frame.clone();
            }

            if (last_wide.empty() || last_ir.empty()) return;

            // IR → grayscale
            cv::Mat ir_gray;
            cv::cvtColor(last_ir, ir_gray, cv::COLOR_BGR2GRAY);
            cv::cvtColor(ir_gray, ir_gray, cv::COLOR_GRAY2BGR);

            // Resize
            cv::Size target(640, 480);
            cv::Mat wide_r, ir_r;
            cv::resize(last_wide, wide_r, target);
            cv::resize(ir_gray,  ir_r,    target);

            // Split view
            cv::Mat combined;
            cv::hconcat(wide_r, ir_r, combined);

            cv::imshow("Wide & IR Split View", combined);
        }

        cv::waitKey(1);
    }

private:
    std::string name_;
};

// Thread di switch Wide ↔ IR
void LensSwitchThread(std::shared_ptr<LiveviewSample> liveview,
                      int wide_ms = 10000,
                      int ir_ms   = 10000)
{
    INFO("Lens switch thread started: Wide %d ms, IR %d ms", wide_ms, ir_ms);

    while (g_run.load()) {

        // WIDE
        liveview->SetCameraSource((Liveview::CameraSource)1);
        g_current_lens.store(1);
        g_accept_frames.store(false);
        usleep(150000); // 150ms per evitare frame corrotti
        g_accept_frames.store(true);
        std::this_thread::sleep_for(std::chrono::milliseconds(wide_ms));

        // IR
        liveview->SetCameraSource((Liveview::CameraSource)3);
        g_current_lens.store(3);
        g_accept_frames.store(false);
        usleep(150000);
        g_accept_frames.store(true);
        std::this_thread::sleep_for(std::chrono::milliseconds(ir_ms));
    }

    INFO("Lens switch thread stopped");
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        ERROR("Usage: %s [QUALITY]", argv[0]);
        return -1;
    }

    auto rc = ESDKInit();
    if (rc != kOk) {
        ERROR("ESDK pre init failed");
        return -1;
    }

    int quality = atoi(argv[1]);
    INFO("Starting Wide + IR pseudo dual-stream, quality=%d", quality);

    auto liveview = std::make_shared<LiveviewSample>("WideIR");

    StreamDecoder::Options decoder_option = {.name = "ffmpeg"};
    auto decoder = CreateStreamDecoder(decoder_option);

    auto processor = std::make_shared<WideIrProcessor>("WideIrProcessor");

    if (0 != InitLiveviewSample(
            liveview,
            Liveview::kCameraTypePayload,
            (Liveview::StreamQuality)quality,
            decoder,
            processor)) {
        ERROR("Init liveview failed");
        return -1;
    }

    liveview->Start();

    // 🎯 Switch ogni 10 secondi
    std::thread switch_thread(LensSwitchThread, liveview, 10000, 10000);

    while (true) sleep(3);

    g_run.store(false);
    if (switch_thread.joinable()) switch_thread.join();

    return 0;
}
