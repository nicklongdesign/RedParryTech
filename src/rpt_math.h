#pragma once
#include "prelude.h"
#include <SDL3/SDL.h>

namespace RPTMath {
	template <typename T>
	constexpr T max(T a, T b) { 
		return (a < b) ? b : a; 
	}
}