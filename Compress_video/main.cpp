#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <opencv2/opencv.hpp>
#include <thread>
#include <mutex>
#include <cstdlib>
#include <fstream>
#include <algorithm>

using namespace std;
namespace fs = std::filesystem;

mutex frameMutex;
atomic<int> videoIndex(0);

// Function to list and sort BMP files
vector<fs::path> ListBmpFiles(const string& directory, const string& prefix)
{
    vector<fs::path> bmpFiles;

    for (const auto& entry : fs::directory_iterator(directory))
    {
        if (entry.path().extension() == ".bmp" && entry.path().filename().string().find(prefix) == 0)
        {
            bmpFiles.push_back(entry.path());
        }
    }

    sort(bmpFiles.begin(), bmpFiles.end());
    return bmpFiles;
}

// Function to determine dimensions of the video
cv::Size GetVideoDimensions(const fs::path& filePath)
{
    cv::Mat firstImage = cv::imread(filePath.string(), cv::IMREAD_UNCHANGED);
    if (firstImage.empty())
    {
        throw runtime_error("Error: Could not open or find the image: " + filePath.string());
    }
    return cv::Size(firstImage.cols, firstImage.rows);
}

// Function to process a chunk of BMP files into an AVI file
void ProcessVideoChunk(const vector<fs::path>& chunk, int chunkIndex, const fs::path& tempVideoDir, double fps, const cv::Size& frameSize)
{
    string tempVideoFilename = (tempVideoDir / ("chunk_" + to_string(chunkIndex) + ".avi")).string();
    cv::VideoWriter videoWriter(tempVideoFilename, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), fps, frameSize, true);

    if (!videoWriter.isOpened())
    {
        cerr << "Error: Could not open the video writer: " + tempVideoFilename << endl;
        return;
    }

    for (const auto& bmpPath : chunk)
    {
        cv::Mat frame = cv::imread(bmpPath.string(), cv::IMREAD_UNCHANGED);
        if (frame.empty())
        {
            cerr << "Error: Could not open or find the image: " + bmpPath.string() << endl;
            continue;
        }
        videoWriter.write(frame);
    }

    videoWriter.release();
}

// Function to concatenate multiple AVI files into one using ffmpeg
void ConcatenateAviFiles(const vector<string>& inputFilenames, const string& outputFilename)
{
    // Create a temporary text file with the list of video files to concatenate
    string listFilename = "file_list.txt";
    ofstream listFile(listFilename);
    for (const auto& inputFilename : inputFilenames)
    {
        listFile << "file '" << inputFilename << "'\n";
    }
    listFile.close();

    string ffmpegCmd = "ffmpeg -f concat -safe 0 -i \"" + listFilename + "\" -c copy -pix_fmt yuv420p \"" + outputFilename + "\"";
    system(ffmpegCmd.c_str());

    fs::remove(listFilename);
}

// Function to delete BMP files
void DeleteBmpFiles(const vector<fs::path>& bmpFiles)
{
    for (const auto& bmpPath : bmpFiles)
    {
        fs::remove(bmpPath);
    }
}

int main(int argc, char** argv)
{
    if (argc != 4)
    {
        cout << "Usage: " << argv[0] << " <image_directory> <prefix> <output_video_filename>" << endl;
        return -1;
    }

    string imageDirectory = argv[1];
    string prefix = argv[2];
    string outputVideoFilename = argv[3];

    try
    {
        // List and sort BMP files
        vector<fs::path> bmpFiles = ListBmpFiles(imageDirectory, prefix);

        if (bmpFiles.empty())
        {
            cout << "No images found in the directory." << endl;
            return -1;
        }

        // Determine video dimensions
        cv::Size frameSize = GetVideoDimensions(bmpFiles[0]);
        double fps = 170.0; // Frames per second

        // Temporary video directory
        fs::path tempVideoDir = fs::path(imageDirectory) / "temp_videos";
        fs::create_directory(tempVideoDir);

        // Split BMP files into chunks
        int numThreads = thread::hardware_concurrency();
        int numImages = bmpFiles.size();
        int imagesPerThread = (numImages + numThreads - 1) / numThreads;

        vector<thread> threads;
        vector<string> tempVideoFilenames;

        for (int i = 0; i < numThreads; ++i)
        {
            int startIdx = i * imagesPerThread;
            int endIdx = min(startIdx + imagesPerThread - 1, numImages - 1);

            if (startIdx > endIdx)
            {
                break;
            }

            tempVideoFilenames.push_back((tempVideoDir / ("chunk_" + to_string(i) + ".avi")).string());

            vector<fs::path> chunk(bmpFiles.begin() + startIdx, bmpFiles.begin() + endIdx + 1);
            threads.emplace_back(ProcessVideoChunk, chunk, i, tempVideoDir, fps, frameSize);
        }

        for (auto& t : threads)
        {
            if (t.joinable())
            {
                t.join();
            }
        }

        // Concatenate the temporary video files into the final output video using ffmpeg
        ConcatenateAviFiles(tempVideoFilenames, outputVideoFilename);

        // Cleanup temporary video files
        for (const auto& tempVideoFilename : tempVideoFilenames)
        {
            fs::remove(tempVideoFilename);
        }

        fs::remove(tempVideoDir);

        // Delete BMP files
        DeleteBmpFiles(bmpFiles);

        cout << "Video saved at " << outputVideoFilename << endl;
    }
    catch (const exception& e)
    {
        cerr << e.what() << endl;
        return -1;
    }

    return 0;
}
