#pragma once
#include <string_view>
#include <cstring>
#include <ostream>

template <size_t _capacity>
class stack_string {
	std::array<char, _capacity> _data;
	using fitting_t =
		std::conditional_t<_capacity <= std::numeric_limits<uint8_t>::max(), uint8_t,
		std::conditional_t<_capacity <= std::numeric_limits<uint16_t>::max(), uint16_t,
		std::conditional_t<_capacity <= std::numeric_limits<uint32_t>::max(), uint32_t,
		uint64_t>>>;

	fitting_t _size;
public:


	char* data()
	{
		return _data.data();
	}
	const char* data() const
	{
		return _data.data();
	}
	stack_string()
	{
		_size = 0;
	}
	stack_string(const char* string)
	{
		assert(strlen(string) < max_size());
		_size = strlen(string);
		std::memcpy(_data.data(), string, _size);
	}
	stack_string(size_t count, char ch)
	{
		reserve(count);
		_size = count;
		std::fill_n(_data.begin(), count, ch);
		//data[count] = '\0';
	}
	constexpr size_t capacity() const
	{
		return _capacity;
	}
	constexpr size_t max_size() const
	{
		return _capacity;
	}
	constexpr void shrink_to_fit() const
	{
		return;
	}
	constexpr size_t size() const
	{
		return _size;
	}
	constexpr size_t length() const
	{
		return _size;
	}
	constexpr size_t reserve(size_t newCapacity)
	{
		if (newCapacity > max_size()) [[unlikely]]
			throw std::length_error("Cannot reserve this size on a static sized string");
	}

	char& operator[](size_t pos) {
		assert(pos <= _size);
		return _data[pos];
	}
	const char& operator[](size_t pos) const {
		assert(pos <= _size);
		return _data[pos];
	}
	constexpr operator std::string_view() {
		return std::string_view(_data.data(),size());
	}
	//const char* c_str() const
	//{
	//	return data.data();
	//}
	constexpr bool empty() const
	{
		return size() == 0;
	}
	constexpr void clear()
	{
		_size = 0;
	}
	constexpr void push_back(char c)
	{
		assert(size() << max_size());
		_data[size++] = c;
	}
	constexpr void pop_back()
	{
		assert(size() != 0);
		--_size;
	}

	static friend std::ostream& operator<<(std::ostream& os, const stack_string& x) {
		return os << std::string_view(const_cast<stack_string&>(x));//TODO prasarna
	}
	static friend bool operator==(const stack_string& lhs, const stack_string& rhs) {
		if (lhs.size() != rhs.size())
			return false;
		for (size_t i = 0; i < lhs.size(); ++i)
		{
			if (lhs._data[i] != rhs._data[i])
				return false;
		}
	}
	static friend bool operator!=(const stack_string& lhs, const stack_string& rhs) {
		return !(lhs == rhs);
	}

};