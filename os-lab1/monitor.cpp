#include <pthread.h>
#include <unistd.h>
#include <cstdio>

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
    // Поток-поставщик
    bool provide(int id)
    {
        pthread_mutex_lock(&mutex);
        if (ready)                      // предыдущее событие ещё не обработано
        {
            pthread_mutex_unlock(&mutex);
            return false;               // повторить попытку позже
        }
        Event* event = new Event(id);   // создать событие
        data = event;                   // записать данные в монитор
        ready = true;
        std::printf("Отправлено событие %d\n", event->id);
        pthread_cond_signal(&condition);
        pthread_mutex_unlock(&mutex);
        return true;
    }

    // Поток-потребитель
    void consume()
    {
        pthread_mutex_lock(&mutex);
        while (!ready)                  // защита от ложных пробуждений
            pthread_cond_wait(&condition, &mutex);
        Event* event = static_cast<Event*>(data);   // извлечь данные
        data = nullptr;
        ready = false;
        std::printf("Обработано событие %d\n", event->id);
        delete event;                   // обработать событие
        pthread_mutex_unlock(&mutex);
    }

private:
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t condition = PTHREAD_COND_INITIALIZER;
    bool ready = false;
    void* data = nullptr;
};

class Producer
{
public:
    static void* run(void* arg)
    {
        Monitor* monitor = static_cast<Monitor*>(arg);
        int id = 1;
        while (true)
        {
            sleep(1);
            if (monitor->provide(id))
                ++id;
        }
        return nullptr;
    }
};

class Consumer
{
public:
    static void* run(void* arg)
    {
        Monitor* monitor = static_cast<Monitor*>(arg);
        while (true)
            monitor->consume();
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
