/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Distributed under BSD 3-Clause license.                                   *
 * Copyright by The HDF Group.                                               *
 * Copyright by the Illinois Institute of Technology.                        *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of Hermes. The full Hermes copyright notice, including  *
 * terms governing use, modification, and redistribution, is contained in    *
 * the COPYING file, which can be found at the top directory. If you do not  *
 * have access to the file, you may request a copy from help@hdfgroup.org.   *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef CHI_INCLUDE_CHI_CHI_CONSTANTS_H_
#define CHI_INCLUDE_CHI_CHI_CONSTANTS_H_

namespace chi {

#include <string>

class Constants {
 public:
  inline static const std::string kClientConfEnv = "CHIMAERA_CLIENT_CONF";
  inline static const std::string kServerConfEnv = "CHIMAERA_CONF";

  static std::string GetEnvSafe(const std::string &env_name) {
    char *data = getenv(env_name.c_str());
    if (data == nullptr) {
      return "";
    } else {
      return data;
    }
  }
};

#define HIDE_SYMBOL __attribute__((visibility("hidden")))
#define EXPORT_SYMBOL __attribute__((visibility("default")))

}  // namespace chi

#endif  // CHI_INCLUDE_CHI_CHI_CONSTANTS_H_
