#include <gtest/gtest.h>
#include <mx/util/queue.h>
#include <mx/util/queue_item.h>

TEST(MxTasking, Queue)
{
    auto queue = mx::util::Queue<mx::util::QueueItem>{};
    EXPECT_EQ(queue.empty(), true);

    auto queue_item = mx::util::QueueItem{};
    queue.push_back(&queue_item);
    EXPECT_EQ(queue.empty(), false);
    auto *pulled_item = queue.pop_front();
    EXPECT_EQ(&queue_item, pulled_item);
    EXPECT_EQ(queue.empty(), true);
    EXPECT_EQ(queue.pop_front(), nullptr);
}