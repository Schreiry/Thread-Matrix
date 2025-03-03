// Directive Space : 


#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <atomic>



// /// /// /// /// /// /// 

using std::string;
using std::vector;


// /// /// /// /// /// ///



// ::::::::: Цвета ::::::::::
string RED = "\033[1;31m";
string GREEN = "\033[1;32m";
string YELLOW = "\033[1;33m";
string BLUE = "\033[1;34m";
string PURPLE = "\033[1;35m";
string CYAN = "\033[1;36m";
string RESET = "\033[0m";

// ===== Global variables =====
int currentN = 100;  
vector<vector<int>> globalA;
vector<vector<int>> globalB;
vector<vector<int>> globalC;

// ===== Mutexes and condition variables =====
std::mutex mtx;
std::condition_variable cv;
bool matrixReady = false;   // Flag: new matrices are ready for multiplication
int completedMultiplications = 0; // Counter of completed threads
const int numMultiplicationThreads = 6;

// ===== Timer Thread Function =====
// A stream that prints out the time elapsed since the start of the multiplication every second.
void timerThreadFunction(std::atomic<bool>& finished, std::chrono::steady_clock::time_point startTime) {
    while (!finished.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
        std::cout << YELLOW << "Timer: " << elapsed << " seconds passed..." << RESET << std::endl;
    }
}

// ===== Workflow function for multiplication =====
// Each thread waits for the matrix readiness signal, then calculates its own range of rows of the resulting matrix.
void multiplicationWorker(int threadId) {
    while (true) {
        // Wait until matrices are generated
        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [] { return matrixReady; });
        }
        int N = currentN;
        // Row splitting: divide the matrix into horizontal stripes
        int rowsPerThread = N / numMultiplicationThreads;
        int startRow = threadId * rowsPerThread;
        int endRow = (threadId == numMultiplicationThreads - 1) ? N : startRow + rowsPerThread;

        // Naive matrix multiplication: C = A * B
        // (To improve efficiency, the algorithm here can be replaced with a method with O(n^2.3728596) asymptotics))
        for (int i = startRow; i < endRow; ++i) {
            for (int j = 0; j < N; ++j) {
                int sum = 0;
                for (int k = 0; k < N; ++k) {
                    sum += globalA[i][k] * globalB[k][j];
                }
                globalC[i][j] = sum;
            }
        }

        // Output a message about the end of the thread
        {
            std::lock_guard<std::mutex> lock(mtx);
            std::cout << BLUE << "Thread " << threadId
                << RESET << " finished processing lines with " << startRow
                << " By " << endRow - 1 << "." << std::endl;
            completedMultiplications++;
            if (completedMultiplications == numMultiplicationThreads) {
                cv.notify_one(); // Notify the main thread that all threads have completed their computations
            }
        }

        // Waiting for a signal to start a new round
        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [] { return !matrixReady; });
        }
    }
}

int main() {
    // Create constant streams for multiplication
    vector<std::thread> workers;
    for (int i = 0; i < numMultiplicationThreads; ++i) {
        workers.push_back(std::thread(multiplicationWorker, i));
    }

    // Main loop: generate matrices, multiply, print results and increase size
    while (true) {
        // ===== Matrix generation (stream generator) =====
        {
            std::lock_guard<std::mutex> lock(mtx);
            std::cout << GREEN << "\n Generator: Creating matrices of size "
                << currentN << "X" << currentN << RESET << std::endl;
            // Fill matrix A with ones and B with twos
            globalA.assign(currentN, vector<int>(currentN, 1));
            globalB.assign(currentN, vector<int>(currentN, 2));
            globalC.assign(currentN, vector<int>(currentN, 0));
            // Сброс счетчика и установка флага готовности
            completedMultiplications = 0;
            matrixReady = true;
        }
        cv.notify_all(); // Signal to worker threads: multiplication can begin

        // ===== Start the timer thread =====
        std::atomic<bool> roundFinished(false);
        auto startTime = std::chrono::steady_clock::now();
        std::thread timerThread(timerThreadFunction, std::ref(roundFinished), startTime);

        // ===== Wait for all multiplication threads to complete =====
        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [] { return completedMultiplications == numMultiplicationThreads; });
        }

        // Terminate the timer thread
        roundFinished.store(true);
        timerThread.join();

        // Calculate execution time
        auto endTime = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed = endTime - startTime;

        // ===== Output the final result =====
        int inputElements = currentN * currentN * 2; // Total number of elements in matrices A and B
        int outputElements = currentN * currentN;     // Number of elements in the resulting matrix C
        std::cout << PURPLE << "\n=============================================" << RESET << std::endl;
        std::cout << CYAN << "Calculation complete!" << RESET << std::endl;
        std::cout << CYAN << "Matrix size: " << currentN << "x" << currentN << RESET << std::endl;
        std::cout << CYAN << "Input elements: " << inputElements
            << ", at the exit: " << outputElements << RESET << std::endl;
        std::cout << CYAN << "Computation time: " << elapsed.count() << " Seconds" << RESET << std::endl;
        std::cout << PURPLE << "=============================================\n" << RESET << std::endl;

        // Reset flag for next round (running threads wait for new signal)
        {
            std::lock_guard<std::mutex> lock(mtx);
            matrixReady = false;
        }
        cv.notify_all(); // Unblock threads so they can wait for a new round

        // ===== Increasing the size of matrices in geometric progression =====
        currentN *= 2;

        // A small delay between rounds for clarity
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    // Attach worker threads (code is unreachable since the loop is infinite, but it's good practice to terminate the program)
    for (auto& worker : workers) {
        worker.join();
    }

    return 0;
}
