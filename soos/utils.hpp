#pragma once


template<typename T> static T min(T l, T r)
{
    return l <= r ? l : r;
}

template<typename T> static T max(T l, T r)
{
    return l >= r ? l : r;
}
