module;

export module JobSystem;

import std;
/**
 * Job system for submitting multithreaded tasks to be done
 */
namespace JobSystem
{
    /**
     * Priority of the jobs
     */
    enum class Priority
    {
        Low,
        Normal,
        High,
        Critical
    };
    /**
     * Internal implementation details 
     */
    namespace Internal
    {
        /**
         * Interface used for containers to hold Jobs
         */
        struct JobInterface
        {
            virtual ~JobInterface() = default;
            /**
             * 
             * @return return type of the executed task
             */
            virtual auto Execute() = 0;
        };

        /**
         * CRTP interface for job system-- calls each derived job's execute in order to capture scope information.
         * @tparam Derived derived class for each possible job that exists
         */
        template <typename Derived>
        struct JobWrapper : JobInterface
        {
            /**
             * Executes the job
             * @return return type of the executed task
             */
            inline auto Execute() override final
            {
                return static_cast<Derived*>(this)->execute();
            }
        };
        
    }

    /**
     * Type used for submitting functions for the Job System to do \n
     *  \b Usage: Job{"name", name, i, "sample_argument", &referenced_argument } 
     * @tparam Func type of the function submitted
     * @tparam Args variadic arguments-- use to submit as any arguments as the function takes
     */
    export template <typename Func, typename... Args>
    struct Job : Internal::JobWrapper<Job<Func, Args...>>
    {
        /**
         * \b Usage: Job{"name", name, i, "sample_argument", &referenced_argument } \n
         * \b Info: Submit with a \b name, submit the function pointer, and as many arguments as the function takes
         * @param name name of the function -- REQUIRED for debugging purposes
         * @param func function pointer to be enqueued
         * @param args variadic arguments-- submit as many arguments as the function takes-- will be MOVED
         */
        Job(const char* name, Func&& func, Args&&... args) 
            :function_name(name),
            func_(std::forward<Func>(func))
            , v_args(std::forward<Args>(args)...) {}

        /**
         * Called by the CRTP wrapper in order to ensure execution within the derived class (this). \n
         * For the sake of easy debugging.
         */
        inline auto execute()
        {
            return std::apply(func_, v_args);
        }

        Func func_;
        std::tuple<std::decay_t<Args>...> v_args;
        const char* function_name = nullptr;
        const char* file_name = nullptr;
        const char* called_from = nullptr;
        const char* function_type = nullptr;
        unsigned int line_number = 0;
        unsigned int column = 0;
    };

    /**
     * For the sake of easy compiler type deduction with Job submission
     * @tparam Func function pointer
     * @tparam Args variadic arguments 
     * @param name name of the function
     * @return 
     */
    template <typename Func, typename... Args>
    Job(const char* name, Func, Args...) -> Job<Func, Args...>;

    /**
     * \b Usage: Used to submit tasks to the job system
     * @tparam Func compiler will automatically attempt to deduce the function type
     * @tparam Args compiler will automatically attempt to deduce variadic arguments-- pass as many as the function pointer needs
     * @param task_info Wrapper around function and function arguments-- as well as debug information
     * @param source_location automatically captures where the task is submitted for debugging
     */
    template <typename Func, typename... Args>
    void SubmitJob(Job<Func, Args...>&& task_info, 
                     const std::source_location& source_location = std::source_location::current())
    {
        task_info.file_name = source_location.file_name();
        task_info.called_from = source_location.function_name();
        task_info.function_type = typeid(Func).name();
        task_info.line_number = source_location.line();
        task_info.column = source_location.column();
        //enqueue into task
        //task.push_back(std::make_unique<TaskDecl<Func, Args...>>(std::move(task_info)));
    }

    /**
     * Used by worker threads to execute a job I think
     * @param job -- job to be executed
     */
    void ExecuteJob(JobSystem::Internal::JobInterface&& job)
    {
        // i guess push result to the spot in the queue where it was done or...?
        job.Execute();
    }

}