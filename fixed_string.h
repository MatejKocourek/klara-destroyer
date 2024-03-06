#pragma once
#include <string_view>
#include <cstring>
#include <ostream>

template <size_t size>
class fixed_string {
	std::array<char, size> data;


public:


	//fixed_string()
	//{
	//	data[0] = '\0';
	//}
	fixed_string(const char* string = "")
	{
		assert(strlen(string) < size);
		std::strcpy(data.data(),string);
	}
	fixed_string(size_t count, char ch)
	{
		reserve(count);
		std::fill_n(data.begin(), count, ch);
		data[count] = '\0';
	}
	constexpr size_t capacity()
	{
		return size;
	}
	constexpr size_t reserve(size_t newCapacity)
	{
		if (newCapacity >= size) [[unlikely]]
			throw std::length_error("Cannot reserve this size on a static sized string");
	}

	char& operator[](size_t pos) {
		return data[pos];
	}
	const char& operator[](size_t pos) const {
		return data[pos];
	}
	std::string_view to_string_view()
	{
		return std::string_view(c_str());
	}
	const char* c_str() const
	{
		return data.data();
	}

	static friend std::ostream& operator<<(std::ostream& os, const fixed_string& x) {
		return os << x.c_str();
	}

};