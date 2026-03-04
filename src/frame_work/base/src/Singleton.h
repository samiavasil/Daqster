#ifndef SINGLETON_H
#define SINGLETON_H
#include <cassert>

template < class T>
class Singleton
{
public:
    static T* instance(){
        static T* g_instance = nullptr;
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
};

#endif // SINGLETON_H
