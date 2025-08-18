#ifndef CHIMAERA_INCLUDE_CHIMAERA_DOMAIN_QUERY_H_
#define CHIMAERA_INCLUDE_CHIMAERA_DOMAIN_QUERY_H_

#include "chimaera/types.h"
#include <cereal/cereal.hpp>

namespace chi {

/**
 * Domain query class for determining task execution location and routing
 * 
 * Provides methods to query different domain types and hash functions
 * for load balancing and task distribution.
 */
class DomainQuery {
 public:
  /**
   * Default constructor
   */
  DomainQuery();

  /**
   * Copy constructor
   */
  DomainQuery(const DomainQuery& other);

  /**
   * Assignment operator
   */
  DomainQuery& operator=(const DomainQuery& other);

  /**
   * Destructor
   */
  ~DomainQuery();

  /**
   * Get local domain identifier
   * @return SubDomainId for local execution
   */
  SubDomainId GetLocalId() const;

  /**
   * Get global domain identifier  
   * @return SubDomainId for global execution
   */
  SubDomainId GetGlobalId() const;

  /**
   * Get local hash for load balancing
   * @return Hash value for local domain
   */
  u32 GetLocalHash() const;

  /**
   * Get global hash for load balancing
   * @return Hash value for global domain
   */
  u32 GetGlobalHash() const;

  /**
   * Get global broadcast domain
   * @return SubDomainId for broadcasting
   */
  SubDomainId GetGlobalBcast() const;

  /**
   * Get dynamic domain for adaptive scheduling
   * @return SubDomainId for dynamic execution
   */
  SubDomainId GetDynamic() const;

  /**
   * Cereal serialization support
   * @param ar Archive for serialization
   */
  template<class Archive>
  void serialize(Archive& ar) {
    ar(local_id_, global_id_, local_hash_, global_hash_);
  }

 private:
  SubDomainId local_id_;
  SubDomainId global_id_;
  u32 local_hash_;
  u32 global_hash_;
};

}  // namespace chi

#endif  // CHIMAERA_INCLUDE_CHIMAERA_DOMAIN_QUERY_H_