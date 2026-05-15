#pragma once
#include <SDL3/SDL.h>
#include <utility>
#include <vector>
#include <unordered_map>

template<typename T>
class CSDLAllocator {
public:
	typedef T value_type;

	CSDLAllocator() noexcept = default;

	template<typename U>
    CSDLAllocator(const CSDLAllocator<U>& other) noexcept {}

    template<typename U>
    struct rebind {
        using other = CSDLAllocator<U>;
    };

	constexpr T* allocate(usize size) {
		return reinterpret_cast<T*>(SDL_malloc(sizeof(T) * size));
	}

	constexpr T* allocate(usize size, const void* hint) {
		return allocate(size); // don't know what to do with this yet.
	}

	constexpr void deallocate(T* p, usize size) noexcept {
		if (p == nullptr) return;
		SDL_free(p);
	}
};

template<typename T, typename U>
bool operator==(const CSDLAllocator<T>& lhs, const CSDLAllocator<U>& rhs) noexcept {
	return true;
}

template<typename T, typename U>
bool operator!=(const CSDLAllocator<T>& lhs, const CSDLAllocator<U>& rhs) noexcept {
	return !(lhs == rhs);
}

template<typename T, usize ALIGN = 4>
class CAlignedSDLAllocator {
public:
	typedef T value_type;

	CAlignedSDLAllocator() noexcept = default;

	template<typename U>
    CAlignedSDLAllocator(const CAlignedSDLAllocator<U, ALIGN>& other) noexcept {}

	template<typename U>
    struct rebind {
        using other = CAlignedSDLAllocator<U, ALIGN>;
    };

	constexpr T* allocate(usize size) {
		return reinterpret_cast<T*>(SDL_aligned_alloc(ALIGN, sizeof(T) * size));
	}

	constexpr T* allocate(usize size, const void* hint) {
		return allocate(size); // don't know what to do with this yet.
	}

	constexpr void deallocate(T* p, usize size) noexcept {
		if (p == nullptr) return;
		SDL_aligned_free(p);
	}
};

template<typename T, typename U, usize ALIGN>
bool operator==(const CAlignedSDLAllocator<T, ALIGN>& lhs, const CAlignedSDLAllocator<U, ALIGN>& rhs) {
	return true;
}

template<typename T, typename U, usize ALIGN>
bool operator!=(const CAlignedSDLAllocator<T, ALIGN>& lhs, const CAlignedSDLAllocator<U, ALIGN>& rhs) {
	return !(lhs == rhs);
}

template<typename T, typename ALLOC = CAlignedSDLAllocator<T, DEFAULT_ALIGNMENT>>
using TDynArray = std::vector<T, ALLOC>;

template<typename T, usize ALIGN = 4>
using AlignedArray_ = std::vector<T, CAlignedSDLAllocator<T, ALIGN>>;

template<typename Key, typename T, typename Alloc = CSDLAllocator<std::pair<const Key, T>>>
using Map_ = std::unordered_map<
	Key,
	T,
	std::hash<Key>,
	std::equal_to<Key>,
	Alloc
>;