#pragma once

#include <memory>

class ArenaAllocator
{

public:
    explicit ArenaAllocator(const size_t bytes)
        : m_size{bytes},
          m_buffer{new std::byte[bytes]},
          m_offset{m_buffer}
    {
    }

    ArenaAllocator(const ArenaAllocator &t); // deleting copy constructor
    ArenaAllocator &operator=(const ArenaAllocator &) = delete;

    template <typename T>
    [[nodiscard]] T *alloc()
    {
        // calculate available allocated memory
        size_t rem_bytes = m_size - static_cast<size_t>(m_offset - m_buffer);
        auto ptr = static_cast<void *>(m_offset);
        // calculate alignment bytes for type T
        const auto aligned_address = std::align(alignof(T), sizeof(T), ptr, rem_bytes);
        if (aligned_address == nullptr)
        {
            throw std::bad_alloc{};
        }
        // moving offset to match the aligned bytes + size of T
        m_offset = static_cast<std::byte *>(aligned_address) + sizeof(T);
        return static_cast<T *>(aligned_address);
    }

    template <typename T, typename... Args>
    [[nodiscard]] T *emplace(Args &&...args)
    {
        // allocating memory for type T and placing its members
        const auto allocated_memory = alloc<T>();
        return new (allocated_memory) T{std::forward<Args>(args)...};
    }

    // destructor
    ~ArenaAllocator()
    {
        delete[] m_buffer;
    }

private:
    size_t m_size;       // size of allocated memory
    std::byte *m_buffer; // pointer to allocated memory
    std::byte *m_offset; // pointer to available allocated memory
};