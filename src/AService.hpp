#pragma once

#include <mutex>

template <typename TInstallParams = void>
class AService
{
private:
    std::mutex _mutex;
    bool _installed = false;

protected:
    virtual bool InstallImpl(TInstallParams params) = 0;
    virtual bool UninstallImpl() = 0;

public:
    bool Install(TInstallParams params)
    {
        _mutex.lock();
        if (_installed)
        {
            _mutex.unlock();
            return;
        }

        bool retVal = InstallImpl(params);

        _mutex.unlock();

        return retVal;
    }

    bool Uninstall()
    {
        _mutex.lock();
        if (!_installed)
        {
            _mutex.unlock();
            return;
        }

        bool retVal = UninstallImpl();

        _mutex.unlock();

        return retVal;
    }

    bool 
};
