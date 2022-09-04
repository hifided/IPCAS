# XIONGMAI IP CAMERAS AUTO SAVER
autosaver for xiongmai-software ip cameras via p2p cloudID (use NetSDK)

------------------------------
Almost all Chinese cameras use Xiongmai software. 
Unfortunately, no program clients (like XMeye, ICSee etc.) allows you to download all videos from ip camera storage to your hdd(instead, it is suggested to buy cloud storage).
So this VS project use C++ Xiongmai NetSDK to connect and automatically download records from camera day by day.

## How To Use

1) To connect to your ip dvr via Xiongmai CloudID you need to fill following hardcoded fields in **``main``** function:
```C++
char hostname[64] = "16wordsannumbers";     // hostname is a hardcoded ip dvr's CloudID
char username[NET_NAME_PASSWORD_LEN] = "admin";     // username = login to acces your dvr
char password[NET_NAME_PASSWORD_LEN] = "password";     // password = password (surpisingly)
```

2) Choose downloading folder, changing it in **``downloadFromTo``** function.
Default downloading folder hardcoded as "C:\\".

3) To set the initial time, write it to a log.txt file and put it in ``IPCAS.exe`` folder.  
``log.txt`` time format is YYYY.MM.DD_hh.mm.ss (example 2021.01.01_06:00:00).

P.S. Note, that ``NetSDK.dll`` and ``StreamReader.dll`` should be next to ``IPCAS.exe`` file.

## Dependencies (or How To Dev)
Dependencies
````
netsdk.h
NetSDK.dll
StreamReader.dll
````
This Visual Studio project contains all necessary dependencies to run "from the box".
If you want to change something, I added a ``netsdk`` folder with manual and some x64 NetSDK C++ libraries for this project that I once found on the Internet.  
Now seems like they can't be found anymore, so if you text me, I can additionally share full С++/С# NetSDK libraries, template projects and manuals.

P.S. The manual is older than dll, so some functions may not be described
