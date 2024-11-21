#include "Spinnaker.h"
#include "SpinGenApi/SpinnakerGenApi.h"
#include <iostream>
#include <opencv2/opencv.hpp>
#include <chrono>
#include <sstream>
#include <fstream>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <cstdlib>
#include "nlohmann/json.hpp"  // Include the nlohmann/json library
#include <direct.h>  // Include for _mkdir on Windows
#include <queue>
#include <filesystem>

using namespace Spinnaker;
using namespace Spinnaker::GenApi;
using namespace Spinnaker::GenICam;
using namespace std;
using namespace std::chrono;
using json = nlohmann::json;
namespace fs = std::filesystem;


#include "Spinnaker.h"
#include "SpinGenApi/SpinnakerGenApi.h"
#include <iostream>
#include <opencv2/opencv.hpp>
#include <chrono>
#include <sstream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdlib>
#include "nlohmann/json.hpp"  // Include the nlohmann/json library
#include <direct.h>           // Include for _mkdir on Windows
#include <filesystem>

using namespace Spinnaker;
using namespace Spinnaker::GenApi;
using namespace Spinnaker::GenICam;
using namespace std;
using namespace std::chrono;
using json = nlohmann::json;
namespace fs = std::filesystem;

class Tracker
{
public:
    // Constructor
    Tracker(const string& mouse_ID, const string& start_time, const string& path, int cam_no, float FPS, int windowWidth, int windowHeight)
        : mouse_ID(mouse_ID), start_time(start_time), path(path), cam_no(cam_no), FPS(FPS), windowWidth(windowWidth), windowHeight(windowHeight), frame_count(0)
    {
        system = System::GetInstance();
        CameraList camList = system->GetCameras();

        // Set serial number based on cam_no
        if (cam_no == 1) {
            camSerial = "22181614";
            max_FPS = 170.0;
        }
        else if (cam_no == 2)
        {
            camSerial = "20530175";
            max_FPS = 170.0;
        }
        else if (cam_no == 3)
        {
            camSerial = "24174008";
            max_FPS = 170.0;
        }
        else if (cam_no == 4)
        {
            camSerial = "24174020";
            max_FPS = 170.0;
        }
        else if (cam_no == 5)   // colour camera
        {
            camSerial = "23606054";
            max_FPS = 170.0;
        }
        else if (cam_no == 6)   // 6.3MP camera
        {
            camSerial = "21423798";
            max_FPS = 59.60;
        }
        else
        {
            throw runtime_error("Invalid camera number");
        }

        // Limit FPS to max amount of camera
        if (FPS > max_FPS) {
            FPS = max_FPS;
        }

        windowTitle << "Rig " << cam_no << ". Press 'Esc' to stop session.";
        title = windowTitle.str();

        // Use GetBySerial to get the camera
        pCam = camList.GetBySerial(camSerial);

        if (!pCam)
        {
            cerr << "Error: Camera can't open\nexit" << endl;
            throw runtime_error("Camera can't open");
        }

        pCam->Init();
        setCameraFrameRate(FPS);  // Set the frame rate
        setGPIOLine2ToOutput();   // Set GPIO Line 2 to output
        // Set the exposure time lower limit here (us)
        setExposureTimeLowerLimit(4000.0);
        INodeMap& nodeMap = pCam->GetNodeMap();

        // Set acquisition mode to continuous
        CEnumerationPtr ptrAcquisitionMode = nodeMap.GetNode("AcquisitionMode");
        if (!IsReadable(ptrAcquisitionMode) || !IsWritable(ptrAcquisitionMode))
        {
            cerr << "Error: Unable to set acquisition mode to continuous (enum retrieval). Aborting..." << endl << endl;
            throw runtime_error("Unable to set acquisition mode to continuous");
        }

        CEnumEntryPtr ptrAcquisitionModeContinuous = ptrAcquisitionMode->GetEntryByName("Continuous");
        if (!IsReadable(ptrAcquisitionModeContinuous))
        {
            cerr << "Error: Unable to get or set acquisition mode to continuous (entry retrieval). Aborting..." << endl << endl;
            throw runtime_error("Unable to set acquisition mode to continuous");
        }
        

        const int64_t acquisitionModeContinuous = ptrAcquisitionModeContinuous->GetValue();
        ptrAcquisitionMode->SetIntValue(acquisitionModeContinuous);



        pCam->BeginAcquisition();

        dims = make_pair(pCam->Width.GetValue(), pCam->Height.GetValue());
    }

    // Destructor
    ~Tracker()
    {
        if (pCam)
        {
            pCam->EndAcquisition();
            pCam->DeInit();
            pCam = nullptr;
        }
        system->ReleaseInstance();
    }

    void startTracking(bool show_frame, bool save_video)
    {
        frame_IDs.clear();
        timer_start_time = high_resolution_clock::now();
        saveData();

        // Start the single-threaded capture loop
        captureFrames(show_frame, save_video);

        end_time = currentDateTime();
        saveData();

        // Create the signal file to indicate that tracking has finished
        createSignalFile();
    }

private:
    string mouse_ID;
    string start_time;
    string end_time;
    string path;
    int cam_no;
    float FPS;
    size_t frame_count;
    string camSerial;
    float max_FPS;
    CameraPtr pCam;
    SystemPtr system;
    pair<int, int> dims;
    vector<uint64_t> frame_IDs;
    vector<uint64_t> frame_IDs_mem;
    high_resolution_clock::time_point timer_start_time;
    ostringstream windowTitle;
    string title;
    int windowWidth;
    int windowHeight;

    const size_t bufferSize = 200;

    void captureFrames(bool show_frame, bool save_video)
    {
        auto prev = high_resolution_clock::now();
        int displayFPS = 15;  // Maximum display FPS
        int frame_skip = int(1000 / displayFPS);  // Frame skip duration in milliseconds

        // Open the frame ID file in append mode
        std::ofstream frameIDFile(path + "/frame_ids_backup.txt", std::ios_base::app);
        if (!frameIDFile.is_open())
        {
            std::cerr << "Error: Could not open frame ID file for writing." << std::endl;
            return;
        }

        bool keepRunning = true;

        while (keepRunning)
        {
            // Capture frame from the camera
            ImagePtr pResultImage = pCam->GetNextImage(1000);

            if (pResultImage->IsIncomplete())
            {
                cerr << "Error: Image incomplete: " << Image::GetImageStatusDescription(pResultImage->GetImageStatus()) << endl << endl;
                pResultImage->Release(); // Release incomplete image
                continue;
            }

            if (save_video)
            {
                stringstream filename;
                filename << path << "/raw_temp" << setw(8) << setfill('0') << frame_count << ".bmp";
                pResultImage->Save(filename.str().c_str(), Spinnaker::ImageFileFormat::SPINNAKER_IMAGE_FILE_FORMAT_BMP);

                // Check if the file exists
                if (std::filesystem::exists(filename.str()))
                {
                    // File exists, add frame ID to the list
                    uint64_t frameID = pResultImage->GetFrameID();  // Grab the frame ID once
                    frame_IDs.push_back(frameID);                   // Save to frame_IDs
                    frame_IDs_mem.push_back(frameID);               // Save to frame_IDs_mem

                    // Check if we need to flush the buffer
                    if (frame_IDs.size() >= bufferSize)
                    {
                        // Write buffer to file
                        for (const auto& frameID : frame_IDs)
                        {
                            frameIDFile << frameID << std::endl;
                        }
                        frameIDFile.flush();
                        frame_IDs.clear();
                    }
                }
                else
                {
                    // File does not exist, handle the error
                    std::cerr << "Failed to save image: " << filename.str() << std::endl;
                    // Optionally, implement error handling or retry logic here
                }
            }

            // Display frames at the specified display FPS (15 FPS)
            if (show_frame)
            {
                auto now = high_resolution_clock::now();
                double elapsedTime = duration_cast<milliseconds>(now - prev).count();

                if (elapsedTime >= frame_skip)
                {
                    cv::Mat image(cv::Size(dims.first, dims.second), CV_8UC1, pResultImage->GetData(), pResultImage->GetStride());

                    cv::Mat resizedImage;
                    cv::resize(image, resizedImage, cv::Size(windowWidth, windowHeight));

                    cv::imshow(title, resizedImage);

                    if (cv::waitKey(1) == 27)
                    {
                        keepRunning = false;
                        break;
                    }

                    prev = now;  // Reset the previous time for the next displayed frame
                }
            }

            // Release the captured image
            pResultImage->Release();
            frame_count++;
        }

        // After the loop, flush any remaining frame IDs in the buffer
        if (!frame_IDs.empty())
        {
            for (const auto& frameID : frame_IDs)
            {
                frameIDFile << frameID << std::endl;
            }
            frameIDFile.flush();
            frame_IDs.clear();
        }

        frameIDFile.close();
        cv::destroyAllWindows();
    }

    void saveData()
    {
        string file_name = start_time + "_Tracker_data.json";
        json data;

        data["frame_rate"] = FPS;
        data["start_time"] = start_time;
        data["end_time"] = end_time;
        data["height"] = dims.second;
        data["width"] = dims.first;
        data["frame_IDs"] = frame_IDs_mem;

        ofstream file(path + "/" + file_name);
        file << data.dump(4); // Pretty print with 4 spaces
        file.close();
    }

    string currentDateTime()
    {
        auto now = system_clock::now();
        time_t now_time = system_clock::to_time_t(now);
        char buffer[80];
        tm localTime;
        localtime_s(&localTime, &now_time);
        strftime(buffer, sizeof(buffer), "%y%m%d_%H%M%S", &localTime);
        return string(buffer);
    }

    void setCameraFrameRate(double frameRate)
    {
        INodeMap& nodeMap = pCam->GetNodeMap();
        CBooleanPtr ptrFrameRateEnable = nodeMap.GetNode("AcquisitionFrameRateEnable");
        if (IsWritable(ptrFrameRateEnable))
        {
            ptrFrameRateEnable->SetValue(true);
        }
        else
        {
            throw runtime_error("Unable to enable frame rate");
        }

        CFloatPtr ptrFrameRate = nodeMap.GetNode("AcquisitionFrameRate");
        if (IsWritable(ptrFrameRate))
        {
            ptrFrameRate->SetValue(frameRate);
        }
        else
        {
            throw runtime_error("Unable to set frame rate");
        }
    }

    void setGPIOLine2ToOutput()
    {
        INodeMap& nodeMap = pCam->GetNodeMap();

        // Select Line 2
        CEnumerationPtr ptrLineSelector = nodeMap.GetNode("LineSelector");
        if (IsWritable(ptrLineSelector))
        {
            CEnumEntryPtr ptrLine2 = ptrLineSelector->GetEntryByName("Line2");
            if (IsReadable(ptrLine2))
            {
                ptrLineSelector->SetIntValue(ptrLine2->GetValue());
            }
            else
            {
                throw runtime_error("Unable to select Line 2");
            }
        }
        else
        {
            throw runtime_error("Unable to access LineSelector");
        }

        // Set Line Mode to Output
        CEnumerationPtr ptrLineMode = nodeMap.GetNode("LineMode");
        if (IsWritable(ptrLineMode))
        {
            CEnumEntryPtr ptrOutput = ptrLineMode->GetEntryByName("Output");
            if (IsReadable(ptrOutput))
            {
                ptrLineMode->SetIntValue(ptrOutput->GetValue());
            }
            else
            {
                throw runtime_error("Unable to set line mode to output");
            }
        }
        else
        {
            throw runtime_error("Unable to access LineMode");
        }
    }

    void createSignalFile()
    {
        // Get the parent directory of the specified path
        // fs::path parent_dir = fs::path(path).parent_path();

        // Create the signal file in the parent directory
        string signal_file = fs::path(path).string() + "/rig_" + to_string(cam_no) + "_camera_finished.signal";
        ofstream file(signal_file);
        file.close();
    }

    void setExposureTimeLowerLimit(double exposureTimeLowerLimit)
    {
        INodeMap& nodeMap = pCam->GetNodeMap();

        // Set ExposureAuto to Continuous
        CEnumerationPtr ptrExposureAuto = nodeMap.GetNode("ExposureAuto");
        if (IsWritable(ptrExposureAuto))
        {
            CEnumEntryPtr ptrExposureAutoContinuous = ptrExposureAuto->GetEntryByName("Continuous");
            if (IsReadable(ptrExposureAutoContinuous))
            {
                ptrExposureAuto->SetIntValue(ptrExposureAutoContinuous->GetValue());
            }
            else
            {
                throw runtime_error("Unable to set ExposureAuto to Continuous");
            }
        }
        else
        {
            throw runtime_error("Unable to access ExposureAuto");
        }

        // Set AutoExposureExposureTimeLowerLimit
        CFloatPtr ptrExposureTimeLowerLimit = nodeMap.GetNode("AutoExposureExposureTimeLowerLimit");
        if (!IsAvailable(ptrExposureTimeLowerLimit) || !IsWritable(ptrExposureTimeLowerLimit))
        {
            throw runtime_error("Unable to access AutoExposureExposureTimeLowerLimit");
        }

        double minExposureTimeLowerLimit = ptrExposureTimeLowerLimit->GetMin();
        double maxExposureTimeLowerLimit = ptrExposureTimeLowerLimit->GetMax();

        if (exposureTimeLowerLimit < minExposureTimeLowerLimit)
            exposureTimeLowerLimit = minExposureTimeLowerLimit;
        else if (exposureTimeLowerLimit > maxExposureTimeLowerLimit)
            exposureTimeLowerLimit = maxExposureTimeLowerLimit;

        ptrExposureTimeLowerLimit->SetValue(exposureTimeLowerLimit);
    }


};


// //argc is argument count, argv is the vector of c-strings 
int main(int argc, char** argv)
{
    string mouse_ID = "NoID";
    string date_time = "";
    string path = "";
    int cam = 2;
    float FPS = 60.0f;
    int windowWidth = 800;  // Default window width
    int windowHeight = 600; // Default window height

    // start at i is 1 because the first arg is the program name
    // iterate through pairs (i += 2)
    for (int i = 1; i < argc; i += 2)
    {
        // convert argv[i] into a c++ string instead of c
        string arg = argv[i];

        // then check the strings to see if they're recognised:
        if (arg == "--id" && i + 1 < argc)
        {
            mouse_ID = argv[i + 1];
        }
        else if (arg == "--date" && i + 1 < argc)
        {
            date_time = argv[i + 1];
        }
        else if (arg == "--path" && i + 1 < argc)
        {
            path = argv[i + 1];
        }
        else if (arg == "--rig" && i + 1 < argc)
        {
            cam = stoi(argv[i + 1]);
        }
        else if (arg == "--fps" && i + 1 < argc)
        {
            FPS = stof(argv[i + 1]);
        }
        else if (arg == "--windowWidth" && i + 1 < argc)
        {
            windowWidth = stoi(argv[i + 1]);
        }
        else if (arg == "--windowHeight" && i + 1 < argc)
        {
            windowHeight = stoi(argv[i + 1]);
        }
    }

    if (date_time.empty())
    {
        auto now = system_clock::now();
        time_t now_time = system_clock::to_time_t(now);
        char buffer[80];
        tm localTime;
        localtime_s(&localTime, &now_time);
        strftime(buffer, sizeof(buffer), "%y%m%d_%H%M%S", &localTime);
        date_time = string(buffer);
    }

    if (path.empty())
    {
        // Default path if none is provided
        path = "E:\\test_vid_output";  // Change this to your desired default path
        path += "\\" + date_time + "_" + mouse_ID;
        if (_mkdir(path.c_str()) != 0) {
            cerr << "Error: Unable to create directory " << path << endl;
            return -1;
        }
    }

    try
    {
        Tracker camera(mouse_ID, date_time, path, cam, FPS, windowWidth, windowHeight);
        camera.startTracking(true, true);
    }
    catch (const std::exception& e)
    {
        cerr << "Error: " << e.what() << endl;
        return -1;
    }

    return 0;
}
