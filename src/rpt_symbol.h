class SSymbol {
public:
	SSymbol();
	explicit SSymbol(const char* string);
	SSymbol(const SSymbol& other);
	SSymbol(u64 inValue);

	explicit operator u64() const { return value; }

	inline SSymbol& operator=(const SSymbol& other) {
		this->value = static_cast<u64>(other);
		return *this;
	}

	static constexpr u64 hash(const char* str) {
		if (str[0] == '\0') return ~0ULL;

		// lazy implimentation https://stackoverflow.com/questions/7666509/hash-function-for-string
		// Switch to Jason's Matrix thing once I actually understand it
		u64 hash = 5381;
		i32 c;
		while (c = *str++) {
			hash = ((hash << 5) + hash) + c;
		}

		return hash;
	}

	inline bool operator==(const SSymbol& other) {
		return value == static_cast<u64>(other);
	}

	inline bool operator!=(const SSymbol& other) {
		return !(*this == other);
	}

private:
	u64 value;
};

struct SSymbolHash {
	u64 operator()(const SSymbol& s) const noexcept {
		return static_cast<u64>(s); // this already is a hash, this is for unordered_map compatibility
	}
};

// inline bool operator==(const SSymbol& lhs, const SSymbol& rhs) const {
// 	return static_cast<u64>(lhs) == static_cast<u64>(rhs);
// }

// inline bool operator!=(const SSymbol& lhs, const SSymbol& rhs) const {
// 	return !(lhs == rhs);
// }

const static SSymbol Invalid_ = SSymbol(~0ULL);

#define Sym_(str) (SSymbol(SSymbol::hash(str)))

typedef SSymbol symbol;