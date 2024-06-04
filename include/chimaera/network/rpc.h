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

#ifndef HRUN_RPC_H_
#define HRUN_RPC_H_

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <ifaddrs.h>

#include <functional>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#include "chimaera/chimaera_types.h"
#include "chimaera/config/config_server.h"

namespace chi {

/** Create RPC singleton */
#define CHI_RPC \
  hshm::Singleton<RpcContext>::GetInstance()
#define HRUN_RPC CHI_RPC

/** Uniquely identify a host machine */
struct HostInfo {
  NodeId node_id_;           /**< Hermes-assigned node id */
  std::string hostname_;  /**< Host name */
  std::string ip_addr_;   /**< Host IP address */

  HostInfo() = default;
  explicit HostInfo(const std::string &hostname,
                    const std::string &ip_addr,
                    NodeId node_id)
      : hostname_(hostname), ip_addr_(ip_addr), node_id_(node_id) {}
};

/** Domain map size */
struct DomainMapEntry {
  size_t size_;
  std::unordered_set<SubDomainId> ids_set_;
  std::vector<SubDomainId> ids_;

  DomainMapEntry() : size_(0) {}

  void Expand(SubDomainIdRange &range) {
    for (size_t i = range.off_; i < range.off_ + range.count_; ++i) {
      SubDomainId id(range.group_, i);
      if (ids_set_.find(id) == ids_set_.end()) {
        ids_set_.insert(id);
        ids_.emplace_back(id);
        size_ += 1;
      }
    }
  }

  void Contract(SubDomainIdRange &range) {
    std::vector<SubDomainId> ids;
    for (const SubDomainId &id : ids_) {
      if (id.major_ != range.group_) {
        continue;
      }
      if (range.off_ <= id.minor_ && id.minor_ < range.off_ + range.count_) {
        ids_set_.erase(id);
      } else {
        ids.emplace_back(id);
      }
    }
    ids_ = std::move(ids);
  }

  SubDomainId& Get(LaneId hash) {
    return ids_[hash % size_];
  }
};

/** A structure to represent RPC context. */
class RpcContext {
 public:
  ServerConfig *config_;
  int port_;  /**< port number */
  std::string protocol_;  /**< Libfabric provider */
  std::string domain_;    /**< Libfabric domain */
  NodeId node_id_;        /**< the ID of this node */
  int num_threads_;       /**< Number of RPC threads */
  std::vector<HostInfo> hosts_;  /**< Hostname and ip addr per-node */
  size_t neighborhood_size_ = 32;

 public:
  /** The type for storing domain mappings */
  typedef std::unordered_map<DomainId, DomainMapEntry> DOMAIN_MAP_T;
  /** The table for storing domain mappings */
  DOMAIN_MAP_T domain_map_;
  /** A rwlock for lane mappings */
  RwLock domain_map_lock_;

 public:
  /**
   * Add a set of subdomains to the domain
   * */
  void UpdateDomains(std::vector<UpdateDomainInfo> &ops) {
    ScopedRwWriteLock(domain_map_lock_, 0);
    for (UpdateDomainInfo &info : ops) {
      auto it = domain_map_.find(info.domain_id_);
      if (it == domain_map_.end()) {
        domain_map_.emplace(info.domain_id_, DomainMapEntry());
      }
      DomainMapEntry &entry = domain_map_[info.domain_id_];
      switch (info.op_) {
        case UpdateDomainOp::kContract: {
          entry.Contract(info.range_);
          break;
        }
        case UpdateDomainOp::kExpand: {
          entry.Expand(info.range_);
          break;
        }
      }
    }
  }

  /** Get the size of a domain */
  size_t GetDomainSize(const DomainId &dom_id) {
    ScopedRwReadLock lock(domain_map_lock_, 0);
    auto it = domain_map_.find(dom_id);
    if (it != domain_map_.end()) {
      return it->second.size_;
    }
    return 0;
  }

  /**
   * Get SubDomainId from domain query
   * */
  SubDomainId GetSubDomainId(const TaskStateId &scope,
                             const DomainQuery &dom_query) {
    if (dom_query.flags_.Any(DomainQuery::kId)) {
      return SubDomainId(dom_query.sub_id_, dom_query.sel_.id_);
    } else if (dom_query.flags_.Any(DomainQuery::kHash)) {
      DomainId major_id(scope, dom_query.sub_id_);
      DomainMapEntry &major_entry = domain_map_[major_id];
      SubDomainId id = major_entry.Get(dom_query.sel_.hash_);
      return id;
    } else {
      return SubDomainId(dom_query.sub_id_);
    }
  }

  /**
   * Get DomainID from domain query
   * */
  DomainId GetDomainId(const TaskStateId &scope, const DomainQuery &dom_query) {
    return DomainId(scope, GetSubDomainId(scope, dom_query));
  }

  /**
   * Resolve the minor domain of a domain query
   * */
  void ResolveMinorDomain(const TaskStateId &scope,
                          const DomainQuery &dom_query,
                          std::vector<ResolvedDomainQuery> &res,
                          bool full) {
    // Get minor domain
    DomainId dom_id = GetDomainId(scope, dom_query);
    DomainMapEntry &entry = domain_map_[dom_id];

    // Minor subdomain contains only nodes
    for (const SubDomainId &id : entry.ids_) {
      if (id.IsPhysical()) {
        ResolvedDomainQuery sub_query;
        sub_query.dom_ = DomainQuery::GetLocalId(SubDomainId::kContainerSet,
                                                 dom_id.sub_id_.minor_);
        sub_query.node_ = id.minor_;
        res.emplace_back(sub_query);
      } else if (id.IsMinor()) {
        DomainQuery sub_query = DomainQuery::GetDirectId(
            id.major_, id.minor_, DomainQuery::kBroadcast);
        ResolveMinorDomain(scope, sub_query, res, full);
      } else if (id.IsMajor()) {
        DomainQuery sub_query = DomainQuery::GetGlobal(
            id.major_, DomainQuery::kBroadcast);
        ResolveMinorDomain(scope, sub_query, res, full);
      }
    }
  }

  /**
   * Resolve the major domain of a domain query
   * */
  void ResolveMajorDomain(const TaskStateId &scope,
                          const DomainQuery &dom_query,
                          std::vector<ResolvedDomainQuery> &res,
                          bool full) {
    // Get major domain
    DomainId dom_id = GetDomainId(scope, dom_query);
    DomainMapEntry &entry = domain_map_[dom_id];

    // Get size of major domain and divide among neighbors
    size_t dom_size, dom_off;
    if (dom_query.flags_.Any(DomainQuery::kRange)) {
      dom_off = dom_query.sel_.range_.off_;
      dom_size = dom_query.sel_.range_.count_;
    } else {
      dom_off = 0;
      dom_size = entry.size_;
    }

    // Divide into sub-queries
    if (dom_size <= neighborhood_size_ || full) {
      // Concretize range queries into local queries
      DomainQuery sub_query = DomainQuery::GetDirectHash(
          dom_query.sub_id_, dom_off, DomainQuery::kBroadcast);
      for (size_t i = 0; i < dom_size; ++i) {
        ResolveMinorDomain(scope, sub_query, res, full);
        sub_query.sel_.hash_ += 1;
      }
    } else {
      // Create smaller range queries
      for (size_t i = 0; i < dom_size; i += neighborhood_size_) {
        ResolvedDomainQuery sub_query;
        size_t rem_size = std::min(neighborhood_size_, dom_size - i);
        sub_query.dom_ = DomainQuery::GetRange(
            dom_query.sub_id_, dom_off, rem_size, dom_query.GetIterFlags());
        u32 forward = hosts_.size() / neighborhood_size_;
        if (forward < 2) {
          sub_query.node_ = node_id_;
        } else {
          sub_query.node_ = forward * neighborhood_size_;
        }
        res.emplace_back(sub_query);
        dom_off += neighborhood_size_;
      }
    }
  }

  /**
   * Convert a DomainQuery into a set of more concretized queries.
   * */
  std::vector<ResolvedDomainQuery>
  ResolveDomainQuery(const TaskStateId &scope,
                     const DomainQuery &dom_query,
                     bool full) {
    ScopedRwReadLock lock(domain_map_lock_, 0);
    std::vector<ResolvedDomainQuery> res;
    if (dom_query.flags_.All(DomainQuery::kLocal | DomainQuery::kId)) {
      // Keep task on this node
      ResolvedDomainQuery sub_query;
      sub_query.dom_ = dom_query;
      sub_query.node_ = node_id_;
      res.emplace_back(sub_query);
    } else if(dom_query.flags_.Any(DomainQuery::kForwardToLeader)) {
      // Forward to leader
      DomainQuery sub_query = DomainQuery::GetDirectHash(
          dom_query.sub_id_, 1, dom_query.flags_.bits_);
      ResolveMinorDomain(scope, sub_query, res, full);
      res[0].dom_ = dom_query;
      res[0].dom_.flags_.UnsetBits(DomainQuery::kForwardToLeader);
    } else if (dom_query.flags_.Any(DomainQuery::kDirect)) {
      ResolveMinorDomain(scope, dom_query, res, full);
    } else if (dom_query.flags_.Any(DomainQuery::kGlobal)) {
      ResolveMajorDomain(scope, dom_query, res, full);
    } else {
      HELOG(kFatal, "Unknown domain query type")
    }
    return res;
  }

  /** Create the default domains */
  std::vector<UpdateDomainInfo>
  CreateDefaultDomains(const TaskStateId &task_state,
                       const TaskStateId &admin_state,
                       const DomainQuery &scope_query,
                       u32 global_lanes,
                       u32 local_lanes_pn) {
    std::vector<UpdateDomainInfo> ops;
    // Resolve the admin scope domain
    std::vector<ResolvedDomainQuery> dom = ResolveDomainQuery(
        admin_state, scope_query, true);
    size_t dom_size = dom.size();
    if (dom_size == 0) {
      for (u32 i = 1; i <= hosts_.size(); ++i) {
        dom.emplace_back(ResolvedDomainQuery{i});
      }
    }
    // Create the set of all lanes
    {
      // Create the major LaneSet domain
      size_t total_dom_size = global_lanes +
          local_lanes_pn * dom.size();
      DomainId dom_id(task_state, SubDomainId::kContainerSet);
      SubDomainIdRange range(
          SubDomainId::kContainerSet,
          1,
          total_dom_size);
      ops.emplace_back(UpdateDomainInfo{
          dom_id, UpdateDomainOp::kExpand, range});
    }
    // Create the set of global lanes
    {
      // Create the major GlobalLaneSet domain
      DomainId dom_id(task_state, SubDomainId::kGlobalContainers);
      SubDomainIdRange range(
          SubDomainId::kGlobalContainers,
          1,
          global_lanes);
      ops.emplace_back(UpdateDomainInfo{
          dom_id, UpdateDomainOp::kExpand, range});
      for (size_t i = 1; i <= global_lanes; ++i) {
        // Update LaneSet
        SubDomainIdRange res_set(
            SubDomainId::kPhysicalNode, dom[i % dom.size()].node_, 1);
        ops.emplace_back(UpdateDomainInfo{
            DomainId(task_state, SubDomainId::kContainerSet, i),
            UpdateDomainOp::kExpand, res_set});
        // Update GlobalLaneSet
        SubDomainIdRange res_glob(
            SubDomainId::kContainerSet, i, 1);
        ops.emplace_back(UpdateDomainInfo{
            DomainId(task_state, SubDomainId::kGlobalContainers, i),
            UpdateDomainOp::kExpand, res_glob});
      }
    }
    // Create the set of local lanes
    {
      // Create the major LocalLaneSet domain
      DomainId dom_id(task_state, SubDomainId::kLocalContainers);
      SubDomainIdRange range(
          SubDomainId::kLocalContainers,
          1,
          local_lanes_pn);
      ops.emplace_back(UpdateDomainInfo{
          dom_id, UpdateDomainOp::kExpand, range});
      // Update LaneSet
      u32 lane_off = global_lanes + 1;
      for (u32 node_id = 1; node_id <= hosts_.size(); ++node_id) {
        for (size_t i = 1; i <= local_lanes_pn; ++i) {
          SubDomainIdRange res_set(
              SubDomainId::kPhysicalNode, node_id, 1);
          // Update LaneSet
          ops.emplace_back(UpdateDomainInfo{
              DomainId(task_state, SubDomainId::kContainerSet, lane_off),
              UpdateDomainOp::kExpand, res_set});
          if (node_id == node_id_) {
            // Update LocalLaneSet
            SubDomainIdRange res_loc(
                SubDomainId::kContainerSet, lane_off, 1);
            ops.emplace_back(UpdateDomainInfo{
                DomainId(task_state, SubDomainId::kLocalContainers, i),
                UpdateDomainOp::kExpand, res_loc});
          }
          ++lane_off;
        }
      }
    }
    // Create the set of caching lanes
    // TODO(llogan)
    return ops;
  }

  /** Get the set of lanes on this node */
  std::vector<SubDomainId> GetLocalContainers(const TaskStateId &scope) {
    std::vector<SubDomainId> res;
    size_t dom_size = GetDomainSize(DomainId(scope, SubDomainId::kContainerSet));
    ScopedRwReadLock lock(domain_map_lock_, 0);
    for (u32 i = 0; i < dom_size; ++i) {
      std::vector<ResolvedDomainQuery> res_query = ResolveDomainQuery(
          scope, DomainQuery::GetDirectHash(SubDomainId::kContainerSet, i), true);
      if (res_query.size() == 1 && res_query[0].node_ == node_id_) {
        res.emplace_back(SubDomainId::kContainerSet, res_query[0].dom_.sel_.id_);
      }
    }
    return res;
  }

 public:
  /** Default constructor */
  RpcContext() = default;

  /** initialize host info list */
  void ServerInit(ServerConfig *config) {
    config_ = config;
    port_ = config_->rpc_.port_;
    protocol_ = config_->rpc_.protocol_;
    domain_ = config_->rpc_.domain_;
    num_threads_ = config_->rpc_.num_threads_;
    if (hosts_.size()) { return; }
    // Uses hosts produced by host_names
    std::vector<std::string> &hosts =
        config_->rpc_.host_names_;
    // Get all host info
    hosts_.reserve(hosts.size());
    NodeId node_id = 1;
    for (const std::string& name : hosts) {
      hosts_.emplace_back(name, _GetIpAddress(name), node_id++);
    }
    // Get id of current host
    node_id_ = _FindThisHost();
    if (node_id_ == 0 || node_id_ > (u32)hosts_.size()) {
      HELOG(kFatal, "Couldn't identify this host.");
    }
  }

  /** get RPC address */
  std::string GetRpcAddress(NodeId node_id, int port) {
    if (config_->rpc_.protocol_ == "shm") {
      return "shm";
    }
    std::string result = config_->rpc_.protocol_ + "://";
    if (!config_->rpc_.domain_.empty()) {
      result += config_->rpc_.domain_ + "/";
    }
    std::string host_name = GetHostNameFromNodeId(node_id);
    result += host_name + ":" + std::to_string(port);
    return result;
  }

  /** Get RPC address for this node */
  std::string GetMyRpcAddress() {
    return GetRpcAddress(node_id_, port_);
  }

  /** get host name from node ID */
  std::string GetHostNameFromNodeId(NodeId node_id) {
    // NOTE(llogan): node_id 0 is reserved as the NULL node
    if (node_id <= 0 || node_id > (i32)hosts_.size()) {
      HELOG(kFatal, "Attempted to get from node {}, which is out of "
                    "the range 1-{}", node_id, hosts_.size())
    }
    u32 index = node_id - 1;
    return hosts_[index].hostname_;
  }

  /** get host name from node ID */
  std::string GetIpAddressFromNodeId(NodeId node_id){
    // NOTE(llogan): node_id 0 is reserved as the NULL node
    if (node_id <= 0 || node_id > (u32)hosts_.size()) {
      HELOG(kFatal, "Attempted to get from node {}, which is out of "
                    "the range 1-{}", node_id, hosts_.size())
    }
    u32 index = node_id - 1;
    return hosts_[index].ip_addr_;
  }

  /** Get RPC protocol */
  std::string GetProtocol() {
    return config_->rpc_.protocol_;
  }

 private:
  /** Get the node ID of this machine according to hostfile */
  int _FindThisHost() {
    int node_id = 1;
    for (HostInfo &host : hosts_) {
      if (_IsAddressLocal(host.ip_addr_)) {
        return node_id;
      }
      ++node_id;
    }
    HELOG(kFatal, "Could not identify this host");
    return -1;
  }

  /** Check if an IP address is local */
  bool _IsAddressLocal(const std::string &addr) {
    struct ifaddrs* ifAddrList = nullptr;
    bool found = false;

    if (getifaddrs(&ifAddrList) == -1) {
      perror("getifaddrs");
      return false;
    }

    for (struct ifaddrs* ifAddr = ifAddrList;
         ifAddr != nullptr; ifAddr = ifAddr->ifa_next) {
      if (ifAddr->ifa_addr == nullptr ||
          ifAddr->ifa_addr->sa_family != AF_INET) {
        continue;
      }

      struct sockaddr_in* sin =
          reinterpret_cast<struct sockaddr_in*>(ifAddr->ifa_addr);
      char ipAddress[INET_ADDRSTRLEN] = {0};
      inet_ntop(AF_INET, &(sin->sin_addr), ipAddress, INET_ADDRSTRLEN);

      if (addr == ipAddress) {
        found = true;
        break;
      }
    }

    freeifaddrs(ifAddrList);
    return found;
  }

  /** Get IPv4 address from the host with "host_name" */
  std::string _GetIpAddress(const std::string &host_name) {
    struct hostent hostname_info = {};
    struct hostent *hostname_result;
    int hostname_error = 0;
    char hostname_buffer[4096] = {};
#ifdef __APPLE__
    hostname_result = gethostbyname(host_name.c_str());
  in_addr **addr_list = (struct in_addr **)hostname_result->h_addr_list;
#else
    int gethostbyname_result =
        gethostbyname_r(host_name.c_str(), &hostname_info, hostname_buffer,
                        4096, &hostname_result, &hostname_error);
    if (gethostbyname_result != 0) {
      HELOG(kFatal, hstrerror(h_errno))
    }
    in_addr **addr_list = (struct in_addr **)hostname_info.h_addr_list;
#endif
    if (!addr_list[0]) {
      HELOG(kFatal, hstrerror(h_errno))
    }

    char ip_address[INET_ADDRSTRLEN] = {0};
    const char *inet_result =
        inet_ntop(AF_INET, addr_list[0], ip_address, INET_ADDRSTRLEN);
    if (!inet_result) {
      perror("inet_ntop");
      HELOG(kFatal, "inet_ntop failed");
    }
    return ip_address;
  }
};

}  // namespace hermes

#endif  // HRUN_RPC_H_
