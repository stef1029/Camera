#include "Spinnaker.h"
#include "SpinGenApi/SpinnakerGenApi.h"
#include <opencv2/opencv.hpp>
#include "nlohmann/json.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <exception>
#include <memory>

namespace fs = std::filesystem;

using namespace Spinnaker;
using namespace Spinnaker::GenApi;
using namespace std;
using json = nlohmann::json;

int main(int argc, char** argv)
{
    // Check for proper usage
    if (argc < 3)
    {
        cout << "Usage: " << argv[0] << " <binary_file_path> <metadata_file_path> [output_video_path]" << endl;
        return -1;
    }

    // Parse command-line arguments
    string binaryFilePath = argv[1];
    string metadataFilePath = argv[2];
    string outputVideoPath;

    if (argc >= 4)
    {
        outputVideoPath = argv[3];
    }
    else
    {
        // Default output video path
        outputVideoPath = fs::path(binaryFilePath).replace_extension(".avi").string();
    }

    try
    {
        // Initialize Spinnaker system
        SystemPtr system = System::GetInstance();

        // Read metadata from JSON file
        ifstream metadataFile(metadataFilePath);
        if (!metadataFile.is_open())
        {
            cerr << "Error: Unable to open metadata file: " << metadataFilePath << endl;
            return -1;
        }

        json metadataJson;
        metadataFile >> metadataJson;
        metadataFile.close();

        // Extract metadata
        size_t imageWidth = metadataJson.at("image_width").get<size_t>();
        size_t imageHeight = metadataJson.at("image_height").get<size_t>();
        string pixelFormatStr = metadataJson.at("pixel_format").get<string>();
        double fps = metadataJson.at("frame_rate").get<double>();
        size_t totalFrames = metadataJson.at("frame_IDs").size();

        // Determine pixel format
        PixelFormatEnums pixelFormat;
        size_t imageSize;

        if (pixelFormatStr == "Mono8")
        {
            pixelFormat = PixelFormat_Mono8;
            imageSize = imageWidth * imageHeight;
        }
        else if (pixelFormatStr == "BayerRG8")
        {
            pixelFormat = PixelFormat_BayerRG8;
            imageSize = imageWidth * imageHeight;
        }
        else
        {
            cerr << "Error: Unsupported pixel format: " << pixelFormatStr << endl;
            return -1;
        }

        // Open binary file
        fstream rawFile(binaryFilePath, ios::in | ios::binary);
        if (!rawFile.is_open())
        {
            cerr << "Error: Could not open binary file: " << binaryFilePath << endl;
            return -1;
        }

        // Prepare OpenCV VideoWriter
        int fourcc = cv::VideoWriter::fourcc('M', 'J', 'P', 'G');
        cv::Size frameSize(static_cast<int>(imageWidth), static_cast<int>(imageHeight));

        bool isColor = (pixelFormat == PixelFormat_BayerRG8);
        cv::VideoWriter videoWriter(outputVideoPath, fourcc, fps, frameSize, isColor);
        if (!videoWriter.isOpened())
        {
            cerr << "Error: Could not open VideoWriter for output file: " << outputVideoPath << endl;
            return -1;
        }

        cout << "Processing binary video file..." << endl;
        cout << "Total frames: " << totalFrames << endl;

        // Move to the beginning of the file
        rawFile.seekg(0);

        for (size_t frameIndex = 0; frameIndex < totalFrames; ++frameIndex)
        {
            // Read image into buffer
            shared_ptr<char> pBuffer(new char[imageSize], default_delete<char[]>());

            rawFile.read(pBuffer.get(), imageSize);

            // Check if reading is successful
            if (!rawFile.good())
            {
                cerr << "Error reading image " << frameIndex << ". Aborting..." << endl;
                return -1;
            }

            // Create Spinnaker image from buffer
            ImagePtr pImage = Image::Create(
                imageWidth,
                imageHeight,
                0,
                0,
                pixelFormat,
                pBuffer.get());

            if (!pImage || !pImage->IsValid())
            {
                cerr << "Error: Invalid image at frame " << frameIndex << endl;
                continue;
            }

            // Convert image to Mono8 or BGR8 as needed
            if (pixelFormat == PixelFormat_Mono8)
            {
                // For Mono8, no conversion needed
            }
            else
            {
                cerr << "Error: Unsupported pixel format during processing." << endl;
                continue;
            }

            // Create OpenCV Mat from Spinnaker image
            cv::Mat image;
            if (pixelFormat == PixelFormat_Mono8)
            {
                image = cv::Mat(static_cast<int>(imageHeight), static_cast<int>(imageWidth), CV_8UC1, pImage->GetData(), pImage->GetStride());
            }
            else if (pixelFormat == PixelFormat_BayerRG8)
            {
                image = cv::Mat(static_cast<int>(imageHeight), static_cast<int>(imageWidth), CV_8UC3, pImage->GetData(), pImage->GetStride());
            }

            if (image.empty())
            {
                cerr << "Error: Empty image at frame " << frameIndex << endl;
                continue;
            }

            // Write frame to video
            videoWriter.write(image);

            // Progress indicator
            if (frameIndex % 100 == 0)
            {
                cout << "Processed frame " << frameIndex << " / " << totalFrames << endl;
            }
        }

        // Release resources
        videoWriter.release();
        rawFile.close();
        system->ReleaseInstance();

        cout << "Video conversion completed successfully. Output file: " << outputVideoPath << endl;
    }
    catch (const Spinnaker::Exception& e)
    {
        cerr << "Spinnaker Exception: " << e.what() << endl;
        return -1;
    }
    catch (const json::exception& e)
    {
        cerr << "JSON Exception: " << e.what() << endl;
        return -1;
    }
    catch (const std::exception& e)
    {
        cerr << "Standard Exception: " << e.what() << endl;
        return -1;
    }
    catch (...)
    {
        cerr << "Unknown Exception occurred." << endl;
        return -1;
    }

    return 0;
}
