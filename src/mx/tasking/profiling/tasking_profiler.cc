#include "tasking_profiler.h"

#include <iostream>
#include <chrono>
#include <numeric>
#include <cxxabi.h>

constexpr std::chrono::time_point<std::chrono::high_resolution_clock> TaskingProfiler::tinit;

void printFloatUS(std::uint64_t ns)
{
    std::uint64_t remainder = ns % 1000;
    std::uint64_t front = ns / 1000;
    char strRemainder[4];
    
    for(int i = 2; i >= 0; i--)
    {
        strRemainder[i] = '0' + (remainder % 10);
        remainder /= 10;
    }
    strRemainder[3] = '\0';

    std::cout << front << '.' << strRemainder;
}

void TaskingProfiler::init(std::uint16_t corenum)
{
    this->total_cores = corenum;

    //create an array of pointers to task_info structs
    task_data = new task_info*[total_cores];
    for (std::uint16_t i = 0; i < total_cores; i++)
    {
        task_data[i] = new task_info[CONFIG_TASKING_ARRAY_LENGTH];
        for(size_t j = CONFIG_TASKING_ARRAY_LENGTH; j > 0; j--)
        {
            task_data[i][j] = {0, 0,NULL, tinit, tinit};
            __builtin_prefetch(&task_data[i][j-1], 1, 1);
        }
    }

    //create an array of pointers to queue_info structs
    queue_data = new queue_info*[total_cores];
    for (std::uint16_t i = 0; i < total_cores; i++)
    {
        queue_data[i] = new queue_info[CONFIG_TASKING_ARRAY_LENGTH];
        for(size_t j = CONFIG_TASKING_ARRAY_LENGTH; j > 0; j--)
        {
            queue_data[i][j] = {0, tinit};
            __builtin_prefetch(&queue_data[i][j-1], 1, 1);
        }
    }

    task_id_counter = new std::uint64_t[total_cores]{0};
    queue_id_counter = new std::uint64_t[total_cores]{0};

    relTime = std::chrono::high_resolution_clock::now();
}

std::uint64_t TaskingProfiler::startTask(std::uint32_t type, const char* name)
{
    const std::uint16_t cpu_id = system_info.cpu_id();
    const std::uint64_t tid = task_id_counter[cpu_id]++;
    task_info& ti = task_data[cpu_id][tid];
    std::chrono::time_point<std::chrono::high_resolution_clock> start = std::chrono::high_resolution_clock::now();

    ti.id = tid;
    ti.type = type;
    ti.name = name;
    ti.start = start;

    std::chrono::time_point<std::chrono::high_resolution_clock> end = std::chrono::high_resolution_clock::now();
    overhead += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    return ti.id;
}

void TaskingProfiler::endTask(std::uint64_t id)
{
    std::chrono::time_point<std::chrono::high_resolution_clock> start = std::chrono::high_resolution_clock::now();

    task_info& ti = task_data[system_info.cpu_id()][id];
    ti.end = std::chrono::high_resolution_clock::now();

    std::chrono::time_point<std::chrono::high_resolution_clock> end = std::chrono::high_resolution_clock::now();
    overhead += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
}

void TaskingProfiler::enqueue(std::uint16_t corenum){
    std::chrono::time_point<std::chrono::high_resolution_clock> timestamp = std::chrono::high_resolution_clock::now();

    const std::uint64_t qid = __atomic_add_fetch(&queue_id_counter[corenum], 1, __ATOMIC_SEQ_CST);
    queue_info& qi = queue_data[corenum][qid];
    qi.id = qid;
    qi.timestamp = timestamp;

    std::chrono::time_point<std::chrono::high_resolution_clock> endTimestamp = std::chrono::high_resolution_clock::now();
    taskQueueOverhead += std::chrono::duration_cast<std::chrono::nanoseconds>(endTimestamp - timestamp).count();
}

void TaskingProfiler::saveProfile()
{   
    std::uint64_t overhead_ms = overhead/1000000;
    std::cout << "Overhead-Time: " << overhead << "ns in ms: " << overhead_ms << "ms" << std::endl;
    std::uint64_t taskQueueOverhead_ms = taskQueueOverhead/1000000;
    std::cout << "TaskQueueOverhead-Time: " << taskQueueOverhead << "ns in ms: " << taskQueueOverhead_ms << "ms" << std::endl;

    //get the number of tasks overal
    std::uint64_t tasknum = 0;
    for (std::uint16_t i = 0; i < corenum; i++)
    {
        tasknum += task_id_counter[i];
    }
    std::cout << "Number of tasks: " << tasknum << std::endl;
    std::cout << "Overhead-Time per Task: " << overhead/tasknum << "ns" std::endl;

    bool first = false;
    std::uint64_t firstTime = 0;
    std::uint64_t throughput = 0;
    std::uint64_t duration = 0;
    std::uint64_t lastEndTime = 0;
    std::uint64_t taskQueueLength;

    std::uint64_t timestamp = 0;
    std::uint64_t start = 0;
    std::uint64_t end = 0;
    char* name;

    std::uint64_t taskCounter = 0;
    std::uint64_t queueCounter = 0;

    std::cout << "--Save--" << std::endl;
    std::cout << "{\"traceEvents\":[" << std::endl;

    //Events
    for(std::uint16_t cpu_id = 0; cpu_id < total_cores; cpu_id++)
    {
        //Metadata Events for each core (CPU instead of process as name,...)
        std::cout << "{\"name\":\"process_name\",\"ph\":\"M\",\"pid\":" << cpu_id << ",\"tid\":" << cpu_id << ",\"args\":{\"name\":\"CPU\"}}," std::endl;
        std::cout << "{\"name\":\"process_sort_index\",\"ph\":\"M\",\"pid\":" << cpu_id << ",\"tid\":" << cpu_id << ",\"args\":{\"name\":" << cpu_id << "}}," << std::endl;
        


#ifdef CONFIG_TASK_QUEUE_LENGTH
        taskQueueLength = 0;
        taskCounter = 0;
        queueCounter = 0;

        //Initial taskQueueLength is zero
        std::cout << "{\"pid\":" << cpu_id << ",\"name\":\"CPU" << cpu_id <<  "\",\"ph\":\"C\",\"ts\":";
        printFloatUS(0);
        std::cout << ",\"args\":{\"TaskQueueLength\":" << taskQueueLength << "}}," << std::endl;

        //go through all tasks and queues
        while(taskCounter<task_id_counter[cpu_id] || queueCounter<queue_id_counter[cpu_id]){
            //get the next task and queue
            queue_info& qi = queue_data[cpu_id][queueCounter];
            task_info& ti = task_data[cpu_id][taskCounter];

            if(qi.timestamp == tinit){
                queueCounter++;

                //increase for the first timestamp
                //not logging because of very big timestamp
                if(qi.id == 0){
                    //taskQueueLength++;
                }
                continue;
            }
            if(ti.start == tinit && !first){
                taskCounter++;
                //LOG_INFO("TASK SKIP");
                continue;
            }

            //get the time from the queue element if existing
            if(queueCounter < queue_id_counter[cpu_id]){
                timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(qi.timestamp - relTime).count();
            }

            //get the time's from the task element if existing
            if(taskCounter < task_id_counter[cpu_id]){
                start = std::chrono::duration_cast<std::chrono::nanoseconds>(ti.start - relTime).count();
                end = std::chrono::duration_cast<std::chrono::nanoseconds>(ti.end - relTime).count();
                name = abi::__cxa_demangle(ti.name, 0, 0, 0);
            }

            //get the first time
            if(!first){
                first = true;
                if(timestamp < start)
                    firstTime = timestamp;
                else
                    firstTime = start;
            }
            //if the queue element is before the task element, it is an enqueue
            if(qi.timestamp < ti.start && queueCounter <= queue_id_counter[cpu_id]){
                queueCounter++;
                taskQueueLength++;
                std::cout << "{\"pid\":" << cpu_id << ",\"name\":\"CPU" << cpu_id <<  "\",\"ph\":\"C\",\"ts\":";
                if(timestamp - firstTime == 0){
                    printFloatUS(10);
                }
                else{
                    printFloatUS(timestamp-firstTime);
                }
                std::cout << ",\"args\":{\"TaskQueueLength\":" << taskQueueLength << "}}," << std::endl;

            }
            //else we print the task itself and a dequeue
            else{
                taskCounter++;
                taskQueueLength--;

                //taskQueueLength
                std::cout << "{\"pid\":" << cpu_id << ",\"name\":\"CPU" << cpu_id <<  "\",\"ph\":\"C\",\"ts\":";
                printFloatUS(start-firstTime);
                std::cout << ",\"args\":{\"TaskQueueLength\":" << taskQueueLength << "}}," << std::endl;

                //if the endtime of the last task is too large (cannot display right)
                if(taskCounter == task_id_counter[cpu_id] && ti.end == tinit){
                    end = start + 1000;
                }
                //Task itself
                std::cout << "{\"pid\":" << cpu_id << ",\"tid\":" << cpu_id << ",\"ts\":" << std::endl;
                printFloatUS(start-firstTime);
                std::cout << ",\"dur\":";
                printFloatUS(end-start);
                std::cout << ",\"ph\":\"X\",\"name\":\"" << name << "\",\"args\":{\"type\":" << ti.type << "}}," << std::endl;

                //reset throughput if there is a gap of more than 1us
                if (start - lastEndTime > 1000){
                    std::cout << "{\"pid\":" << cpu_id << ",\"name\":\"CPU" << cpu_id <<  "\",\"ph\":\"C\",\"ts\":";
                    printFloatUS(lastEndTime);
                    std::cout << ",\"args\":{\"TaskThroughput\":";
                    //Tasks per microsecond is zero
                    std::cout << 0;
                    std::cout << "}}," << std::endl;
                }
                duration = end - start;

                //Task Throughput
                std::cout << "{\"pid\":" << cpu_id << ",\"name\":\"CPU" << cpu_id <<  "\",\"ph\":\"C\",\"ts\":";
                printFloatUS(start-firstTime);
                std::cout << ",\"args\":{\"TaskThroughput\":");
                //Tasks per microsecond
                throughput = 1000/duration;
                std::cout << throughput;
                std::cout << "}}," << std::endl;
                lastEndTime = end;
            }
        }
#else //CONFIG_TASK_QUEUE_LENGTH
        for(std::uint32_t i = 0; i < task_id_counter[cpu_id]; i++){
            task_info& ti = task_data[cpu_id][i];

            // check if task is valid
            if(ti.end == tinit)
            {
                continue;
            }
            std::uint64_t start = std::chrono::duration_cast<std::chrono::nanoseconds>(ti.start - relTime).count();
            std::uint64_t end = std::chrono::duration_cast<std::chrono::nanoseconds>(ti.end - relTime).count();
            name = abi::__cxa_demangle(ti.name, 0, 0, 0);

            //Task itself
            std::cout << "{\"pid\":" << cpu_id << ",\"tid\":" << cpu_id << ",\"ts\":";
            printFloatUS(start-firstTime);
            std::cout << ",\"dur\":";
            printFloatUS(end-start);
            std::cout << ",\"ph\":\"X\",\"name\":\"" << name << "\",\"args\":{\"type\":" << ti.type << "}}," << std::endl;
                
            //reset throughput if there is a gap of more than 1us
            if (start - lastEndTime > 1000){
                std::cout << "{\"pid\":" << cpu_id << ",\"name\":\"CPU" << cpu_id <<  "\",\"ph\":\"C\",\"ts\":";
                printFloatUS(lastEndTime);
                std::cout << ",\"args\":{\"TaskThroughput\":";
                //Tasks per microsecond is zero
                std::cout << 0;
                std::cout << "}}," << std::endl;
            }
            duration = end - start;

            //Task Throughput
            std::cout << "{\"pid\":" << cpu_id << ",\"name\":\"CPU" << cpu_id <<  "\",\"ph\":\"C\",\"ts\":";
            printFloatUS(start-firstTime);
            std::cout << ",\"args\":{\"TaskThroughput\":";

            //Tasks per microsecond
            throughput = 1000/duration;

            std::cout << throughput;
            std::cout << "}}," << std::endl;
            lastEndTime = end;
        }
#endif //CONFIG_TASK_QUEUE_LENGTH
    }
            //sample Task (so we dont need to remove the last comma)
            std::cout << "{\"name\":\"sample\",\"ph\":\"P\",\"ts\":0,\"pid\":5,\"tid\":0}";
            std::cout << "]}" << std::endl;;        
}

void TaskingProfiler::printTP(std::uint64_t start, std::uint64_t end)
{
    std::uint64_t tp[total_cores]{0};

    for(std::uint16_t cpu_id = 0; cpu_id < total_cores; cpu_id++)
    {
        // get the task_info array for the current core
        task_info* core_data = task_data[cpu_id];

        // get the id counter for the current core
        for(std::uint64_t i = 0; i < task_id_counter[cpu_id]; i++)
        {
            task_info& ti = core_data[i];
            const std::uint64_t tstart = std::chrono::duration_cast<std::chrono::nanoseconds>(ti.start - relTime).count();
            const std::uint64_t tend = std::chrono::duration_cast<std::chrono::nanoseconds>(ti.end - relTime).count();            
            if(tstart > start && tend < end) {
                tp[cpu_id]++;
            }
        }

        LOG_INFO("TP " << cpu_id << " " << tp[cpu_id]);
    }
    LOG_INFO("TP " << "total " << std::accumulate(tp, tp + total_cores, 0));
}