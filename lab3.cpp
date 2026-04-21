#include <iostream>
#include <vector>
#include <thread>
#include <future>
#include <atomic>
#include <chrono>
#include <mutex>

std::mutex cout_mutex;

std::atomic<int> completedTasks(0); //атомарный счётчик завершённых задач


unsigned long long factorial(int n) {
    unsigned long long result = 1;
    for (int i = 1; i <= n; ++i)
        result *= i;
        std::this_thread::sleep_for(std::chrono::microseconds(10));

    completedTasks.fetch_add(1, std::memory_order_release); //+1 задача завершена

    return result;
}


void monitor(int totalTasks, std::promise<void> completionSignal) {
    while (completedTasks.load(std::memory_order_acquire) < totalTasks) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << "Монитор: все задачи завершены.\n";
    }

    completionSignal.set_value();
}

int main() {
    const int N = 5;

    std::vector<std::thread> threads;
    std::vector<std::future<unsigned long long>> futures;

    std::cout << "Генерация " << N << " задач...\n";


    for (int i = 1; i <= N; ++i) {
        std::packaged_task<unsigned long long(int)> task(factorial);

        futures.push_back(task.get_future());

        threads.emplace_back(std::move(task), i + 5);
    }

    std::promise<void> systemPromise;
    std::future<void> systemFuture = systemPromise.get_future();

    std::thread monitorThread(monitor, N, std::move(systemPromise));

    //получение результатов
    for (int i = 0; i < N; ++i) {
        {
            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cout << "Ожидание результата задачи " << i + 1 << "...\n";
        }
        unsigned long long result = futures[i].get(); //блокирует
        {
            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cout << "Результат: " << result << "\n";
        }
    }

    {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << "Основной поток ожидает сигнал от монитора...\n";
    }
    systemFuture.get();
    {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << "Все задачи полностью обработаны.\n";
    }


    for (auto& t : threads)
        t.join();

    monitorThread.join();

    return 0;
}