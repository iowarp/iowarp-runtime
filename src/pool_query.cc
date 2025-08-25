/**
 * Pool query implementation
 */

#include "chimaera/pool_query.h"

namespace chi {

PoolQuery::PoolQuery() : routing_mode_(RoutingMode::Local), hash_value_(0), container_id_(0) {
}

PoolQuery::PoolQuery(const PoolQuery& other) 
    : routing_mode_(other.routing_mode_), 
      hash_value_(other.hash_value_), 
      container_id_(other.container_id_) {
}

PoolQuery& PoolQuery::operator=(const PoolQuery& other) {
  if (this != &other) {
    routing_mode_ = other.routing_mode_;
    hash_value_ = other.hash_value_;
    container_id_ = other.container_id_;
  }
  return *this;
}

PoolQuery::~PoolQuery() {
  // Stub destructor
}

// Static factory methods

PoolQuery PoolQuery::Local() {
  PoolQuery query;
  query.routing_mode_ = RoutingMode::Local;
  query.hash_value_ = 0;
  query.container_id_ = 0;
  return query;
}

PoolQuery PoolQuery::DirectId(ContainerId container_id) {
  PoolQuery query;
  query.routing_mode_ = RoutingMode::DirectId;
  query.hash_value_ = 0;
  query.container_id_ = container_id;
  return query;
}

PoolQuery PoolQuery::DirectHash(u32 hash) {
  PoolQuery query;
  query.routing_mode_ = RoutingMode::DirectHash;
  query.hash_value_ = hash;
  query.container_id_ = 0;
  return query;
}

PoolQuery PoolQuery::Range(u32 hash) {
  PoolQuery query;
  query.routing_mode_ = RoutingMode::Range;
  query.hash_value_ = hash;
  query.container_id_ = 0;
  return query;
}

PoolQuery PoolQuery::Broadcast() {
  PoolQuery query;
  query.routing_mode_ = RoutingMode::Broadcast;
  query.hash_value_ = 0;
  query.container_id_ = 0;
  return query;
}

// Getter methods

u32 PoolQuery::GetHash() const {
  return hash_value_;
}

ContainerId PoolQuery::GetContainerId() const {
  return container_id_;
}

RoutingMode PoolQuery::GetRoutingMode() const {
  return routing_mode_;
}

bool PoolQuery::IsLocalMode() const {
  return routing_mode_ == RoutingMode::Local;
}

bool PoolQuery::IsDirectIdMode() const {
  return routing_mode_ == RoutingMode::DirectId;
}

bool PoolQuery::IsDirectHashMode() const {
  return routing_mode_ == RoutingMode::DirectHash;
}

bool PoolQuery::IsRangeMode() const {
  return routing_mode_ == RoutingMode::Range;
}

bool PoolQuery::IsBroadcastMode() const {
  return routing_mode_ == RoutingMode::Broadcast;
}

}  // namespace chi