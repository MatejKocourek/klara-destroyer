#pragma once
#include <string_view>
#include <cstring>
#include <ostream>

template <size_t _capacity>
class stack_string {
	using fitting_t =
		std::conditional_t<_capacity <= std::numeric_limits<uint8_t>::max(), uint8_t,
		std::conditional_t<_capacity <= std::numeric_limits<uint16_t>::max(), uint16_t,
		std::conditional_t<_capacity <= std::numeric_limits<uint32_t>::max(), uint32_t,
		uint64_t>>>;

	fitting_t _size;
	std::array<char, _capacity> _data;
public:

	stack_string() : _size(0)
	{
	}
	template <size_t N>
	stack_string(const stack_string<N>& str)
	{
		reserve(str.size());
		_size = str.size();
		std::memcpy(_data.data(), str._data.data(), str.size());
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
	char* data()
	{
		return _data.data();
	}
	const char* data() const
	{
		return _data.data();
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
		[[assume (_size <= _capacity)]]
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
	constexpr operator std::string_view() const {
		return std::string_view(_data.data(), size());
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
		assert(size() < max_size());
		_data[size++] = c;
	}
	constexpr void pop_back()
	{
		assert(size() != 0);
		--_size;
	}

	constexpr bool starts_with(const std::string_view sv) const noexcept {
		return std::string_view(*this).starts_with(std::move_if_noexcept(sv));
	}
	template <size_t N>
	constexpr bool starts_with(const stack_string<N>& str) const noexcept {
		return std::string_view(*this).starts_with(std::string_view(str));
	}
	constexpr bool starts_with(char c) const noexcept {
		return std::string_view(*this).starts_with(c);
	}
	constexpr bool starts_with(const char* c_str) const {
		return std::string_view(*this).starts_with(c_str);
	}

	constexpr bool ends_with(const std::string_view sv) const noexcept {
		return std::string_view(*this).ends_with(std::move_if_noexcept(sv));
	}
	template <size_t N>
	constexpr bool ends_with(const stack_string<N>& str) const noexcept {
		return std::string_view(*this).ends_with(std::string_view(str));
	}
	constexpr bool ends_with(char c) const noexcept {
		return std::string_view(*this).ends_with(c);
	}
	constexpr bool ends_with(const char* c_str) const {
		return std::string_view(*this).ends_with(c_str);
	}

	constexpr bool contains(const std::string_view sv) const noexcept {
		return std::string_view(*this).contains(std::move_if_noexcept(sv));
	}
	template <size_t N>
	constexpr bool contains(const stack_string<N>& str) const noexcept {
		return std::string_view(*this).contains(std::string_view(str));
	}
	constexpr bool contains(char c) const noexcept {
		return std::string_view(*this).contains(c);
	}
	constexpr bool contains(const char* c_str) const {
		return std::string_view(*this).contains(c_str);
	}

	static friend std::ostream& operator<<(std::ostream& os, const stack_string& x) {
		return os << std::string_view(x);
	}
	template <size_t N>
	static friend bool operator==(const stack_string& lhs, const stack_string<N>& rhs) noexcept {
		return std::string_view(lhs) == std::string_view(rhs);
	}
	static friend bool operator==(const stack_string& lhs, const std::string_view& rhs) noexcept {
		return std::string_view(lhs) == rhs;
	}
	static friend bool operator==(const std::string_view& lhs, const stack_string& rhs) noexcept {
		return lhs == std::string_view(rhs);
	}
	static friend bool operator==(const stack_string& lhs, const std::string& rhs) noexcept {
		return std::string_view(lhs) == rhs;
	}
	static friend bool operator==(const std::string& lhs, const stack_string& rhs) noexcept {
		return lhs == std::string_view(rhs);
	}


};