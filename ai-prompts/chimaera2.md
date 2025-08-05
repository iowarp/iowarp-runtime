# Division of Runtime and Client code
work orchestrator and module manager are entirely runtime objects. Do not use chi::ipc data structures here, just std:: is fine. 

Assume that the macro CHIMAERA_RUNTIME is set in the runtime code to allow for specially compiling code shared between client and runtime.

# Data Structure Scope
Ensure you use HSHM_DATA_STRUCTURES_TEMPLATE(chi, CHI_MAIN_ALLOC_T) in a header file that everything includes to get chi:: and chi::ipc namespaces for data structures, such as vector. 

# Work Orchestrator
Worker and WorkOrchestrator should be implement more in the source code than in the headers. 

# Singletons
Don't make custom singletons. E.g., don't do ``ModuleManager* ModuleManager::instance_ = nullptr;``. This is accounted for using ``HSHM_DEFINE_GLOBAL_CROSS_PTR_VAR_CC(chi::ModuleManager, chiModuleManager);``. You use chiModuleManager as the variable instead of instance_.

# Configuration Manager
ClearParseVector takes as input the entire container for the template type (e.g., std::vector<int>).

# Task Flags and Bitfields
Make use of ``hshm::ibitfield`` here.

Currently we have:
```cpp
namespace TaskFlags {
static const IntFlag kTaskPeriodic = (1 << 0);
static const IntFlag kTaskFireAndForget = (1 << 1);
}  // namespace TaskFlags
```

With hshm, we would use C-style macros:
```cpp
#define TASK_PERIODIC BIT_OPT(int, 0)
#define TASK_FIRE_AND_FORGET(int, 1)
```

bitfield definition is here:
```cpp
#include <hermes_shm/types/bitfield.h>

template <typename T = u32, bool ATOMIC = false>
struct bitfield {
  hipc::opt_atomic<T, ATOMIC> bits_;

  /** Default constructor */
  HSHM_INLINE_CROSS_FUN bitfield() : bits_(0) {}

  /** Emplace constructor */
  HSHM_INLINE_CROSS_FUN explicit bitfield(T mask) : bits_(mask) {}

  /** Copy constructor */
  HSHM_INLINE_CROSS_FUN bitfield(const bitfield &other) : bits_(other.bits_) {}

  /** Copy assignment */
  HSHM_INLINE_CROSS_FUN bitfield &operator=(const bitfield &other) {
    bits_ = other.bits_;
    return *this;
  }

  /** Move constructor */
  HSHM_INLINE_CROSS_FUN bitfield(bitfield &&other) noexcept
      : bits_(other.bits_) {}

  /** Move assignment */
  HSHM_INLINE_CROSS_FUN bitfield &operator=(bitfield &&other) noexcept {
    bits_ = other.bits_;
    return *this;
  }

  /** Copy from any bitfield */
  template <bool ATOMIC2>
  HSHM_INLINE_CROSS_FUN bitfield(const bitfield<T, ATOMIC2> &other)
      : bits_(other.bits_) {}

  /** Copy assignment from any bitfield */
  template <bool ATOMIC2>
  HSHM_INLINE_CROSS_FUN bitfield &operator=(const bitfield<T, ATOMIC2> &other) {
    bits_ = other.bits_;
    return *this;
  }

  /** Set bits using mask */
  HSHM_INLINE_CROSS_FUN void SetBits(T mask) { bits_ |= mask; }

  /** Unset bits in mask */
  HSHM_INLINE_CROSS_FUN void UnsetBits(T mask) { bits_ &= ~mask; }

  /** Check if any bits are set */
  HSHM_INLINE_CROSS_FUN T Any(T mask) const { return (bits_ & mask).load(); }

  /** Check if all bits are set */
  HSHM_INLINE_CROSS_FUN T All(T mask) const { return Any(mask) == mask; }

  /** Copy bits from another bitfield */
  HSHM_INLINE_CROSS_FUN void CopyBits(bitfield field, T mask) {
    bits_ &= (field.bits_ & mask);
  }

  /** Clear all bits */
  HSHM_INLINE_CROSS_FUN void Clear() { bits_ = 0; }

  /** Make a mask */
  HSHM_INLINE_CROSS_FUN static T MakeMask(int start, int length) {
    return ((((T)1) << length) - 1) << start;
  }

  /** Serialization */
  template <typename Ar>
  void serialize(Ar &ar) {
    ar & bits_;
  }
};
```