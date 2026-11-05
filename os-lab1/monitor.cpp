#include <pthread.h>
#include <unistd.h>
#include <cstdio>

const int EVENTS = 10;   // количество событий до завершения

// Условное событие: несериализуемые данные, передаются по указателю
class Event
{
public:
    explicit Event(int id) : id(id) {}
    int id;
};

class Monitor
{
public:
    ~Monitor()
    {
        pthread_cond_destroy(&condition);
        pthread_mutex_destroy(&mutex);
    }

    // Поток-поставщик
    void provide(Event* event)
    {
        pthread_mutex_lock(&mutex);
        while (ready)                   // ждём, пока предыдущее событие обработают
            pthread_cond_wait(&condition, &mutex);
        pthread_mutex_unlock(&mutex);

        std::printf("Отправлено событие %d\n", event->id);

        pthread_mutex_lock(&mutex);
        data = event;                   // записать данные в монитор
        ready = true;
        pthread_cond_signal(&condition);
        pthread_mutex_unlock(&mutex);
    }

    // Поток-потребитель; возвращает false, когда монитор остановлен
    bool consume()
    {
        pthread_mutex_lock(&mutex);
        while (!ready && !stopping)     // защита от ложных пробуждений
            pthread_cond_wait(&condition, &mutex);
        if (!ready)                     // событий нет и поставщик закончил
        {
            pthread_mutex_unlock(&mutex);
            return false;
        }
        Event* event = static_cast<Event*>(data);   // извлечь данные
        data = nullptr;
        pthread_mutex_unlock(&mutex);

        std::printf("Обработано событие %d\n", event->id);
        delete event;                   // обработать событие

        pthread_mutex_lock(&mutex);
        ready = false;                  // обработка завершена
        pthread_cond_signal(&condition);
        pthread_mutex_unlock(&mutex);
        return true;
    }

    // Завершение монитора
    void stop()
    {
        pthread_mutex_lock(&mutex);
        while (ready)                   // дождаться обработки последнего события
            pthread_cond_wait(&condition, &mutex);
        stopping = true;
        pthread_cond_broadcast(&condition);
        pthread_mutex_unlock(&mutex);
    }

private:
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t condition = PTHREAD_COND_INITIALIZER;
    bool ready = false;
    bool stopping = false;
    void* data = nullptr;
};

class Producer
{
public:
    static void* run(void* arg)
    {
        Monitor* monitor = static_cast<Monitor*>(arg);
        for (int id = 1; id <= EVENTS; ++id)
        {
            sleep(1);
            monitor->provide(new Event(id));
        }
        monitor->stop();
        return nullptr;
    }
};

class Consumer
{
public:
    static void* run(void* arg)
    {
        Monitor* monitor = static_cast<Monitor*>(arg);
        while (monitor->consume())
            ;
        return nullptr;
    }
};

int main()
{
    Monitor monitor;
    pthread_t producer, consumer;

    pthread_create(&consumer, nullptr, Consumer::run, &monitor);
    pthread_create(&producer, nullptr, Producer::run, &monitor);

    pthread_join(producer, nullptr);
    pthread_join(consumer, nullptr);
    return 0;
}
