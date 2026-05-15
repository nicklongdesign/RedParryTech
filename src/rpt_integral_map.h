#pragma once
#include "prelude.h"
#include "rpt_allocators.h"

#include <utility>
#include <type_traits>

// A map that uses integral types only as keys (or rather more specifically anything that can be cast to an unsigned 64-bit integer)
// The purpose is that most things I will want to use as keys will be handles or pointers rather than actual collections of data
// This way I don't have to impliment hash for all the types I would normally use as keys
template<typename Key, typename Value, typename Alloc = CAlignedSDLAllocator<std::pair<u64, Value>, DEFAULT_ALIGNMENT>>
class TIntegralMap {
public:
	typedef std::pair<u64, Value> Entry;

	TIntegralMap() {}
	TIntegralMap(usize capacity) : m_capacity(capacity) {}
	bool contains(const Key& key) const {
		bool result = false;
		usize closest = find_nearest_index(key, result);
		return result;
	};

	const Value* get(const Key& key) const {
		bool isElementFound = false;
		usize index = find_nearest_index(key, isElementFound);
		if (isElementFound) {
			return &m_data[index].second;
		}

		return nullptr;
	}

	Value* get_mut(const Key& key) {
		return const_cast<Value*>(get(key));
	}

	bool try_add(const Key& key, const Value& value) {
		return internal_add(key, value);
	}

	void force_add(const Key& key, const Value& value) {
		internal_add(key, value, true);
	}

	bool try_remove(const Key& key) {
		bool isExactMatch = false;
		usize closest = find_nearest_index(key, isExactMatch);
		if (!isExactMatch) {
			return false;
		}

		const usize numElementsToMove = m_numElements - (closest + 1);
		const usize numBytesToMove = numElementsToMove * sizeof(Entry);
		Entry* temp = Alloc{}.allocate(numElementsToMove);
		memcpy(temp, &m_data[closest+1], numBytesToMove);
		memcpy(&m_data[closest], temp, numBytesToMove);
		Alloc{}.deallocate(temp, numElementsToMove);
		--m_numElements;
		return true;
	}

	bool try_set_capacity(usize newCapacity) {
		if (newCapacity <= m_capacity) { return false; } //???

		Entry* newData = Alloc{}.allocate(newCapacity);
		if (newData == nullptr) return false;

		memcpy(newData, m_data, m_capacity * sizeof(Entry));
		Alloc{}.deallocate(m_data, m_capacity);
		m_data = Recast_<Entry*>(newData);
		m_capacity = newCapacity;
		return true;
	}

	usize length() const {
		return m_numElements;
	}

	usize size() const {
		return m_numElements;
	}

	const Value* at_index(usize index) const {
		if (index >= m_numElements) return nullptr;

		return &m_data[index].second;
	}

	Value* at_index_mut(usize index) {
		return const_cast<Value*>(at_index(index));
	}

private:
	usize m_capacity = 0;
	usize m_numElements = 0;
	Entry* m_data = nullptr;

	bool internal_add(const Key& key, const Value& value, bool forceAdd = false) {
		bool isExactMatch = false;
		usize insertionIndex = find_nearest_index(key, isExactMatch);
		if (isExactMatch) {
			if (forceAdd) {
				m_data[insertionIndex].second = value;
				return true;
			}

			return false;
		}

		// Grow the map if we need to
		if (m_capacity <= m_numElements + 1) {
			const usize newCapacity = m_capacity == 0 ? 2 : m_capacity * 2;
			if (!try_set_capacity(newCapacity)) {
				return false;
			}
		}

		const usize elementsToMove = m_numElements - insertionIndex;
		if (elementsToMove > 0) {
			const usize bytesToMove = sizeof(Entry) * elementsToMove;
			Entry* temp = Alloc{}.allocate(elementsToMove);
			memcpy(temp, &m_data[insertionIndex], bytesToMove);
			memcpy(&m_data[insertionIndex+1], temp, bytesToMove);
			Alloc{}.deallocate(temp, elementsToMove);
		}

		m_data[insertionIndex] = Entry(static_cast<u64>(key), value);
		++m_numElements;
		return true;
	}

	usize find_nearest_index(const Key& key, bool& isExactMatch) const {
		isExactMatch = false;
		if (m_numElements == 0) {
			return 0;
		}

		const u64 toFindKey = static_cast<u64>(key);

		if (m_numElements == 1) {
			isExactMatch = m_data[0].first == toFindKey;
			return toFindKey > m_data[0].first ? 1 : 0;
		}

		isize lowIndex = 0;
		isize highIndex = static_cast<isize>(m_numElements - 1); // we'll risk the overflow (I don't think we should ever have a map with 2^32 members in it)
		isize testIndex = (lowIndex + highIndex) >> 1; // divide by 2 shortcut
		while(true) {
			u64 testKey = m_data[testIndex].first;
			if (testKey == toFindKey) {
				isExactMatch = true;
				return static_cast<usize>(testIndex);
			}

			if (toFindKey > testKey) {
				lowIndex = testIndex + 1;
			}
			else if(toFindKey < testKey) {
				highIndex = testIndex - 1;
			}

			if (lowIndex >= highIndex || highIndex <= 0 || lowIndex >= m_numElements) {
				break;
			}

			testIndex = (lowIndex + highIndex) >> 1;
		}

		u64 foundKey = m_data[testIndex].first;
		usize insertionIndex = toFindKey > foundKey
			? testIndex + 1
			: testIndex;

		isExactMatch = insertionIndex < m_numElements
			? m_data[insertionIndex].first == toFindKey
			: false;

		return insertionIndex;
	}
};
