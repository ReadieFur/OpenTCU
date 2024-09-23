#pragma once

#include "EServiceResult.h"
#include <mutex>
#include <functional>

// template <typename TInstallParams>
class AService
{
private:
    std::mutex _serviceMutex;
    bool _installed = false;
    bool _running = false;

    static int ImplWrapper(std::function<int()> func, std::mutex* mutex, bool& status, bool desiredStatus)
    {
        mutex->lock();
        if (status == desiredStatus)
        {
            mutex->unlock();
            return EServiceResult::Ok;
        }

        int retVal = func();
        if (retVal == EServiceResult::Ok)
            status = desiredStatus;

        mutex->unlock();
        return retVal;
    }

protected:
    virtual int InstallServiceImpl() = 0;
    virtual int UninstallServiceImpl() = 0;
    virtual int StartServiceImpl() = 0;
    virtual int StopServiceImpl() = 0;

public:
    bool IsInstalled()
    {
        _serviceMutex.lock();
        bool retVal = _installed;
        _serviceMutex.unlock();
        return retVal;
    }

    bool IsRunning()
    {
        _serviceMutex.lock();
        bool retVal = _running;
        _serviceMutex.unlock();
        return retVal;
    }

    int InstallService()
    {
        return ImplWrapper([this](){ return InstallServiceImpl(); }, &_serviceMutex, _installed, true);
    }

    int UninstallService()
    {
        int stopServiceRes = StopService();
        if (stopServiceRes != EServiceResult::Ok)
            return stopServiceRes;

        return ImplWrapper([this](){ return UninstallServiceImpl(); }, &_serviceMutex, _installed, false);
    }

    int StartService()
    {
        if (!IsInstalled())
            return EServiceResult::NotInstalled;

        return ImplWrapper([this](){ return StartServiceImpl(); }, &_serviceMutex, _running, true);
    }

    int StopService()
    {
        if (IsInstalled())
            return EServiceResult::NotInstalled;

        return ImplWrapper([this](){ return StopServiceImpl(); }, &_serviceMutex, _running, false);
    }
};
