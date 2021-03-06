#include "base/untyped_buffer_view.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

TEST(UntypedBufferView, Construction) {
  base::untyped_buffer buf;
  {
    base::untyped_buffer_view view(buf);
    EXPECT_EQ(view.size(), 0);
    EXPECT_TRUE(view.empty());
  }

  buf.append(1);
  buf.append(2);
  {
    base::untyped_buffer_view view(buf);
    EXPECT_EQ(view.size(), sizeof(int) * 2);
    EXPECT_FALSE(view.empty());
  }
}

TEST(UntypedBufferView, Get) {
  base::untyped_buffer buf;
  buf.append(1);
  buf.append(2);

  base::untyped_buffer_view view(buf);
  EXPECT_EQ(view.get<int>(0), 1);
  EXPECT_EQ(view.get<int>(sizeof(int)), 2);
}

TEST(UntypedBufferView, RemovePrefixChangesSize) {
  base::untyped_buffer buf;
  buf.append(1);
  buf.append(2);
  buf.append(3);

  base::untyped_buffer_view view(buf);
  EXPECT_EQ(view.size(), sizeof(int) * 3);
  view.remove_prefix(sizeof(int));
  EXPECT_EQ(view.size(), sizeof(int) * 2);

  view.remove_prefix(sizeof(int) * 2);
  EXPECT_TRUE(view.empty());
}

TEST(UntypedBufferView, RemovePrefixWithGet) {
  base::untyped_buffer buf;
  buf.append(1);
  buf.append(2);
  buf.append(3);

  base::untyped_buffer_view view(buf);

  view.remove_prefix(sizeof(int));
  EXPECT_EQ(view.get<int>(0), 2);
  EXPECT_EQ(view.get<int>(sizeof(int)), 3);

  view.remove_prefix(sizeof(int));
  EXPECT_EQ(view.get<int>(0), 3);
}

TEST(UntypedBufferView, RemoveSuffixChangesSize) {
  base::untyped_buffer buf;
  buf.append(1);
  buf.append(2);
  buf.append(3);

  base::untyped_buffer_view view(buf);
  EXPECT_EQ(view.size(), sizeof(int) * 3);
  view.remove_suffix(sizeof(int));
  EXPECT_EQ(view.size(), sizeof(int) * 2);

  view.remove_suffix(sizeof(int) * 2);
  EXPECT_TRUE(view.empty());
}

TEST(UntypedBufferView, DataPointer) {
  base::untyped_buffer buf;
  buf.append(1);
  buf.append(2);

  base::untyped_buffer_view view(buf);
  EXPECT_EQ(buf.raw(0), view.data());
  view.remove_prefix(4);
  EXPECT_EQ(buf.raw(4), view.data());
}

}  // namespace
