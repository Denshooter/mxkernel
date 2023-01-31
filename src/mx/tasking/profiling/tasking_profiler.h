#include <chrono>
#include <atomic>
#include <mx/tasking/config.h>


class TaskingProfiler
{
public:
    static TaskingProfiler& getInstance()
    {
        static TaskingProfiler instance;
        return instance;
    }

    struct task_info
    {
        std::uint64_t id;
        std::uint32_t type;
        const char* name;
        std::chrono::high_resolution_clock::time_point start;
        std::chrono::high_resolution_clock::time_point end;
    };
    
    struct queue_info
    {
        std::uint64_t id;
        std::chrono::high_resolution_clock::time_point timestamp;
    };

private:
    TaskingProfiler() {};
    std::chrono::time_point<std::chrono::high_resolution_clock> relTime;
    std::atomic<std::uint64_t> overhead{0};
    std::atomic<std::uint64_t> taskQueueOverhead{0};
    static constexpr std::chrono::time_point<std::chrono::high_resolution_clock> tinit = std::chrono::time_point<std::chrono::high_resolution_clock>::max();

    TaskingProfiler(const TaskingProfiler& copy) = delete;
    TaskingProfiler& operator=(const TaskingProfiler& src) = delete;

    // total number of cores
    std::uint16_t total_cores;

    // profile data inside a multidimensional array
    task_info** task_data;
    queue_info** queue_data;

    // id counters for every core
    std::uint64_t* task_id_counter;
    std::uint64_t* queue_id_counter;

public:
    /**
     * @brief 
     * 
     * @param corenum 
     */
    void init(std::uint16_t corenum);

    /**
     * @brief 
     * 
     * @param type 
     * @return std::uint64_t 
     */
    std::uint64_t startTask(std::uint32_t type, const char* name);
    
    /**
     * @brief 
     * 
     * @param id 
     */
    void endTask(std::uint64_t id);

    /**
     * @brief 
     * 
     * @param corenum
     */
    void enqueue(std::uint16_t corenum);

    /**
     * @brief 
     * 
     * @param file 
     */
    void saveProfile();

    /**
     * @brief 
     * 
     * @param start 
     * @param end 
     */
    void printTP(std::uint64_t start, std::uint64_t end);
};