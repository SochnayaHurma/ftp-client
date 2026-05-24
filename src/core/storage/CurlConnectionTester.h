#pragma once

#include "RemoteConnectionProfile.h"

#include <QString>

namespace stl {

struct ConnectionTestResult {
    bool ok = false;
    QString message;
};

class CurlConnectionTester {
public:
    static ConnectionTestResult test(const RemoteConnectionProfile& profile);
};

} // namespace stl
