#ifndef SINGLETON_H
#define SINGLETON_H
#include<assert.h>

template < class T>
class Singleton
{
public:
    static T* instance(){
        if(  nullptr == g_instance ){
            g_instance = new T();
        }
        assert( g_instance != nullptr );
        return g_instance;
    }

protected:
    Singleton();
    ~Singleton();
private:
    Singleton( Singleton const& );
    Singleton& operator= (Singleton const&);
    static T* g_instance;
};

template < class T> T* Singleton<T>::g_instance =  nullptr;

#endif // SINGLETON_H
