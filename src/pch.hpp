#pragma once
#ifdef __cplusplus
#include <array>
#include <algorithm>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <iostream>
#include <span>
#include <source_location>
#include <string>
#include <variant>
#include <bitset>
#include "hal/hal.hpp"

#define PRED(X) [](auto const& lhs, auto const& rhs) {return X;}
#define PAIR2(T) std::pair<T,T>
static void _assert(const char* cond_s, const char* fmt = "", auto ...args) {
	static char _assert_msg_buffer[1024];
	int p = sprintf(_assert_msg_buffer, "Assertion failed: %s\n", cond_s);
	sprintf(_assert_msg_buffer + p, fmt, args...);
#ifdef _WIN32
	MessageBoxA(NULL, _assert_msg_buffer, "Error", MB_ICONERROR);
#else
	std::wcerr << _assert_msg_buffer << std::endl;
#endif
	exit(1);
}
#define ASSERT(cond, ...) if (!(cond)) _assert(#cond, __VA_ARGS__);
// https://stackoverflow.com/a/22713396
template<typename T, size_t N> constexpr size_t extent_of(T(&)[N]) { return N; };
#define DEFINE_DATA_ARRAY(T, NAME, ...) T NAME[] = {__VA_ARGS__}; constexpr size_t NAME##_size = extent_of(NAME);
#define EXTERN_DATA_ARRAY(T, NAME) extern T NAME[]; extern const size_t NAME##_size;
// C++ Weekly - Ep 440 - Revisiting Visitors for std::visit - https://www.youtube.com/watch?v=et1fjd8X1ho
// This also demonstrates default visitor behavior for unhandled types
template<typename Arg, typename... T> concept none_invocable = (!std::is_invocable<T, Arg&>::value && ...);
template<typename... T> struct visitor : T... {
	using T::operator()...;
	template<typename Arg> requires none_invocable<Arg, T...> auto operator()(Arg&) {
		/* nop */
	};
};
// Fixed size vector
template<typename T, size_t Size> class fixed_vector {
	std::array<T, Size> _data{};
	size_t _size{ 0 };
public:	
	inline fixed_vector() = default;
	inline explicit fixed_vector(const T* data, const size_t size) {
		memcpy(this->data(), data, size);		
		resize(size);
	}
	
	inline void clear() { _size = 0; }
	inline void push_back(const T& value) { _data[_size++] = value; }
	inline T& operator[](size_t index) { return _data[index]; }
	
	inline std::array<T, Size>::iterator begin() { return _data.begin(); }
	inline std::array<T, Size>::iterator end() { return _data.begin() + _size; }
	inline std::array<T, Size>::iterator end_max() { return _data.end(); }

	inline std::span<T> span() { return { begin(), end() }; }
	inline std::span<T> span_max() { return { begin(), end_max() }; }

	inline T* data() { return _data.data(); }
	inline size_t size() { return _size; }
	inline void resize(size_t size) { _size = size; }
};
// Column major matrix
template<typename T, size_t Rows, size_t Cols> class fixed_matrix {
	using column_type = fixed_vector<T, Cols>;
	using row_type = std::array<column_type, Rows>;
	row_type _data{};
	size_t _size{ Rows };
public:
	inline column_type& operator[](size_t row) { return _data[row]; }

	inline row_type::iterator begin() { return _data.begin(); }
	inline row_type::iterator end() { return _data.begin() + _size; }
	inline row_type::iterator end_max() { return _data.end(); }

	inline std::span<column_type> span() { return { begin(), end() }; }
	inline std::span<column_type> span_max() { return { begin(), end_max() }; }

	inline size_t size() { return _size; }
	inline void resize(size_t size) { _size = size; }
};
#endif // !__cplusplus