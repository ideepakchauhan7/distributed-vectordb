#include "src/common/error/status_code.h"
#include "src/common/error/status.h"
#include "src/common/error/error_or.h"
#include <iostream>
#include <string>

using namespace vectordb::common;

ErrorOr<std::string> TestFunc(bool succeed) {
    if (succeed) {
        return std::string("Success");
    }
    return Status(StatusCode::kNotFound, "Item not found");
}

Status RunMacro() {
    // Tests our ASSIGN_OR_RETURN macro
    ASSIGN_OR_RETURN(std::string val, TestFunc(true));
    std::cout << val << std::endl;
    return Status::Ok();
}

int main() {
    auto res1 = TestFunc(true);
    if (res1.IsOk()) {
        std::cout << "Direct call (Success): " << res1.value() << std::endl;
    }

    auto res2 = TestFunc(false);
    if (!res2.IsOk()) {
        std::cout << "Direct call (Failure): " << res2.status().ToString() << std::endl;
    }

    auto macro_res = RunMacro();
    if (macro_res.IsOk()) {
        std::cout << "Macro succeeded\n";
    }

    return 0;
}
