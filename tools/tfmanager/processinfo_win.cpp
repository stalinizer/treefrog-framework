/* Copyright (c) 2011-2013, AOYAMA Kazuharu
 * All rights reserved.
 *
 * This software may be used and distributed according to the terms of
 * the New BSD License, which is incorporated herein by reference.
 */

#include <QtCore>
#include <TWebApplication>
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <winternl.h>
#include <ntstatus.h>
#include "processinfo.h"

namespace TreeFrog {


bool ProcessInfo::exists() const
{
    return ProcessInfo::allConcurrentPids().contains(processId);
}


qint64 ProcessInfo::ppid() const
{
    qint64 ppid = 0;
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (hProcess) {
        PROCESS_BASIC_INFORMATION basicInfo;
        if (NtQueryInformationProcess(hProcess, ProcessBasicInformation, &basicInfo, sizeof(basicInfo), NULL) == STATUS_SUCCESS) {
            ppid = (qint64)basicInfo.InheritedFromUniqueProcessId;
        }
    }
    return ppid;
}


QString ProcessInfo::processName() const
{
    QString ret;
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (hProcess) {
        WCHAR fileName[512];
        DWORD len = GetModuleFileNameEx(hProcess, NULL, (LPWSTR)fileName, 512);
        if (len > 0) {
            QString path = QString::fromUtf16((ushort*)fileName);
            ret = QFileInfo(path).baseName();
        }
        CloseHandle(hProcess);
    }
    return ret;
}


static BOOL CALLBACK terminateProc(HWND hwnd, LPARAM procId)
{
    DWORD currentPid = 0;
    GetWindowThreadProcessId(hwnd, &currentPid);
    if (currentPid == (DWORD)procId) {
        PostMessage(hwnd, WM_CLOSE, 0, 0);
        return FALSE;
    }
    return TRUE;
}


void ProcessInfo::terminate()
{
    if (processId > 0) {
#if QT_VERSION < 0x050000
        EnumWindows(terminateProc, processId);
#else
        // Sends to the local socket of tfmanager
        TWebApplication::sendLocalCtrlMessage(QByteArray::number(WM_CLOSE), processId);

        Q_UNUSED(terminateProc);
#endif
    }
}


void ProcessInfo::kill()
{
    if (processId > 0) {
        HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, (DWORD)processId);
        if (hProcess){
            TerminateProcess(hProcess, 0);
            WaitForSingleObject(hProcess, 500);
            CloseHandle(hProcess);
        }
    }
    processId = -1;
}


static BOOL CALLBACK restartProc(HWND hwnd, LPARAM procId)
{
    DWORD currentPid = 0;
    GetWindowThreadProcessId(hwnd, &currentPid);
    if (currentPid == (DWORD)procId) {
        PostMessage(hwnd, WM_APP, 0, 0);
        return FALSE;
    }
    return TRUE;
}


void ProcessInfo::restart()
{
    if (processId > 0) {
#if QT_VERSION < 0x050000
        EnumWindows(restartProc, processId);
#else
        // Sends to the local socket of tfmanager
        TWebApplication::sendLocalCtrlMessage(QByteArray::number(WM_APP), processId);

        Q_UNUSED(restartProc);
#endif
    }
}


QList<qint64> ProcessInfo::allConcurrentPids()
{
    QList<qint64> ret;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32 entry;

    entry.dwSize = sizeof(PROCESSENTRY32);
    if (Process32First(hSnapshot, &entry)) {
        do {
            ret << (qint64)entry.th32ProcessID;
        } while(Process32Next(hSnapshot, &entry));
    }

    qSort(ret.begin(), ret.end());  // Sorts the items
    return ret;
}

} // namespace TreeFrog
