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

#ifndef CHIMAERA_RPC_THALLIUM_H_
#define CHIMAERA_RPC_THALLIUM_H_

#include <thallium.hpp>
#include <string>
#include "hermes_shm/util/singleton.h"

#include "rpc.h"
#include "serialize.h"

namespace tl = thallium;

namespace chm {

/**
   A structure to represent Thallium state
*/
class ThalliumRpc {
 public:
  std::atomic<bool> kill_requested_; /**< is kill requested? */
  std::unique_ptr<tl::engine> client_engine_; /**< pointer to client engine */
  std::unique_ptr<tl::engine> server_engine_; /**< pointer to server engine */
  RpcContext *rpc_;

  /** initialize RPC context  */
  ThalliumRpc() {}

  /** Initialize server */
  void ServerInit(RpcContext *rpc) {
    rpc_ = rpc;
    HILOG(kInfo, "Initializing RPC server");
    std::string addr = rpc->GetMyRpcAddress();
    HILOG(kInfo, "Attempting to start server on: {}", addr);
    try {
      server_engine_ = std::make_unique<tl::engine>(
          addr, THALLIUM_SERVER_MODE);
    } catch (std::exception &e) {
      HELOG(kFatal, "RPC init failed for host: {}\n{}", addr, e.what());
    }
    std::string rpc_server_name = server_engine_->self();
    HILOG(kInfo, "Serving {} (i.e., {}) with {} RPC threads as node id {}",
          rpc_server_name,
          addr,
          rpc->num_threads_,
          rpc->node_id_);
    ClientInit(rpc);
  }

  /** Initialize client */
  void ClientInit(RpcContext *rpc) {
    rpc_ = rpc;
    std::string protocol = rpc->GetProtocol();
    client_engine_ = std::make_unique<tl::engine>(protocol,
                                                  THALLIUM_CLIENT_MODE,
                                                  true, 1);
    HILOG(kInfo, "This client is on node {} (i.e., {}, proto: {})",
          rpc->node_id_, rpc->GetHostNameFromNodeId(DomainQuery::GetNode(rpc->node_id_)), protocol);
  }

  /** Run the daemon */
  void RunDaemon() {
    HILOG(kInfo, "Daemon has started on node: {}", rpc_->node_id_);
    server_engine_->wait_for_finalize();
    HILOG(kInfo, "Daemon has stopped on node: {}", rpc_->node_id_);
  }

  /** Stop this daemon */
  void StopThisDaemon() {
    client_engine_->finalize();
    server_engine_->finalize();
  }

  /** Thallium-compatible server name */
  std::string GetServerName(NodeId node_id) {
    std::string ip_address = rpc_->GetIpAddressFromNodeId(DomainQuery::GetNode(node_id));
    return rpc_->protocol_ + "://" +
        std::string(ip_address) +
        ":" + std::to_string(rpc_->port_);
  }

  /** Register an RPC with thallium */
  template<typename RpcLambda>
  void RegisterRpc(tl::pool &pool, const char *name, RpcLambda &&lambda) {
    server_engine_->define(name, std::forward<RpcLambda>(lambda), 0, pool);
  }

  /** RPC call */
  template <typename RetT, bool ASYNC, typename... Args>
  RetT Call(NodeId node_id, const std::string &func_name, Args&&... args) {
    HILOG(kDebug, "Calling {} {} -> {}", func_name, rpc_->node_id_, node_id)
    try {
      std::string server_name = GetServerName(node_id);
      tl::remote_procedure remote_proc = client_engine_->define(func_name);
      tl::endpoint server = client_engine_->lookup(server_name);
      if constexpr(!ASYNC) {
        if constexpr (std::is_same<RetT, void>::value) {
          remote_proc.disable_response();
          remote_proc.on(server)(std::forward<Args>(args)...);
        } else {
          RetT result = remote_proc.on(server)(std::forward<Args>(args)...);
          return result;
        }
      } else {
        return remote_proc.on(server).async(std::forward<Args>(args)...);
      }
    } catch (tl::margo_exception &err) {
      HELOG(kFatal, "(node {} -> {}) Thallium failed on function: {}: {}",
            rpc_->node_id_, node_id, func_name, err.what())
      exit(1);
    }
  }

  /** RPC call */
  template <typename RetT, typename... Args>
  RetT SyncCall(NodeId node_id, const std::string &func_name, Args&&... args) {
    return Call<RetT, false>(
        node_id, func_name, std::forward<Args>(args)...);
  }

  /** Async RPC call */
  template <typename... Args>
  thallium::async_response AsyncCall(NodeId node_id,
                                     const std::string &func_name,
                                     Args&&... args) {
    return Call<thallium::async_response, true>(
        node_id, func_name, std::forward<Args>(args)...);
  }

  /** Make a generic bulk */
  tl::bulk MakeBulk(
      tl::engine &engine,
      const std::vector<DataTransfer> &xfer,
      tl::bulk_mode flag,
      size_t &size_bytes) {
    size_bytes = 0;
    std::vector<std::pair<void *, size_t>> segments(xfer.size());
    for (size_t i = 0; i < xfer.size(); ++i) {
      segments[i].first = xfer[i].data_;
      segments[i].second = xfer[i].data_size_;
      size_bytes += xfer[i].data_size_;
    }
    tl::bulk bulk;
    if (size_bytes) {
      bulk = engine.expose(segments, flag);
    }
    return bulk;
  }

  /** Make bulk object for data transfer client-side */
  tl::bulk MakeBulkClient(const std::vector<DataTransfer> &xfer,
                          u32 xfer_flag,
                          size_t &size_bytes) {
    tl::bulk_mode flag;
    if (xfer_flag & DT_RECEIVER_READ) {
      flag = tl::bulk_mode::read_only;
    } else {
      flag = tl::bulk_mode::write_only;
    }
    return MakeBulk(*client_engine_, xfer, flag, size_bytes);
  }

  /** Make bulk object for data transfer at server-side */
  tl::bulk MakeBulkServer(const  std::vector<DataTransfer> &xfer,
                          u32 xfer_flag,
                          size_t &size_bytes) {
    tl::bulk_mode flag;
    if (xfer_flag & DT_RECEIVER_READ) {
      flag = tl::bulk_mode::read_only;
    } else {
      flag = tl::bulk_mode::write_only;
    }
    return MakeBulk(*server_engine_, xfer, flag, size_bytes);
  }

  /** I/O transfers */
  template<typename RetT, bool ASYNC, typename ...Args>
  RetT IoCall(i32 node_id, const std::string &func_name,
              const SegmentedTransfer &xfer, u32 io_flag, Args&& ...args) {
    HILOG(kDebug, "Calling {} {} -> {}", func_name, rpc_->node_id_, node_id)
    try {
      std::string server_name = GetServerName(node_id);
      tl::bulk bulk;
      size_t planned_bytes;
      bulk = MakeBulkClient(
          xfer.bulk_, io_flag, planned_bytes);
      tl::remote_procedure remote_proc =
          client_engine_->define(func_name);
      tl::endpoint server = client_engine_->lookup(server_name);
      if constexpr (!ASYNC) {
        if constexpr (std::is_same_v<RetT, void>) {
          remote_proc.disable_response();
          remote_proc.on(server)(bulk, xfer,
              std::forward<Args>(args)...);
        } else {
          return remote_proc.on(server)(bulk, xfer,
              std::forward<Args>(args)...);
        }
      } else {
        return remote_proc.on(server).async(bulk, xfer,
              std::forward<Args>(args)...);
      }
    } catch (tl::margo_exception &err) {
      HELOG(kFatal, "(node {} -> {}) Thallium failed on function: {}: {}",
            rpc_->node_id_, node_id, func_name, err.what())
      exit(1);
    }
  }

  /** Synchronous I/O transfer */
  template<typename RetT, typename ...Args>
  RetT SyncIoCall(i32 node_id, const std::string &func_name,
                  SegmentedTransfer &xfer, u32 io_flag, Args&& ...args) {
    return IoCall<RetT, false>(
        node_id, func_name, xfer, io_flag, std::forward<Args>(args)...);
  }

  /** I/O transfers */
  template<typename ...Args>
  thallium::async_response AsyncIoCall(NodeId node_id, const char *func_name,
                                       SegmentedTransfer &xfer, u32 io_flag,
                                       Args&& ...args) {
    return IoCall<thallium::async_response, true>(
        node_id, func_name, xfer, io_flag, std::forward<Args>(args)...);
  }

  /** Io transfer at the server */
  size_t IoCallServerRead(const tl::request &req,
                          tl::bulk &bulk,
                          SegmentedTransfer &xfer) {
    tl::endpoint endpoint = req.get_endpoint();
    size_t planned_bytes, real_bytes = 0;
    tl::bulk local_rbulk =
        MakeBulkServer(xfer.bulk_, DT_RECEIVER_READ,
                       planned_bytes);
    if (planned_bytes == 0) {
      return 0;
    }
    try {
      real_bytes += bulk.on(endpoint) << local_rbulk;
    } catch (std::exception &e) {
      HELOG(kFatal, "(node {}) Failed to perform bulk I/O thallium: {}",
            rpc_->node_id_, e.what());
    }
    return real_bytes;
  }

  /** Io transfer at the server */
  size_t IoCallServerWrite(const tl::request &req,
                           tl::bulk &bulk,
                           const SegmentedTransfer &xfer) {
    tl::endpoint endpoint = req.get_endpoint();
    size_t planned_bytes, real_bytes = 0;
    tl::bulk local_wbulk =
        MakeBulkServer(xfer.bulk_, DT_RECEIVER_WRITE,
                       planned_bytes);
    if (planned_bytes == 0) {
      return 0;
    }
    try {
      real_bytes += bulk.on(endpoint) >> local_wbulk;
    } catch (std::exception &e) {
      HELOG(kFatal, "(node {}) Failed to perform bulk I/O thallium: {}",
            rpc_->node_id_, e.what());
    }
    return real_bytes;
  }

  /** Check if request is complete */
  bool IsDone(thallium::async_response &req) {
    return req.received();
  }

  /** Wait for async thallium to complete */
  template<typename RetT>
  RetT Wait(thallium::async_response &req) {
    if constexpr(std::is_same_v<void, RetT>) {
      req.wait().as<RetT>();
    } else {
      return req.wait();
    }
  }

  /** Check if async thallium complete */
  bool IsComplete(thallium::async_response &req) {
    return req.received();
  }
};

}  // namespace hermes

/** Lets thallium know how to serialize an enum */
#define SERIALIZE_ENUM(T)\
  template <typename A>\
  void save(A &ar, T &mode) {\
    int cast = static_cast<int>(mode);\
    ar << cast;\
  }\
  template <typename A>\
  void load(A &ar, T &mode) {\
    int cast;\
    ar >> cast;\
    mode = static_cast<T>(cast);\
  }

#endif  // CHIMAERA_RPC_THALLIUM_H_
