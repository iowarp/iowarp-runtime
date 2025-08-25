#ifndef CHIMAERA_INCLUDE_CHIMAERA_POOL_QUERY_H_
#define CHIMAERA_INCLUDE_CHIMAERA_POOL_QUERY_H_

#include "chimaera/types.h"
#include <cereal/cereal.hpp>

namespace chi {

/**
 * Routing algorithm modes for PoolQuery
 */
enum class RoutingMode {
  Local,      /**< Route to local node only */
  DirectId,   /**< Route to specific container by ID */
  DirectHash, /**< Route using hash-based load balancing */
  Range,      /**< Route to range of containers */
  Broadcast   /**< Broadcast to all containers */
};

/**
 * Pool query class for determining task execution location and routing
 * 
 * Provides methods to query different container addresses and routing modes
 * for load balancing and task distribution to containers.
 */
class PoolQuery {
 public:
  /**
   * Default constructor
   */
  PoolQuery();

  /**
   * Copy constructor
   */
  PoolQuery(const PoolQuery& other);

  /**
   * Assignment operator
   */
  PoolQuery& operator=(const PoolQuery& other);

  /**
   * Destructor
   */
  ~PoolQuery();

  // Static factory methods to create different types of PoolQuery

  /**
   * Create a local routing pool query
   * @return PoolQuery configured for local container routing
   */
  static PoolQuery Local();

  /**
   * Create a direct ID routing pool query  
   * @param container_id Specific container ID to route to
   * @return PoolQuery configured for direct container ID routing
   */
  static PoolQuery DirectId(ContainerId container_id);

  /**
   * Create a direct hash routing pool query
   * @param hash Hash value for container selection
   * @return PoolQuery configured for hash-based routing to specific container
   */
  static PoolQuery DirectHash(u32 hash);

  /**
   * Create a range routing pool query
   * @param hash Hash value for range selection  
   * @return PoolQuery configured for range-based routing
   */
  static PoolQuery Range(u32 hash);

  /**
   * Create a broadcast routing pool query
   * @return PoolQuery configured for broadcast to all containers
   */
  static PoolQuery Broadcast();

  // Getter methods for internal query parameters (used by routing logic)

  /**
   * Get the hash value for hash-based routing modes
   * @return Hash value used for container routing
   */
  u32 GetHash() const;

  /**
   * Get the container ID for direct ID routing mode
   * @return Container ID for direct routing
   */
  ContainerId GetContainerId() const;

  /**
   * Determine the routing mode of this pool query
   * @return RoutingMode enum indicating how this query should be routed
   */
  RoutingMode GetRoutingMode() const;

  /**
   * Check if pool query is in Local routing mode
   * @return true if routing mode is Local
   */
  bool IsLocalMode() const;

  /**
   * Check if pool query is in DirectId routing mode
   * @return true if routing mode is DirectId
   */
  bool IsDirectIdMode() const;

  /**
   * Check if pool query is in DirectHash routing mode
   * @return true if routing mode is DirectHash
   */
  bool IsDirectHashMode() const;

  /**
   * Check if pool query is in Range routing mode
   * @return true if routing mode is Range
   */
  bool IsRangeMode() const;

  /**
   * Check if pool query is in Broadcast routing mode
   * @return true if routing mode is Broadcast
   */
  bool IsBroadcastMode() const;

  /**
   * Cereal serialization support
   * @param ar Archive for serialization
   */
  template<class Archive>
  void serialize(Archive& ar) {
    ar(routing_mode_, hash_value_, container_id_);
  }

 private:
  RoutingMode routing_mode_;  /**< The routing mode for this query */
  u32 hash_value_;           /**< Hash value for hash-based and range routing */
  ContainerId container_id_; /**< Container ID for direct ID routing */
};

/**
 * Resolved pool query representing a concrete physical address for task execution
 * 
 * Contains the actual node ID and the resolved pool query where a task should be executed.
 */
struct ResolvedPoolQuery {
  u32 node_id_;              /**< Physical node identifier where task should execute */
  PoolQuery pool_query_;     /**< Resolved pool query for the task */
  
  ResolvedPoolQuery() : node_id_(0), pool_query_() {}
  ResolvedPoolQuery(u32 node_id, const PoolQuery& pool_query)
      : node_id_(node_id), pool_query_(pool_query) {}
      
  // Equality operator
  bool operator==(const ResolvedPoolQuery& other) const {
    return node_id_ == other.node_id_ && pool_query_.GetHash() == other.pool_query_.GetHash();
  }
  
  // Inequality operator  
  bool operator!=(const ResolvedPoolQuery& other) const {
    return !(*this == other);
  }
  
  // Cereal serialization support
  template<class Archive>
  void serialize(Archive& ar) {
    ar(node_id_, pool_query_);
  }
};


}  // namespace chi

#endif  // CHIMAERA_INCLUDE_CHIMAERA_POOL_QUERY_H_