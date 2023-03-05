// jvswatchdog.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"



#define VERSION_DESCRIPTION "2023.03.05.00 \"Akita\""

#ifdef _WIN64
#define ARCHITECTURE_DESCRIPTION "x64"
#else
#define ARCHITECTURE_DESCRIPTION "x86"
#endif

#ifdef _DEBUG
#define PROFILE_DESCRIPTION "Debug"
#else
#define PROFILE_DESCRIPTION "Release"
#endif


#define POLLING_INTERVAL 1000ul // milliseconds


#define JVS_PORT L"COM2"

#define JVS_HEARTBEAT_INTERVAL 60000ul // milliseconds
#define JVS_HEARTBEAT_INTERVAL_RESOLUTION 30000ul // milliseconds


//#define VERBOSE

#ifdef VERBOSE
#define LOG_VERBOSE(...) (fprintf_s(stdout, __VA_ARGS__))
#define LOG_LEVEL_DESCRIPTION "Verbose"
#else
#define LOG_VERBOSE(...)
#define LOG_LEVEL_DESCRIPTION "Default"
#endif // VERBOSE

#define LOG_INFO(...) (fprintf_s(stdout, __VA_ARGS__))
#define LOG_ERROR(...) (fprintf_s(stderr, __VA_ARGS__))

HANDLE hMainThread;
BOOL keepPolling = TRUE;


BOOL WINAPI ctrlHandler(DWORD signal)
{
    UNREFERENCED_PARAMETER(signal);

    keepPolling = FALSE;
    WaitForSingleObject(hMainThread, INFINITE);
    CloseHandle(hMainThread);

    return TRUE;
}


HANDLE JVS_Open()
{
    HANDLE hCom = CreateFile(JVS_PORT, GENERIC_READ | GENERIC_WRITE, 0, NULL,
        OPEN_EXISTING, 0, NULL);
    if (hCom == INVALID_HANDLE_VALUE)
    {
        // This might not be an error per se, so it is not logged as an error.
        // Rather, it might just mean that no JVS device is present.
        LOG_VERBOSE("JVS_Open: CreateFile failed. Error code: %d\n", GetLastError());
        return INVALID_HANDLE_VALUE;
    }

    DWORD errors;
    if (!ClearCommError(hCom, &errors, NULL))
    {
        LOG_ERROR("JVS_Open: ClearCommError failed. Error code: %d\n", GetLastError());
        CloseHandle(hCom);
        return INVALID_HANDLE_VALUE;
    }

    if (!SetupComm(hCom, 516ul, 516ul))
    {
        LOG_ERROR("JVS_Open: SetupComm failed. Error code: %d\n", GetLastError());
        CloseHandle(hCom);
        return INVALID_HANDLE_VALUE;
    }

    DCB dcb = { 0 };
    dcb.DCBlength = sizeof(DCB);

    if (!GetCommState(hCom, &dcb))
    {
        LOG_ERROR("JVS_Open: GetCommState failed. Error code: %d\n", GetLastError());
        CloseHandle(hCom);
        return INVALID_HANDLE_VALUE;
    }

    dcb.BaudRate = CBR_115200;
    dcb.ByteSize = 8u;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;

    if (!SetCommState(hCom, &dcb))
    {
        LOG_ERROR("JVS_Open: SetCommState failed. Error code: %d\n", GetLastError());
        CloseHandle(hCom);
        return INVALID_HANDLE_VALUE;
    }

    if (!EscapeCommFunction(hCom, CLRRTS)) // Not sending
    {
        LOG_ERROR("JVS_Open: EscapeCommFunction failed. Error code: %d\n", GetLastError());
        CloseHandle(hCom);
        return INVALID_HANDLE_VALUE;
    }

    if (!EscapeCommFunction(hCom, SETDTR)) // Open for communications
    {
        LOG_ERROR("JVS_Open: EscapeCommFunction failed. Error code: %d\n", GetLastError());
        CloseHandle(hCom);
        return INVALID_HANDLE_VALUE;
    }

    if (!SetCommMask(hCom, EV_RXCHAR))
    {
        LOG_ERROR("JVS_Open: SetCommMask failed. Error code: %d\n", GetLastError());
        CloseHandle(hCom);
        return INVALID_HANDLE_VALUE;
    }

    COMMTIMEOUTS comTimeouts = { 0 };
    if (!SetCommTimeouts(hCom, &comTimeouts))
    {
        LOG_ERROR("JVS_Open: SetCommTimeouts failed. Error code: %d\n", GetLastError());
        CloseHandle(hCom);
        return INVALID_HANDLE_VALUE;
    }

    return hCom;
}


void JVS_Close(HANDLE hCom)
{
    EscapeCommFunction(hCom, CLRDTR); // Closed for communications
    CloseHandle(hCom);
}


BOOL JVS_Write(HANDLE hCom, const byte *message, DWORD messageLength)
{
    if (!EscapeCommFunction(hCom, SETRTS)) // Not sending (yet)
    {
        LOG_ERROR("JVS_Write: EscapeCommFunction failed. Error code: %d\n", GetLastError());
        return FALSE;
    }

    DWORD numBytesWritten;
    if (!WriteFile(hCom, message, messageLength, &numBytesWritten, NULL))
    {
        LOG_ERROR("JVS_Write: WriteFile failed. Error code: %d\n", GetLastError());
        return FALSE;
    }

    if (numBytesWritten != messageLength)
    {
        LOG_ERROR("JVS_Write: WriteFile failed to write enough bytes: messageLength=%d, numBytesWritten=%d\n",
            messageLength, numBytesWritten);
        return FALSE;
    }

    if (!FlushFileBuffers(hCom))
    {
        LOG_ERROR("JVS_Write: FlushFileBuffers failed. Error code: %d\n", GetLastError());
        return FALSE;
    }

    DWORD errors;
    if (!ClearCommError(hCom, &errors, NULL))
    {
        LOG_ERROR("JVS_Write: ClearCommError failed. Error code: %d\n", GetLastError());
        return FALSE;
    }

    if (!EscapeCommFunction(hCom, SETRTS)) // Sending
    {
        LOG_ERROR("JVS_Write: EscapeCommFunction failed. Error code: %d\n", GetLastError());
        return FALSE;
    }

    return TRUE;
}


BOOL JVS_Reset(HANDLE hCom)
{
    static const DWORD resetMessageLength = 6ul;
    static const byte resetMessage[resetMessageLength] = {
        0xe0, 0xff, 0x03, 0xf0, 0xd9, 0xcb
    };

    if (!JVS_Write(hCom, resetMessage, resetMessageLength))
    {
        LOG_ERROR("JVS_Reset: Failed\n");
        return FALSE;
    }

    LOG_VERBOSE("JVS_Reset: Succeeded\n");
    return TRUE;
}


BOOL JVS_Register(HANDLE hCom)
{
    static const DWORD registerMessageLength = 6ul;
    static const byte registerMessage[registerMessageLength] = {
        0xe0, 0xff, 0x03, 0xf1, 0x01, 0xf4
    };

    if (!JVS_Write(hCom, registerMessage, registerMessageLength))
    {
        LOG_ERROR("JVS_Register: Failed\n");
        return FALSE;
    }

    LOG_VERBOSE("JVS_Register: Succeeded\n");
    return TRUE;
}


void CALLBACK JVS_HeartbeatCallback(UINT uTimerID, UINT uMsg,
    DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2)
{
    UNREFERENCED_PARAMETER(uTimerID);
    UNREFERENCED_PARAMETER(uMsg);
    UNREFERENCED_PARAMETER(dw1);
    UNREFERENCED_PARAMETER(dw2);

    HANDLE hCom = (HANDLE)dwUser;
    JVS_Reset(hCom);
}


int main(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "/?") == 0 || strcmp(argv[i], "/h") == 0)
        {
            LOG_INFO(
                "--------------------------------------------->\n"
                "- JVSWATCHDOG ------------------------------->\n"
                "--------------------------------------------->\n"
                "  Version:       %s\n"
                "  Architecture:  %s\n"
                "  Profile:       %s\n"
                "  Log Level:     %s\n",
                VERSION_DESCRIPTION,
                ARCHITECTURE_DESCRIPTION,
                PROFILE_DESCRIPTION,
                LOG_LEVEL_DESCRIPTION);
            return 0;
        }
    }

    HANDLE pseudoHProcess = GetCurrentProcess();
    HANDLE pseudoHMainThread = GetCurrentThread();

    if (!DuplicateHandle(pseudoHProcess, pseudoHMainThread, pseudoHProcess,
        &hMainThread, NULL, FALSE, DUPLICATE_SAME_ACCESS))
    {
        LOG_ERROR("Failed to get handle to main thread. Error code: %d\n", GetLastError());
        return 1;
    }

    if (!SetConsoleCtrlHandler(ctrlHandler, TRUE))
    {
        LOG_ERROR("Failed to set control handler. Error code: %d\n", GetLastError());
        return 1;
    }

    // Try to open JVS communications.
    // If a JVS device is present, periodic communications with it might be
    // necessary in order to reset the watchdog timer and prevent the system
    // from power-cycling.
    HANDLE hCom = JVS_Open();
    UINT uTimerID = NULL;
    if (hCom == INVALID_HANDLE_VALUE)
    {
        LOG_VERBOSE("JVS Port: Closed\n");
    }
    else
    {
        LOG_VERBOSE("JVS Port: Open\n");
        JVS_Reset(hCom);
        uTimerID = timeSetEvent(JVS_HEARTBEAT_INTERVAL,
            JVS_HEARTBEAT_INTERVAL_RESOLUTION, JVS_HeartbeatCallback,
            (DWORD_PTR)hCom, TIME_PERIODIC | TIME_KILL_SYNCHRONOUS);
        if (uTimerID == NULL)
        {
            LOG_ERROR("Failed to set periodic callback for JVS heartbeat\n");
        }
        else
        {
            LOG_VERBOSE("Starting main loop...\n");
            while (keepPolling) {
                Sleep(POLLING_INTERVAL);
            }
            LOG_VERBOSE("Finished main loop\n");
            timeKillEvent(uTimerID);
        }
        JVS_Close(hCom);
    }

    return 0;
}
