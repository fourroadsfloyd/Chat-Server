#ifndef SINGLETON_H
#define SINGLETON_H

#include "const.h"

template<typename T>
class Singleton {
protected:
    Singleton() = default;

    Singleton(const Singleton<T>& other) = delete;
    Singleton& operator=(const Singleton<T>& other) = delete;

    static std::shared_ptr<T> _instance;    

public:
    static std::shared_ptr<T> GetInstance()
    {
        static std::once_flag once_flag;
        std::call_once(once_flag, [&]() {
            _instance = std::shared_ptr<T>(new T());  
            });

        return _instance;
    }

    ~Singleton() = default;
};

template<typename T>
std::shared_ptr<T> Singleton<T>::_instance = nullptr; 

#endif 