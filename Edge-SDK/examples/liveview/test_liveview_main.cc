// /**
//  ********************************************************************
//  *
//  * @copyright (c) 2023 DJI. All rights reserved.
//  *
//  * All information contained herein is, and remains, the property of DJI.
//  * The intellectual and technical concepts contained herein are proprietary
//  * to DJI and may be covered by U.S. and foreign patents, patents in process,
//  * and protected by trade secret or copyright law.  Dissemination of this
//  * information, including but not limited to data and other proprietary
//  * material(s) incorporated within the information, in any form, is strictly
//  * prohibited without the express written consent of DJI.
//  *
//  * If you receive this source code without DJI’s authorization, you may not
//  * further disseminate the information, and you must immediately remove the
//  * source code and notify DJI of its removal. DJI reserves the right to pursue
//  * legal actions against you for any loss(es) or damage(s) caused by your
//  * failure to do so.
//  *
//  *********************************************************************
//  */
// #include <unistd.h>

// #include "logger.h"
// #include "sample_liveview.h"

// using namespace edge_sdk;
// using namespace edge_app;

// ErrorCode ESDKInit();

// int main(int argc, char **argv) {
//     auto rc = ESDKInit();
//     if (rc != kOk) {
//         ERROR("pre init failed");
//         return -1;
//     }

//     int type = 0;
//     while (argc < 3 || (type = atoi(argv[1])) > 2) {
//         ERROR(
//             "Usage: %s [CAMERA_TYPE] [QUALITY] [SOURCE] \nDESCRIPTION:\n "
//             "CAMERA_TYPE: "
//             "0-FPV. 1-Payload \n QUALITY: 1-540p. 2-720p. 3-720pHigh. "
//             "4-1080p. 5-1080pHigh"
//             "\n SOURCE: 1-wide 2-zoom 3-IR \n eg: \n %s 1 3 1",
//             argv[0], argv[0]);
//         sleep(1);
//     }

//     auto quality = atoi(argv[2]);

//     const char type_to_str[2][16] = {"FPVCamera", "PayloadCamera"};

//     // create liveview sample
//     auto camera = std::string(type_to_str[type]);

//     auto liveview_sample = std::make_shared<LiveviewSample>(std::string(camera));

//     StreamDecoder::Options decoder_option = {.name = std::string("ffmpeg")};
//     auto stream_decoder = CreateStreamDecoder(decoder_option);

//     ImageProcessor::Options image_processor_option = {.name = std::string("display"),
//                                        .alias = camera, .userdata = liveview_sample};
//     auto image_processor = CreateImageProcessor(image_processor_option);

//     if (0 != InitLiveviewSample(
//         liveview_sample, (Liveview::CameraType)type, (Liveview::StreamQuality)quality,
//         stream_decoder, image_processor)) {
//         ERROR("Init %s liveview sample failed", camera.c_str());
//     } else {
//         liveview_sample->Start();
//         liveview_sample->
//     }

//     if (argc == 4) {
//         auto src = atoi(argv[3]);
//         INFO("set camera soure: %d", src);
//         liveview_sample->SetCameraSource((edge_sdk::Liveview::CameraSource)src);
//     }

//     while (1) sleep(3);

//     return 0;
// }


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
 * If you receive this source code without DJI’s authorization, you may not
 * further disseminate the information, and you must immediately remove the
 * source code and notify DJI of its removal. DJI reserves the right to pursue
 * legal actions against you for any loss(es) or damage(s) caused by your
 * failure to do so.
 *
 *********************************************************************
 */
#include <unistd.h>
#include <iostream>
#include <thread> // Required for multithreading
#include <cstring>

#include "logger.h"
#include "sample_liveview.h"

#include <iostream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

// Nome POSIX: deve iniziare con '/'
constexpr const char* SHM_NAME = "/my_shm";

using namespace edge_sdk;
using namespace edge_app;

ErrorCode ESDKInit();


#define LENS 1     // Wide lens by default

int video_type = 0;
int old_video_type = -1;
bool memory_initialized = false;

// Forward declaration of the liveview_sample shared pointer
std::shared_ptr<LiveviewSample> g_liveview_sample;

// --- New Thread Function for Input Monitoring ---
void input_monitor_thread() {

    while (true) {
        
        INFO("----------------------------------------------------------------------------------------------------------Waiting for lens change command in shared memory...");

        sleep(2); // Polling interval

        // Apri la shared memory creata da Python
        int fd = shm_open(SHM_NAME, O_RDONLY, 0666);
        if (fd == -1) {
            memory_initialized = false;
            ERROR("shm_not_initialized");
        }
        else {
            memory_initialized = true;
        }

        if (memory_initialized) {
            // Mappiamo 4 byte (1 int)
            void* addr = mmap(nullptr, sizeof(int), PROT_READ, MAP_SHARED, fd, 0);
            if (addr == MAP_FAILED) {
                ERROR("address_mapping_failed");
                close(fd);
            }

            // Cast e lettura del valore
            old_video_type = video_type;
            video_type = *reinterpret_cast<int*>(addr);
                
            if (old_video_type != video_type) {
                INFO("Detected lens change request: %d", video_type);
                g_liveview_sample->SetCameraSource((edge_sdk::Liveview::CameraSource)video_type);
            }

            if ( video_type != 1 | video_type != 2 | video_type != 3) {
                INFO("Error!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!.");
                
            }
        }

        
    }
}
// --------------------------------------------------

int main(int argc, char **argv) {
    auto rc = ESDKInit();
    if (rc != kOk) {
        ERROR("pre init failed");
        return -1;
    }

    int type = 0;
    int quality = 0;
    int source = 0;
    std::string stream_url = "";

    // Check for --stream-url option
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--stream-url") == 0 && i + 1 < argc) {
            stream_url = argv[i + 1];
            // Shift remaining arguments
            for (int j = i; j < argc - 2; j++) {
                argv[j] = argv[j + 2];
            }
            argc -= 2;
            break;
        }
    }

    // --- Input Validation Loop (Same as previous solution) ---
    while (argc < 3 || (type = atoi(argv[1])) > 1 || (quality = atoi(argv[2])) > 5 ||
           (argc == 4 && ((source = atoi(argv[3])) < 1 || source > 3))) {
        ERROR(
            "Usage: %s [CAMERA_TYPE] [QUALITY] [LENS] [--stream-url URL]\nDESCRIPTION:\n "
            "CAMERA_TYPE: "
            "0-FPV. 1-Payload \n QUALITY: 1-540p. 2-720p. 3-720pHigh. "
            "4-1080p. 5-1080pHigh"
            "\n LENS (Optional): 1-wide 2-zoom 3-IR"
            "\n --stream-url (Optional): RTSP/RTMP URL to stream video (e.g., rtsp://localhost:8554/drone)"
            "\n eg: \n %s 1 4 2 --stream-url rtsp://localhost:8554/drone (Payload, 1080p, Zoom, stream to URL)",
            argv[0], argv[0]);
        sleep(1);
        
        if (argc < 3 || type > 1 || quality > 5 || (argc == 4 && (source < 1 || source > 3))) {
            return -1;
        }
    }

    const char type_to_str[2][16] = {"FPVCamera", "PayloadCamera"};

    // create liveview sample, assign to global pointer
    auto camera = std::string(type_to_str[type]);
    g_liveview_sample = std::make_shared<LiveviewSample>(std::string(camera)); // Use the global pointer

    StreamDecoder::Options decoder_option = {.name = std::string("ffmpeg")};
    auto stream_decoder = CreateStreamDecoder(decoder_option);

    // Create image processor based on whether streaming URL is provided
    std::shared_ptr<ImageProcessor> image_processor;
    if (!stream_url.empty()) {
        INFO("Streaming video to: %s", stream_url.c_str());
        ImageProcessor::Options image_processor_option = {
            .name = std::string("stream"),
            .alias = camera,
            .userdata = nullptr,
            .stream_url = stream_url
        };
        image_processor = CreateImageProcessor(image_processor_option);
    } else {
        ImageProcessor::Options image_processor_option = {
            .name = std::string("display"),
            .alias = camera,
            .userdata = g_liveview_sample
        };
        image_processor = CreateImageProcessor(image_processor_option);
    }

    if (0 != InitLiveviewSample(
        g_liveview_sample, (Liveview::CameraType)type, (Liveview::StreamQuality)quality,
        stream_decoder, image_processor)) {
        ERROR("Init %s liveview sample failed", camera.c_str());
    } else {
        g_liveview_sample->Start();
    }

    // Set initial lens if provided
    if (argc == 4) {
        INFO("Setting initial camera source (lens): %d", source);
        g_liveview_sample->SetCameraSource((edge_sdk::Liveview::CameraSource)source);
    }

    // --- Start the new input monitoring thread ---
    std::thread input_thread(input_monitor_thread);

    // Main thread keeps running to prevent the program from exiting
    // and keeps the liveview active.
    while (1) sleep(3);

    // Clean up (though unreachable in an infinite loop, good practice)
    input_thread.join();
    return 0;
}