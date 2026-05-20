#pragma once
template <typename T>
struct VectorPool
{
    struct Deleter
    {
        void operator()(std::vector<T>* v) const
        {
            if (!v) return;
            v->clear();
            auto& instance = VectorPool<T>::GetSingleton();
            std::lock_guard<std::mutex> lock(instance.mtx);
            instance.pool.push(v);
        }
    };

    using PoolVector = std::unique_ptr<std::vector<T>, Deleter>;

    static VectorPool& GetSingleton()
    {
        static VectorPool instance;
        return instance;
    }
    ~VectorPool()
    {
        while (!pool.empty())
        {
            delete pool.top();
            pool.pop();
        }
    }
    VectorPool(const VectorPool&) = delete;
    VectorPool(VectorPool&&) = delete;
    VectorPool& operator=(const VectorPool&) = delete;
    VectorPool& operator=(VectorPool&&) = delete;
    PoolVector Rent()
    {
        std::lock_guard<std::mutex> lock(mtx);
        std::vector<T>* ptr;
        if (pool.empty())
        {
            ptr = new std::vector<T>();
        }
        else
        {
            ptr = pool.top();
            pool.pop();
        }
        return VectorPool::PoolVector(ptr);
    }
private:
    VectorPool() = default;
    std::mutex mtx;
    std::stack<std::vector<T>*> pool;
};
