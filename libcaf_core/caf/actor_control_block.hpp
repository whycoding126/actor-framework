/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_ACTOR_CONTROL_BLOCK_HPP
#define CAF_ACTOR_CONTROL_BLOCK_HPP

#include <atomic>

#include "caf/fwd.hpp"
#include "caf/node_id.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/weak_intrusive_ptr.hpp"

namespace caf {

/// Actors are always allocated with a control block that stores its identity
/// as well as strong and weak reference counts to it. Unlike
/// "common" weak pointer designs, the goal is not to allocate the data
/// separately. Instead, the only goal is to break cycles. For
/// example, linking two actors automatically creates a cycle when using
/// strong reference counts only.
///
/// When allocating a new actor, CAF will always embed the user-defined
/// actor in an `actor_storage` with the control block prefixing the
/// actual actor type, as shown below.
///
///     +----------------------------------------+
///     |            actor_storage<T>            |
///     +----------------------------------------+
///     | +-----------------+------------------+ |
///     | |  control block  |  actor data (T)  | |
///     | +-----------------+------------------+ |
///     | | ref count       | mailbox          | |
///     | | weak ref count  | .                | |
///     | | actor ID        | .                | |
///     | | node ID         | .                | |
///     | +-----------------+------------------+ |
///     +----------------------------------------+
///
/// Actors start with a strong reference count of 1. This count is transferred
/// to the first `actor` or `typed_actor` handle used to store the actor.
/// Actors will also start with a weak reference count of 1. This count
/// is decremenated once the strong reference count drops to 0.
///
/// The data block is destructed by calling the destructor of `T` when the
/// last strong reference expires. The storage itself is destroyed when
/// the last weak reference expires.
class actor_control_block {
public:
  using data_destructor = void (*)(abstract_actor*);
  using block_destructor = void (*)(actor_control_block*);

  actor_control_block(actor_id x, node_id& y, actor_system* sys,
                      data_destructor ddtor, block_destructor bdtor)
      : strong_refs(1),
        weak_refs(1),
        aid(x),
        nid(std::move(y)),
        home_system(sys),
        data_dtor(ddtor),
        block_dtor(bdtor) {
    // nop
  }

  actor_control_block(const actor_control_block&) = delete;
  actor_control_block& operator=(const actor_control_block&) = delete;

  std::atomic<size_t> strong_refs;
  std::atomic<size_t> weak_refs;
  const actor_id aid;
  const node_id nid;
  actor_system* const home_system;
  const data_destructor data_dtor;
  const block_destructor block_dtor;

  /// Returns a pointer to the actual actor instance.
  inline abstract_actor* get() {
    // this pointer arithmetic is compile-time checked in actor_storage's ctor
    return reinterpret_cast<abstract_actor*>(
      reinterpret_cast<intptr_t>(this) + CAF_CACHE_LINE_SIZE);
  }

  /// Returns a pointer to the control block that stores
  /// identity and reference counts for this actor.
  static actor_control_block* from(const abstract_actor* ptr) {
    // this pointer arithmetic is compile-time checked in actor_storage's ctor
    return reinterpret_cast<actor_control_block*>(
          reinterpret_cast<intptr_t>(ptr) - CAF_CACHE_LINE_SIZE);
  }

  /// @cond PRIVATE

  inline actor_id id() const noexcept {
    return aid;
  }

  inline const node_id& node() const noexcept {
    return nid;
  }

  void enqueue(strong_actor_ptr sender, message_id mid,
               message content, execution_unit* host);

  void enqueue(mailbox_element_ptr what, execution_unit* host);

  /// @endcond
};

/// @relates actor_control_block
bool intrusive_ptr_upgrade_weak(actor_control_block* x);

/// @relates actor_control_block
inline void intrusive_ptr_add_weak_ref(actor_control_block* x) {
  x->weak_refs.fetch_add(1, std::memory_order_relaxed);
}

/// @relates actor_control_block
void intrusive_ptr_release_weak(actor_control_block* x);

/// @relates actor_control_block
inline void intrusive_ptr_add_ref(actor_control_block* x) {
  x->strong_refs.fetch_add(1, std::memory_order_relaxed);
}

/// @relates actor_control_block
void intrusive_ptr_release(actor_control_block* x);

/// @relates abstract_actor
/// @relates actor_control_block
using strong_actor_ptr = intrusive_ptr<actor_control_block>;

/// @relates abstract_actor
/// @relates actor_control_block
using weak_actor_ptr = weak_intrusive_ptr<actor_control_block>;

/// @relates strong_actor_ptr
void serialize(serializer&, strong_actor_ptr&, const unsigned int);

/// @relates strong_actor_ptr
void serialize(deserializer&, strong_actor_ptr&, const unsigned int);

/// @relates weak_actor_ptr
void serialize(serializer&, weak_actor_ptr&, const unsigned int);

/// @relates weak_actor_ptr
void serialize(deserializer&, weak_actor_ptr&, const unsigned int);

/// @relates strong_actor_ptr
std::string to_string(const strong_actor_ptr&);

/// @relates weak_actor_ptr
std::string to_string(const weak_actor_ptr&);

} // namespace caf

#endif // CAF_ACTOR_CONTROL_BLOCK_HPP