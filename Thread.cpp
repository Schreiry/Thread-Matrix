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

// ===== Глобальные переменные =====
int currentN = 100;  // Начальный размер матриц (минимум 10x10)
vector<vector<int>> globalA;
vector<vector<int>> globalB;
vector<vector<int>> globalC;

// ===== Мьютексы и условные переменные =====
std::mutex mtx;
std::condition_variable cv;
bool matrixReady = false;   // Флаг: новые матрицы готовы к умножению
int completedMultiplications = 0; // Счётчик завершённых потоков
const int numMultiplicationThreads = 6;

// ===== Функция потока таймера =====
// Поток, который каждую секунду выводит время, прошедшее с начала умножения.
void timerThreadFunction(std::atomic<bool>& finished, std::chrono::steady_clock::time_point startTime) {
    while (!finished.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
        std::cout << YELLOW << "Timer: " << elapsed << " seconds passed..." << RESET << std::endl;
    }
}

// ===== Функция рабочего потока для умножения =====
// Каждый поток ждёт сигнала о готовности матриц, затем вычисляет свой диапазон строк результирующей матрицы.
void multiplicationWorker(int threadId) {
    while (true) {
        // Ожидание, пока матрицы не будут сгенерированы
        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [] { return matrixReady; });
        }
        int N = currentN;
        // Разбиение по строкам: делим матрицу на горизонтальные полосы
        int rowsPerThread = N / numMultiplicationThreads;
        int startRow = threadId * rowsPerThread;
        int endRow = (threadId == numMultiplicationThreads - 1) ? N : startRow + rowsPerThread;

        // Наивное умножение матриц: C = A * B
        // (Для повышения эффективности здесь можно заменить алгоритм на метод с асимптотикой O(n^2.3728596))
        for (int i = startRow; i < endRow; ++i) {
            for (int j = 0; j < N; ++j) {
                int sum = 0;
                for (int k = 0; k < N; ++k) {
                    sum += globalA[i][k] * globalB[k][j];
                }
                globalC[i][j] = sum;
            }
        }

        // Вывод сообщения об окончании работы потока
        {
            std::lock_guard<std::mutex> lock(mtx);
            std::cout << BLUE << "Thread " << threadId
                << RESET << " finished processing lines with " << startRow
                << " By " << endRow - 1 << "." << std::endl;
            completedMultiplications++;
            if (completedMultiplications == numMultiplicationThreads) {
                cv.notify_one(); // Уведомляем главный поток, что все потоки завершили вычисления
            }
        }

        // Ожидание сигнала о начале нового раунда
        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [] { return !matrixReady; });
        }
    }
}

int main() {
    // Создаем постоянные потоки для умножения
    vector<std::thread> workers;
    for (int i = 0; i < numMultiplicationThreads; ++i) {
        workers.push_back(std::thread(multiplicationWorker, i));
    }

    // Главный цикл: генерация матриц, умножение, вывод результатов и увеличение размера
    while (true) {
        // ===== Генерация матриц (поток-генератор) =====
        {
            std::lock_guard<std::mutex> lock(mtx);
            std::cout << GREEN << "\n Generator: Creating matrices of size "
                << currentN << "X" << currentN << RESET << std::endl;
            // Заполняем матрицу A единицами, а B – двойками
            globalA.assign(currentN, vector<int>(currentN, 1));
            globalB.assign(currentN, vector<int>(currentN, 2));
            globalC.assign(currentN, vector<int>(currentN, 0));
            // Сброс счетчика и установка флага готовности
            completedMultiplications = 0;
            matrixReady = true;
        }
        cv.notify_all(); // Сигнал для рабочих потоков: можно начинать умножение

        // ===== Запуск потока таймера =====
        std::atomic<bool> roundFinished(false);
        auto startTime = std::chrono::steady_clock::now();
        std::thread timerThread(timerThreadFunction, std::ref(roundFinished), startTime);

        // ===== Ожидание завершения всех потоков умножения =====
        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [] { return completedMultiplications == numMultiplicationThreads; });
        }

        // Завершаем работу потока таймера
        roundFinished.store(true);
        timerThread.join();

        // Подсчет времени выполнения
        auto endTime = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed = endTime - startTime;

        // ===== Вывод итогового результата =====
        int inputElements = currentN * currentN * 2; // Общее число элементов в матрицах A и B
        int outputElements = currentN * currentN;     // Число элементов в результирующей матрице C
        std::cout << PURPLE << "\n=============================================" << RESET << std::endl;
        std::cout << CYAN << "Calculation complete!" << RESET << std::endl;
        std::cout << CYAN << "Matrix size: " << currentN << "x" << currentN << RESET << std::endl;
        std::cout << CYAN << "Input elements: " << inputElements
            << ", at the exit: " << outputElements << RESET << std::endl;
        std::cout << CYAN << "Computation time: " << elapsed.count() << " Seconds" << RESET << std::endl;
        std::cout << PURPLE << "=============================================\n" << RESET << std::endl;

        // Сброс флага для следующего раунда (работающие потоки ожидают нового сигнала)
        {
            std::lock_guard<std::mutex> lock(mtx);
            matrixReady = false;
        }
        cv.notify_all(); // Разблокировать потоки, чтобы они перешли в режим ожидания нового раунда

        // ===== Увеличение размера матриц в геометрической прогрессии =====
        currentN *= 2;

        // Небольшая задержка между раундами для наглядности
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    // Присоединяем рабочие потоки (код недостижим, так как цикл бесконечный, но для завершения программы – хорошая практика)
    for (auto& worker : workers) {
        worker.join();
    }

    return 0;
}
