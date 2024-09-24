#pragma once

namespace ReadieFur::Abstractions
{
    enum EServiceResult
    {
        Ok = 0,
        NotInstalled = -1,
        InUse = -2,
        MissingDependencies = -3,
        DependencyNotReady = -4
    };
};
