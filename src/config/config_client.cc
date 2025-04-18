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

#include "chimaera/config/config_client.h"

#include <hermes_shm/util/config_parse.h>

#include <filesystem>

#include "chimaera/config/config_client_default.h"

namespace stdfs = std::filesystem;

namespace chi::config {

/** parse the YAML node */
void ClientConfig::ParseYAML(YAML::Node &yaml_conf) {}

/** Load the default configuration */
void ClientConfig::LoadDefault() {
  LoadText(kChiDefaultClientConfigStr, false);
}

}  // namespace chi::config
