#pragma once
#include <utility>
#include <type_traits>
#include <memory>
#include <stdexcept>


template <typename T, size_t N>
class static_vector
{
private:
	std::aligned_storage_t<sizeof(T)* N, alignof(T)> _buffer;
	size_t _size;

	/// <summary>
	/// Fill this vector from another vector. Number of elements that should be copied/moved must be >= _size and <= _capacity.
	/// </summary>
	void fillFromAnother(static_vector&& source)
	{
		//Reassigning new elements to already existing elements in current vector (reusing allocated objects without destroying them).
		for (size_t i = 0; i < _size; i++)
			data()[i] = std::move(source.data()[i]);
		//Creating the rest of elements in-place, increasing size of the vector. Should exception be thrown, the vector remains in a working state.
		for (; _size < source._size; _size++)
			new (&data()[_size]) T(std::move(source.data()[_size]));
	}
	void fillFromAnother(const static_vector& source)
	{
		for (size_t i = 0; i < _size; i++)
			data()[i] = source.data()[i];
		for (; _size < source._size; _size++)
			new (&data()[_size]) T(source.data()[_size]);
	}

	constexpr bool isSmall() const
	{
		return true;
	}

	//Freeing all allocated memory and destroying everything. Caller is resposible for restoring vector to a working state, if necessary.
	void clearMem()
	{
		clear();
	}


public:
	static_vector() : _size(0) {}

	static_vector(const static_vector& source) : _size(0)
	{
		try
		{
			fillFromAnother(source);
		}
		catch (...)
		{
			clearMem();
			throw;
		}
	}

	static_vector(static_vector&& source) : _size(0)
	{
		fillFromAnother(std::move(source));
		source.clear();
	}

	static_vector(size_t count, const T& value) : _size(0)
	{
		for (size_t i = 0; i < count; ++i)
		{
			push_back(value);
		}
	}
	static_vector(size_t count) : _size(0)
	{
		for (size_t i = 0; i < count; ++i)
		{
			emplace_back();
		}
	}

	static_vector(std::initializer_list<T> ilist) : _size(0)
	{
		for (auto& i : ilist)
		{
			push_back(std::move(i));
		}
	}

	static_vector& operator=(std::initializer_list<T> ilist)
	{
		clear();
		for (auto& i : ilist)
		{
			push_back(std::move(i));
		}
	}

	static_vector& operator=(const static_vector& source) {
		if (this != &source) [[likely]]
		{
			fillFromAnother(source);
		}
		return *this;
	}


	static_vector& operator=(static_vector&& source)
	{
		if (this != &source) [[likely]]
		{
			fillFromAnother(std::move(source));
			source.clear();
		}
		return *this;
	}

	~static_vector() {
		clearMem();
	}

	constexpr size_t capacity_sbo() const {
		return N;
	}

	T* data() {
		return reinterpret_cast<T*>(&_buffer);
	}
	const T* data() const {
		return reinterpret_cast<const T*>(&_buffer);
	}

	T& operator[](size_t pos) {
		return data()[pos];
	}
	const T& operator[](size_t pos) const {
		return data()[pos];
	}

	T& at(size_t pos) {
		if (!(pos < size())) [[unlikely]]
			throw std::out_of_range("Subscript out of range");
		else [[likely]]
			return data()[pos];
	}

	const T& at(size_t pos) const {
		if (!(pos < size())) [[unlikely]]
			throw std::out_of_range("Subscript out of range");
		else [[likely]]
			return data()[pos];
	}

	T& back() {
		return data()[size() - 1];
	}

	const T& back() const {
		return data()[size() - 1];
	}

	T& front() {
		return *data();
	}

	const T& front() const {
		return *data();
	}

	size_t size() const noexcept {
		[[assume(_size <= N)]];
		return _size;
	}

	bool empty() const noexcept {
		return size() == 0;
	}

	constexpr size_t max_size() const noexcept {
		return N;
	}

	constexpr size_t capacity() const noexcept {
		return max_size();
	}

	void reserve(size_t newCapacity) {
		if (newCapacity > N) [[unlikely]]
			throw std::length_error("Cannot reserve this size on a static sized vector");
	}

	constexpr void shrink_to_fit() {
		//Nothing to do here
	}

	void push_back(const T& element) {
		emplace_back(element);
	}
	void push_back(T&& element) {
		emplace_back(std::move(element));
	}

	template <typename... Ts>
	void emplace_back(Ts&&... param) {

		//if constexpr (_DEBUG)
		reserve(size() + 1);

		new (&data()[_size]) T(std::forward<Ts>(param)...);
		++_size;
	}

	void pop_back()
	{
		std::destroy_at(&data()[_size - 1]);
		--_size;
	}

	void clear()
	{
		for (size_t i = 0; i < _size; ++i)
			std::destroy_at(&data()[i]);

		_size = 0;
	}


	struct Iterator
	{
		using value_type = T;
		using reference = value_type&;
		using pointer = value_type*;
		using iterator_category = std::random_access_iterator_tag;
		using difference_type = std::ptrdiff_t;
		using iterator_concept = std::contiguous_iterator_tag;

		constexpr Iterator(T* iter = nullptr) : m_iterator{ iter } {}

		constexpr bool operator==(const Iterator& other) const noexcept { return m_iterator == other.m_iterator; }
		constexpr bool operator!=(const Iterator& other) const noexcept { return m_iterator != other.m_iterator; }
		constexpr reference operator*() const noexcept { return *m_iterator; }
		constexpr pointer operator->() const noexcept { return m_iterator; }
		constexpr Iterator& operator++() noexcept { ++m_iterator; return *this; }
		constexpr Iterator operator++(int) noexcept { Iterator tmp(*this); ++(*this); return tmp; }
		constexpr Iterator& operator--() noexcept { --m_iterator; return *this; }
		constexpr Iterator operator--(int) noexcept { Iterator tmp(*this); --(*this); return tmp; }
		constexpr Iterator& operator+=(const difference_type other) noexcept { m_iterator += other; return *this; }
		constexpr Iterator& operator-=(const difference_type other) noexcept { m_iterator -= other; return *this; }
		constexpr Iterator operator+(const difference_type other) const noexcept { return Iterator(m_iterator + other); }
		constexpr Iterator operator-(const difference_type other) const noexcept { return Iterator(m_iterator - other); }
		constexpr Iterator operator+(const Iterator& other) const noexcept { return Iterator(*this + other.m_iterator); }
		constexpr difference_type operator-(const Iterator& other) const noexcept { return m_iterator - other.m_iterator; }
		constexpr reference operator[](std::size_t index) const { return m_iterator[index]; }
		constexpr bool operator<(const Iterator& other) const noexcept { return m_iterator < other.m_iterator; }
		constexpr bool operator>(const Iterator& other) const noexcept { return m_iterator > other.m_iterator; }
		constexpr bool operator<=(const Iterator& other) const noexcept { return m_iterator <= other.m_iterator; }
		constexpr bool operator>=(const Iterator& other) const noexcept { return m_iterator >= other.m_iterator; }

	private:
		T* m_iterator;
	};

	Iterator begin() {
		return Iterator(&data()[0]);
	}
	Iterator end() {
		return Iterator(&data()[size()]);
	}



	struct ConstantIterator
	{
		using iterator_category = std::contiguous_iterator_tag;
		using difference_type = std::ptrdiff_t;
		using value_type = T;
		using pointer = T*;
		using reference = T&;

		ConstantIterator(const T* ptr) : m_ptr(ptr) {}

		const T& operator*() const { return *m_ptr; }
		const T* operator->() { return m_ptr; }
		ConstantIterator& operator++() { ++m_ptr; return *this; }
		ConstantIterator operator++(int) { ConstantIterator tmp = *this; ++(*this); return tmp; }
		friend bool operator== (const ConstantIterator& a, const ConstantIterator& b) { return a.m_ptr == b.m_ptr; };
		friend bool operator!= (const ConstantIterator& a, const ConstantIterator& b) { return a.m_ptr != b.m_ptr; };

	private:
		const T* m_ptr;
	};

	ConstantIterator cbegin() {
		return ConstantIterator(&data()[0]);
	}
	ConstantIterator cend() {
		return ConstantIterator(&data()[size()]);
	}

	ConstantIterator begin() const {
		return ConstantIterator(&data()[0]);
	}
	ConstantIterator end() const {
		return ConstantIterator(&data()[size()]);
	}
};
