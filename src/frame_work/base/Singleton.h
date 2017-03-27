#ifndef SINGLETON_H
#define SINGLETON_H
#include<assert.h>

template < class T>
class Singleton
{
public:
    static T* instance(){
        if(  NULL == g_instance ){
            g_instance = new T();
        }
        assert( g_instance != NULL );
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

template < class T> T* Singleton<T>::g_instance =  NULL;

#endif // SINGLETON_H
