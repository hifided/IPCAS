#define _CRT_SECURE_NO_WARNINGS
#define MAX_RECORDS	64

#include "netsdk.h"

#include <iostream>
#include <fstream>
#include <string>
#include <mutex>
#include <condition_variable>
#include <filesystem>

using namespace std;
namespace fs = std::filesystem;

mutex mtx;
condition_variable cv;
bool stop_thread = false;

// creating folders from path string
int createFolderTree(string sPath)
{
    fs::path pSavedFileDIR = sPath;
    try {
        if (!fs::exists(pSavedFileDIR) || !fs::is_directory(pSavedFileDIR))
            fs::create_directories(pSavedFileDIR);
        return 1;
    }
    catch (std::filesystem::filesystem_error const& ex) {
        printf("Error: %s\n", ex.code().message().c_str());
        return 0;
    }
}

// download progress callback function
// it changes stop_thread flag when the current record downloading is finished to unlock mutex
void cbDownLoadPos(long lPlayHandle, long lTotalSize, long lDownLoadSize, long dwUser)
{
    if (lDownLoadSize == lTotalSize) // means 100 %
    {
        bool stopGetFile = H264_DVR_StopGetFile(lPlayHandle);
        if (stopGetFile)
        {
            // unlocking mutex (see downloadPeriod func)
            stop_thread = true;
            cv.notify_all();
        }
    }
    int downloadPos = H264_DVR_GetDownloadPos(lPlayHandle);
    printf("Progress: %d 0/o, downloaded (KB) %d of %d\r", downloadPos, lDownLoadSize, lTotalSize);     // idk how to print % :)
}

// digits from d to 0d form
string withZero(int a)
{   
    string zerostr = "0";
    if (abs(a) <= 9)
        zerostr += to_string(abs(a));
    else
        zerostr = to_string(a);
    return zerostr;
}

// month string for folder names
string to_month(int imonth)
{
    switch (imonth)
    {
    case 1:
        return "Jan";
    case 2:
        return "Feb";
    case 3:
        return "Mar";
    case 4:
        return "Apr";
    case 5:
        return "May";
    case 6:
        return "Jun";
    case 7:
        return "Jul";
    case 8:
        return "Aug";
    case 9:
        return "Sep";
    case 10:
        return "Oct";
    case 11:
        return "Nov";
    case 12:
        return "Dec";
    default:
        return "Unknown";
    }
}


// write last record timestamp to log file
// logfile contains the following time format: YYYY:MM:DD_hh:mm:ss
int setLastRecordTime(SDK_SYSTEM_TIME time)
{
    std::ofstream out;
    out.open("log.txt");
    if (!out.is_open())
    {
        printf("Can't open logfile for write\n");
        return 0;
    }
    else
    {
        out << time.year << "." << withZero(time.month) << "." << withZero(time.day) << "_" << withZero(time.hour) << ":" << withZero(time.minute) << ":" << withZero(time.second);
        out.close();
        return 1;
    }
}


// convert "YYYY:MM:DD_hh:mm:ss" string to H264_DVR_TIME time format
int strToH264_DVR_TIME(H264_DVR_TIME* H264Time, string str)
{
    try
    {
        H264Time->dwYear = stoi(str.substr(0, 4));
        H264Time->dwMonth = stoi(str.substr(5, 2));
        H264Time->dwDay = stoi(str.substr(8, 2));
        H264Time->dwHour = stoi(str.substr(11, 2));
        H264Time->dwMinute = stoi(str.substr(14, 2));
        H264Time->dwSecond = stoi(str.substr(17, 2));
    }
    catch (const std::invalid_argument& ia) {
        printf("Invalid H264_DVR_TIME argument: %s\n", ia.what());
        return 0;
    }

    catch (const std::out_of_range& oor) {
        printf("H264_DVR_TIME Out of Range value: %s\n", oor.what());
        return 0;
    }

    catch (const std::exception& e)
    {
        printf("strToH264_DVR_TIME undefined error : %s\n", e.what());
        return 0;
    }
    return 1;
}

// read last record timestamp from log file
// then write it to H264_DVR_TIME structure
int getLastRecordTime(H264_DVR_TIME* H264Time)
{
    string firstline;

    ifstream in("log.txt");
    if (!in.is_open())
    {
        printf("Can't open logfile for read\n");
        return 0;
    }
    else
    {
        getline(in, firstline);
        if (strToH264_DVR_TIME(H264Time, firstline) != 1)
        {
            printf("Can't convert string to H264_DVR_TIME\n");
            return 0;
        };
        in.close();
        return 1;
    }
}

// convert tm time format to H264_DVR_TIME format
int tmToH264_DVR_TIME(H264_DVR_TIME* H264Time, tm* tmtime)
{
    H264Time->dwYear = 1900 + tmtime->tm_year;
    H264Time->dwMonth = 1 + tmtime->tm_mon;
    H264Time->dwDay = tmtime->tm_mday;
    H264Time->dwHour = tmtime->tm_hour;
    H264Time->dwMinute = tmtime->tm_min;
    H264Time->dwSecond = tmtime->tm_sec;
    return 1;
}

// get system time into H264_DVR_TIME format
int getCurrentTime(H264_DVR_TIME* currentTime)
{
    time_t t = std::time(0);   // get current system time
    tm* now = std::localtime(&t);
    if (!tmToH264_DVR_TIME(currentTime, now))
    {
        printf("Can't convert tm to H264_DVR_TIME\n");
        return 0;
    };
    return 1;
}

// this is the major function
// downloads all records from startPeriod time to endPeriod time into created folders tree
// root of the folder tree has been hardcoded as "C:\", you can also change it or parameterize by yourself
int downloadFromTo(long lLoginID, H264_DVR_TIME startPeriod, H264_DVR_TIME endPeriod)
{
    H264_DVR_FINDINFO lpFindInfo;
    memset(&lpFindInfo, 0, sizeof(lpFindInfo));
    lpFindInfo.nChannelN0 = 0;
    lpFindInfo.nFileType = SDK_RECORD_ALL;
    lpFindInfo.startTime = startPeriod;
    lpFindInfo.endTime = endPeriod;


    H264_DVR_FILE_DATA lpFileData[MAX_RECORDS];
    memset(lpFileData, 0, sizeof(H264_DVR_FILE_DATA) * MAX_RECORDS);

    int findcount = -1;
    long findFile = -1;
    string filename;
    string sSavedFileDIR;
    string fileFullPath;
    string rootFolder = "C:\\";

    while (findcount != 0)
    {
        findFile = H264_DVR_FindFile(lLoginID, &lpFindInfo, lpFileData, MAX_RECORDS, &findcount, 5000);
        if (!findFile)
        {
            printf("findFile=%lui,nError:%d\n", findFile, H264_DVR_GetLastError());
            return 0;
        }

        for (int irec = 0; irec < findcount; ++irec)
        {
            filename = withZero(lpFileData[irec].stBeginTime.hour) + withZero(lpFileData[irec].stBeginTime.minute) + withZero(lpFileData[irec].stBeginTime.second) + ".h264";
            sSavedFileDIR = rootFolder + to_string(lpFileData[irec].stBeginTime.year) + "\\" + to_month(lpFileData[irec].stBeginTime.month) + "\\" + to_string(lpFileData[irec].stBeginTime.day) + "\\";
            fileFullPath = sSavedFileDIR + filename;

            if (!createFolderTree(sSavedFileDIR))
            {
                printf("Folder tree creation error\n");
                return 0;
            }

            long getFileByName = H264_DVR_GetFileByName(lLoginID, &lpFileData[irec], (char*)fileFullPath.c_str(), cbDownLoadPos, 0, 0);
            printf("\nDownload \"%s\" record started\n", filename.c_str());
            if (!getFileByName)
            {
                printf("\ngetFileByName=%lui,nError:%d\n", getFileByName, H264_DVR_GetLastError());
                return 0;
            }
            std::unique_lock<std::mutex> lck(mtx); // locking cause func H264_DVR_GetFileByName does not waiting the end of downloading
            while (!stop_thread) cv.wait(lck);
            stop_thread = false;
            
            if (!setLastRecordTime(lpFileData[irec].stEndTime))
            {
                printf("Can't set last record day into log file\n");
                return 0;
            }
        }
        // at the next iteration the download start time will be the end time of the previous video
        lpFindInfo.startTime.dwYear = lpFileData[findcount - 1].stEndTime.year;
        lpFindInfo.startTime.dwMonth = lpFileData[findcount - 1].stEndTime.month;
        lpFindInfo.startTime.dwDay = lpFileData[findcount - 1].stEndTime.day;
        lpFindInfo.startTime.dwHour = lpFileData[findcount - 1].stEndTime.hour;
        lpFindInfo.startTime.dwMinute = lpFileData[findcount - 1].stEndTime.minute;
        lpFindInfo.startTime.dwSecond = lpFileData[findcount - 1].stEndTime.second;
    }
    return 1;
}


int main()
{
    printf("IPCAS is starting\n");
    //INITIALIZE SDK
    H264_DVR_Init(NULL, NULL);
    
    // LOGIN DEVICE using Xiongmai P2P cloud server
    //H264_DVR_Login_Cloud
    char hostname[64] = "16wordsannumbers";     // hostname is a hardcoded ip dvr's CloudID
    char username[NET_NAME_PASSWORD_LEN] = "admin";     // username = login to acces your dvr
    char password[NET_NAME_PASSWORD_LEN] = "password";     // password = password (surpisingly)
    
    int error = 0;
    H264_DVR_DEVICEINFO lpDeviceInfo;
    memset(&lpDeviceInfo, 0, sizeof(lpDeviceInfo));

    long lLoginID = H264_DVR_Login_Cloud((char*)hostname, 34567, (char*)username, (char*)password, (LPH264_DVR_DEVICEINFO)(&lpDeviceInfo), &error, NULL);
    printf("lLoginID=%lui,nError:%d\n", lLoginID, error);
    if (!lLoginID)
    {
        printf("Cloud login failed\n");
        return 0;
    }

    //set the date from which the records download will start
    // can be changed by edit log file "log.txt"
    H264_DVR_TIME startPeriod;
    memset(&startPeriod, 0, sizeof(startPeriod));
    if (!getLastRecordTime(&startPeriod))
    {
        printf("Cant't get last record time\n");
        system("PAUSE");
        return 0;
    }

    // set the upper date to which you need to download records
    // default = current OS time
    H264_DVR_TIME endPeriod; // <- you can change ending time by your hands, just setting struct H264_DVR_TIME endPeriod fields(int)
    memset(&endPeriod, 0, sizeof(endPeriod));
    if (!getCurrentTime(&endPeriod))
    {
        printf("Cant't get current time\n");
        system("PAUSE");
        return 0;
    }
    
   // download records from startPeriod to endPeriod
   if (!downloadFromTo(lLoginID, startPeriod, endPeriod))
   {
       printf("Downloading period failure\n");
       system("PAUSE");
       return 0;
   }

    //LOGOUT DEVICE
    //H264_DVR_Logout
    long lLogout = H264_DVR_Logout(lLoginID);
    printf("lLogout=%lui,nError:%d\n", lLogout, error);

    //RELEASE SDK RESOURCES
    //H264_DVR_Cleanup
    if (!H264_DVR_Cleanup())
    {
        printf("SDK resources release failure\n");
        system("PAUSE");
        return 0;
    }

    system("PAUSE");
    return 1;
}