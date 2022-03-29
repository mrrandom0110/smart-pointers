#pragma once

#ifndef SMARTPOINTERS_IMPL_H
#define SMARTPOINTERS_IMPL_H

#endif //SMARTPOINTERS_IMPL_H

#include <memory>
#include <atomic>
#include <iostream>

template<typename T>
class weak_pointer;
struct base {
	virtual ~base() = default;
	virtual void destroy() = 0;
	std::atomic<long> weak_ref = 0;
	std::atomic<long> shared_ref = 0;
};

template<typename T>
struct ctrl_block_from_ptr: public base {
	ctrl_block_from_ptr(T* ptr): _ptr(ptr)
	{}

	virtual void destroy() override
	{
		delete _ptr;
		_ptr = nullptr;
	}

	~ctrl_block_from_ptr()
	{
		if (_ptr) destroy();
	}

	T* get()
	{
		return _ptr;
	}

	T* _ptr;
};

template<typename T>
struct ctrl_block_make_shared: public base {
	template<typename... Args>
	ctrl_block_make_shared(Args&&... args): _deleted(false)
	{
		new(&data) T(std::forward<Args> (args)...);
	}

	virtual void destroy() override
	{
		reinterpret_cast<T*> (&data)->~T();
		_deleted = true;
	}

	~ctrl_block_make_shared()
	{
		if (!_deleted) destroy();
	}

	T* get()
	{
		return reinterpret_cast<T*> (&data);
	}

	typename std::aligned_storage<sizeof(T), alignof(T)>::type data;
	bool _deleted;
};

// T* data в классе заменяет адресс aligned_storage

template<typename T>
class shared_pointer {
public:
	shared_pointer() noexcept
	{
		set(nullptr, nullptr);
	}

	shared_pointer(T* ptr) noexcept
	{
		ctrl_block = new ctrl_block_from_ptr<T> (ptr);
		set(ptr, ctrl_block);
		add_reference();
	}

	shared_pointer(const shared_pointer& other)
	{
		set(other.data, other.ctrl_block);
		add_reference();
	}

	shared_pointer& operator=(const shared_pointer& other)
	{
		shared_pointer<T> (other).swap(*this);
		add_reference();
		return *this;
	}

	shared_pointer(shared_pointer&& other) noexcept
	{
		set(other.data, other.ctrl_block);
		other.set(nullptr, nullptr);
	}

	shared_pointer& operator=(shared_pointer&& other) noexcept
	{
		shared_pointer(std::move(other)).swap(*this);
		return *this;
	}

	shared_pointer(const weak_pointer<T>& other)
	{
		shared_pointer<T>(other.lock()).swap(*this);
		add_reference();
	}

	~shared_pointer()
	{
		remove_reference();
	}

	void reset() noexcept
	{
		shared_pointer().swap(*this);
	}

	size_t use_count() const noexcept
	{
		return ctrl_block ? ctrl_block->shared_ref.load() : 0;
	}

	bool unique() const noexcept
	{
		return use_count() == 1;
	}

	T* get() const noexcept
	{
		return data;
	}

	T& operator*() const
	{
		return *data;
	}

	T* operator->() const
	{
		return data;
	}

	operator bool() const noexcept
	{
		return data != nullptr;
	}

	void internal_reset(T* ptr, base* _ctrl_block)
	{
		remove_reference();
		set(ptr, _ctrl_block);
		add_reference();
	}

	friend class weak_pointer<T>;

private:
	base* ctrl_block;
	T* data;
	void swap(shared_pointer& obj)
	{
		std::swap(obj.ctrl_block, ctrl_block);
		std::swap(obj.data, data);
	}

	void set(T* ptr, base* _ctrl_block)
	{
		data = ptr;
		ctrl_block = _ctrl_block;
	}

	void remove_reference()
	{
		if (!ctrl_block || --ctrl_block->shared_ref != 0) return;
		if (ctrl_block->weak_ref == 0)
		{
			delete ctrl_block;
			ctrl_block = nullptr;
		}
		else
		{
			ctrl_block->destroy();
		}
	}

	void add_reference()
	{
		if (!ctrl_block) ctrl_block = new ctrl_block_from_ptr<T>(data);
		++ctrl_block->shared_ref;
	}
	template<typename U, typename... Args>
	friend shared_pointer make_shared(Args&&... args);
};

template<typename T, typename... Args>
shared_pointer<T> make_shared(Args&&... args) // no member
{
	shared_pointer<T> tmp;
	auto ctrl_block_tmp = new ctrl_block_make_shared<T>(std::forward<Args>(args)...);
	tmp.internal_reset(ctrl_block_tmp->get(), ctrl_block_tmp);
	return tmp;
}

template<typename L, typename R>
bool operator==(const shared_pointer<L>& _p1, const shared_pointer<R>& _p2)
{
	return _p1.get() == _p2.get();
}

template<typename L>
bool operator==(const shared_pointer<L>& _p1, nullptr_t)
{
	return !_p1;
}

template<typename R>
bool operator==(nullptr_t, const shared_pointer<R>& _p2)
{
	return !_p2;
}

template<typename L, typename R>
bool operator!=(const shared_pointer<L>& _p1, const shared_pointer<R>& _p2)
{
	return _p1.get() != _p2.get();
}


template<typename T>
class weak_pointer {
public:
	friend class shared_pointer<T>;
	constexpr weak_pointer() noexcept
	{
		set(nullptr, nullptr);
	}

	weak_pointer(const weak_pointer& other) noexcept
	{
		set(other.data, other.ctrl_block);
	}

	weak_pointer(const shared_pointer<T>& other) noexcept
	{
		set(other.data, other.ctrl_block);
		++ctrl_block->weak_ref;
	}

	weak_pointer& operator=(const weak_pointer& other)
	{
		weak_pointer(other).swap(*this);
		return *this;
	}

	weak_pointer(weak_pointer&& other) noexcept
	{
		set(other.data, other.ctrl_block);
		other.set(nullptr, nullptr);
	}

	weak_pointer& operator=(weak_pointer&& other) noexcept
	{
		weak_pointer(std::move(other)).swap(*this);
	}

	~weak_pointer()
	{
		--ctrl_block->weak_ref;
		data = nullptr;
		ctrl_block = nullptr;
	}

	size_t use_count() const noexcept
	{
		return ctrl_block->shared_ref.load();
	}

	bool expired() const noexcept
	{
		return ctrl_block->shared_ref.load() <= 0;
	}

	shared_pointer<T> lock() const noexcept
	{
		shared_pointer<T> tmp;
		tmp.set(data, ctrl_block);
		tmp.add_reference();
		return expired() ? shared_pointer<T>() : tmp;
	}


private:
	T* data;
	base* ctrl_block;
	void set(T* ptr, base* _ctrl_block)
	{
		data = ptr;
		ctrl_block = _ctrl_block;
	}

	void swap(weak_pointer& obj)
	{
		std::swap(obj.data, data);
		std::swap(obj.ctrl_block, ctrl_block);
	}
};

template<typename T>
class unique_pointer {
public:

	unique_pointer() noexcept: data(nullptr)
	{}

	unique_pointer(T* ptr) noexcept: data(ptr)
	{}

	unique_pointer(const unique_pointer&) = delete;

	unique_pointer& operator=(const unique_pointer&) = delete;

	unique_pointer(unique_pointer&& other) noexcept
	{
		unique_pointer<T>().swap(*this);
	}

	unique_pointer& operator=(unique_pointer&& other) noexcept
	{
		unique_pointer<T>(std::move(other)).swap(*this);
	}

	~unique_pointer()
	{
		reset();
	}

	T* get()
	{
		return data;
	}

	operator bool() const noexcept
	{
		return data != nullptr;
	}

	T& operator*() const
	{
		return *data;
	}

	T* operator->() const
	{
		return data;
	}

	void reset() noexcept
	{
		delete data;
	}

	T* release() noexcept
	{
		auto ptr = get();
		data = nullptr;
		return ptr;
	}

private:
	T* data;
	void swap(unique_pointer& obj)
	{
		std::swap(obj.data, data);
	}
};

template<typename T>
class unique_pointer<T[]> {
public:
	using pointer_type = T*;
	using element_type = T;

	unique_pointer() noexcept: data(nullptr)
	{}

	unique_pointer(pointer_type ptr) noexcept: data(ptr)
	{}

	unique_pointer(const unique_pointer&) = delete;

	unique_pointer& operator=(const unique_pointer) = delete;

	unique_pointer(unique_pointer&& other) noexcept
	{
		std::swap(other.data, data);
	}

	unique_pointer& operator=(unique_pointer&& other) noexcept
	{
		std::swap(other.data, data);
		return *this;
	}

	~unique_pointer()
	{
		remove();
	}

	operator bool() const noexcept
	{
		return data != nullptr;
	}

	element_type& operator*() const
	{
		return *data;
	}

	pointer_type operator->() const
	{
		return data;
	}

	element_type& operator[](size_t idx) const
	{
		return get()[idx];
	}

	pointer_type get() const
	{
		return data;
	}

	pointer_type release() noexcept
	{
		auto ptr = get();
		data = nullptr;
		return ptr;
	}

	void remove() noexcept
	{
		delete data;
		data = nullptr;
	}

	void reset(pointer_type ptr = nullptr) noexcept
	{
		remove();
		data = ptr;
	}

private:
	T* data;
};