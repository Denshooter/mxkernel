#include <iostream>
#include <mx/tasking/runtime.h>

static const uint64_t CORES = 4;
static const uint64_t NUM = 8;
static const uint64_t COUNT = 4;

class DummyTask : public mx::tasking::TaskInterface
{
    uint64_t num;
public:
    DummyTask() : num(-1) {}
    ~DummyTask() override = default;

    void setNum(uint64_t c) { num = c; }

    mx::tasking::TaskResult execute(const std::uint16_t core_id, const std::uint16_t /*channel_id*/) override
    {
        std::cout << "DummyTask cpuid: " << core_id << " num " << (num + 1) << " / " << NUM << std::endl;
        return mx::tasking::TaskResult::make_remove();
    }
};

class CreateTask : public mx::tasking::TaskInterface
{
    uint64_t cnt;
    static uint8_t endOnce;
public:
    CreateTask() : cnt(-1) {}
    ~CreateTask() override = default;

    void setCnt(uint64_t c) { cnt = c; }

    mx::tasking::TaskResult end()
    {
        const uint8_t testEnd = __atomic_add_fetch(&CreateTask::endOnce, 1, __ATOMIC_SEQ_CST);
        if(testEnd == CORES)
        {
            return mx::tasking::TaskResult::make_stop();
        }
        return mx::tasking::TaskResult::make_remove();
    }

    mx::tasking::TaskResult execute(const std::uint16_t core_id, const std::uint16_t /*channel_id*/) override
    {
        // check counter
        if(cnt >= COUNT)
        {
            return end();
        }

        // enqueue dummy tasks
        for(uint64_t i = 0; i < NUM; i++)
        {
            auto *dummy = mx::tasking::runtime::new_task<DummyTask>(core_id);
            dummy->annotate(core_id);
            dummy->setNum(i);
            mx::tasking::runtime::spawn(*dummy);
        }

        // enqueue creation task
        auto *create = mx::tasking::runtime::new_task<CreateTask>(core_id);
        create->annotate(core_id);
        create->setCnt(cnt + 1);
        mx::tasking::runtime::spawn(*create);

        std::cout << "CreateTask cpuid: " << core_id << " cnt " << (cnt + 1) << " / " << COUNT << std::endl;
        return mx::tasking::TaskResult::make_remove();
    }
};

uint8_t CreateTask::endOnce = 0;

int main()
{
    // Define which cores will be used
    const auto cores = mx::util::core_set::build(CORES);

    { // Scope for the MxTasking runtime.

        // Create a runtime for the given cores.
        mx::tasking::runtime_guard _{cores};

        for(uint16_t i = 0; i < CORES; i++)
        {
            auto *create = mx::tasking::runtime::new_task<CreateTask>(i);
            create->annotate(i);
            create->setCnt(0);
            mx::tasking::runtime::spawn(*create);
        }
    }

    return 0;
}