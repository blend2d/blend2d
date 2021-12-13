// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_p.h"
#include "../support/arenaallocator_p.h"
#include "../support/arenalist_p.h"

// ArenaList - Tests
// =================

#ifdef BL_TEST
class MyListNode : public BLArenaListNode<MyListNode> {};

UNIT(arena_list, -5) {
  BLArenaAllocator zone(4096);
  BLArenaList<MyListNode> list;

  MyListNode* a = zone.newT<MyListNode>();
  MyListNode* b = zone.newT<MyListNode>();
  MyListNode* c = zone.newT<MyListNode>();
  MyListNode* d = zone.newT<MyListNode>();

  INFO("Append / Unlink");

  // []
  EXPECT_TRUE(list.empty());

  // [A]
  list.append(a);
  EXPECT_FALSE(list.empty());
  EXPECT_EQ(list.first(), a);
  EXPECT_EQ(list.last(), a);
  EXPECT_EQ(a->prev(), nullptr);
  EXPECT_EQ(a->next(), nullptr);

  // [A, B]
  list.append(b);
  EXPECT_EQ(list.first(), a);
  EXPECT_EQ(list.last(), b);
  EXPECT_EQ(a->prev(), nullptr);
  EXPECT_EQ(a->next(), b);
  EXPECT_EQ(b->prev(), a);
  EXPECT_EQ(b->next(), nullptr);

  // [A, B, C]
  list.append(c);
  EXPECT_EQ(list.first(), a);
  EXPECT_EQ(list.last(), c);
  EXPECT_EQ(a->prev(), nullptr);
  EXPECT_EQ(a->next(), b);
  EXPECT_EQ(b->prev(), a);
  EXPECT_EQ(b->next(), c);
  EXPECT_EQ(c->prev(), b);
  EXPECT_EQ(c->next(), nullptr);

  // [B, C]
  list.unlink(a);
  EXPECT_EQ(list.first(), b);
  EXPECT_EQ(list.last(), c);
  EXPECT_EQ(a->prev(), nullptr);
  EXPECT_EQ(a->next(), nullptr);
  EXPECT_EQ(b->prev(), nullptr);
  EXPECT_EQ(b->next(), c);
  EXPECT_EQ(c->prev(), b);
  EXPECT_EQ(c->next(), nullptr);

  // [B]
  list.unlink(c);
  EXPECT_EQ(list.first(), b);
  EXPECT_EQ(list.last(), b);
  EXPECT_EQ(b->prev(), nullptr);
  EXPECT_EQ(b->next(), nullptr);
  EXPECT_EQ(c->prev(), nullptr);
  EXPECT_EQ(c->next(), nullptr);

  // []
  list.unlink(b);
  EXPECT_TRUE(list.empty());
  EXPECT_EQ(list.first(), nullptr);
  EXPECT_EQ(list.last(), nullptr);
  EXPECT_EQ(b->prev(), nullptr);
  EXPECT_EQ(b->next(), nullptr);

  INFO("Prepend / Unlink");

  // [A]
  list.prepend(a);
  EXPECT_FALSE(list.empty());
  EXPECT_EQ(list.first(), a);
  EXPECT_EQ(list.last(), a);
  EXPECT_EQ(a->prev(), nullptr);
  EXPECT_EQ(a->next(), nullptr);

  // [B, A]
  list.prepend(b);
  EXPECT_EQ(list.first(), b);
  EXPECT_EQ(list.last(), a);
  EXPECT_EQ(b->prev(), nullptr);
  EXPECT_EQ(b->next(), a);
  EXPECT_EQ(a->prev(), b);
  EXPECT_EQ(a->next(), nullptr);

  INFO("InsertAfter / InsertBefore");

  // [B, A, C]
  list.insertAfter(a, c);
  EXPECT_EQ(list.first(), b);
  EXPECT_EQ(list.last(), c);
  EXPECT_EQ(b->prev(), nullptr);
  EXPECT_EQ(b->next(), a);
  EXPECT_EQ(a->prev(), b);
  EXPECT_EQ(a->next(), c);
  EXPECT_EQ(c->prev(), a);
  EXPECT_EQ(c->next(), nullptr);

  // [B, D, A, C]
  list.insertBefore(a, d);
  EXPECT_EQ(list.first(), b);
  EXPECT_EQ(list.last(), c);
  EXPECT_EQ(b->prev(), nullptr);
  EXPECT_EQ(b->next(), d);
  EXPECT_EQ(d->prev(), b);
  EXPECT_EQ(d->next(), a);
  EXPECT_EQ(a->prev(), d);
  EXPECT_EQ(a->next(), c);
  EXPECT_EQ(c->prev(), a);
  EXPECT_EQ(c->next(), nullptr);

  INFO("PopFirst / Pop");

  // [D, A, C]
  EXPECT_EQ(list.popFirst(), b);
  EXPECT_EQ(b->prev(), nullptr);
  EXPECT_EQ(b->next(), nullptr);

  EXPECT_EQ(list.first(), d);
  EXPECT_EQ(list.last(), c);
  EXPECT_EQ(d->prev(), nullptr);
  EXPECT_EQ(d->next(), a);
  EXPECT_EQ(a->prev(), d);
  EXPECT_EQ(a->next(), c);
  EXPECT_EQ(c->prev(), a);
  EXPECT_EQ(c->next(), nullptr);

  // [D, A]
  EXPECT_EQ(list.pop(), c);
  EXPECT_EQ(c->prev(), nullptr);
  EXPECT_EQ(c->next(), nullptr);

  EXPECT_EQ(list.first(), d);
  EXPECT_EQ(list.last(), a);
  EXPECT_EQ(d->prev(), nullptr);
  EXPECT_EQ(d->next(), a);
  EXPECT_EQ(a->prev(), d);
  EXPECT_EQ(a->next(), nullptr);
}
#endif
