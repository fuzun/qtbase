// Copyright (C) 2023 Intel Corporation.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
// Qt-Security score:significant reason:default

#ifndef QFUTEX_WIN_P_H
#define QFUTEX_WIN_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <private/qglobal_p.h>
#include <qdeadlinetimer.h>
#include <qtsan_impl.h>
#include <QtCore/private/qsystemlibrary_p.h>

#include <qt_windows.h>

QT_BEGIN_NAMESPACE

namespace QtWindowsFutex {
typedef BOOL(*WaitOnAddressCompat)(_In_ volatile VOID *Address, _In_ PVOID CompareAddress, _In_ SIZE_T AddressSize, _In_ DWORD dwMilliseconds);
typedef void(*WakeByAddressAllCompat)(_In_ PVOID Address);
typedef void(*WakeByAddressSingleCompat)(_In_ PVOID Address);

struct FutexFuncs {
    WaitOnAddressCompat waitOnAddress = nullptr;
    WakeByAddressAllCompat wakeByAddressAll = nullptr;
    WakeByAddressSingleCompat wakeByAddressSingle = nullptr;
};

inline const FutexFuncs futexFuncs = []() {
    FutexFuncs funcs;

    QSystemLibrary synchWin8ApiSet(QLatin1String("api-ms-win-core-synch-l1-2-0"));

    funcs.waitOnAddress = (WaitOnAddressCompat)(synchWin8ApiSet.resolve("WaitOnAddress"));
    funcs.wakeByAddressAll = (WakeByAddressAllCompat)(synchWin8ApiSet.resolve("WakeByAddressAll"));
    funcs.wakeByAddressSingle = (WakeByAddressSingleCompat)(synchWin8ApiSet.resolve("WakeByAddressSingle"));

    if (!funcs.waitOnAddress || !funcs.wakeByAddressAll || !funcs.wakeByAddressSingle)
        qDebug("Qt: Futex functions are not available, mutex will be used instead!"); // Windows 7, or `api-ms-win-core-synch-l1-2-0` is not available.

    return funcs;
}();

inline bool futexAvailable()
{
    return (futexFuncs.waitOnAddress && futexFuncs.wakeByAddressAll && futexFuncs.wakeByAddressSingle);
}

template <typename Atomic>
inline void futexWait(Atomic &futex, typename Atomic::Type expectedValue)
{
    Q_ASSERT(futexFuncs.waitOnAddress);
    QtTsan::futexRelease(&futex);
    futexFuncs.waitOnAddress(&futex, &expectedValue, sizeof(expectedValue), INFINITE);
    QtTsan::futexAcquire(&futex);
}
template <typename Atomic>
inline bool futexWait(Atomic &futex, typename Atomic::Type expectedValue, QDeadlineTimer deadline)
{
    Q_ASSERT(futexFuncs.waitOnAddress);
    using namespace std::chrono;
    BOOL r = futexFuncs.waitOnAddress(&futex, &expectedValue, sizeof(expectedValue), DWORD(deadline.remainingTime()));
    return r || GetLastError() != ERROR_TIMEOUT;
}
template <typename Atomic> inline void futexWakeAll(Atomic &futex)
{
    Q_ASSERT(futexFuncs.wakeByAddressAll);
    futexFuncs.wakeByAddressAll(&futex);
}
template <typename Atomic> inline void futexWakeOne(Atomic &futex)
{
    Q_ASSERT(futexFuncs.wakeByAddressSingle);
    futexFuncs.wakeByAddressSingle(&futex);
}
} // namespace QtWindowsFutex
namespace QtFutex = QtWindowsFutex;

QT_END_NAMESPACE

#endif // QFUTEX_WIN_P_H
