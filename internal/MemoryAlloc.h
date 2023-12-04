#pragma once
#include"Common.h"
#include"ThreadCache.h"
static thread_local ThreadCache TLS;

template<typename T>
static T* alloca_obj() {
	size_t size = sizeof(T);
	if (size > MAX_BYTES) {
		//一次申请内存大于thread最大的256kb，则直接向page层按页直接申请
		size_t align_size = SizeAlignment::RoundUp(size);
		size_t kpage = align_size >> PAGE_SHIFT;

		PageCache::GetInstance()._page_mutex.lock();
		Span* span = PageCache::GetInstance().NewSpan(kpage);
		span->_obj_size = size;
		PageCache::GetInstance()._page_mutex.unlock();
		T* ptr = (T*)(span->_page_id << PAGE_SHIFT);
		return ptr;
	}
	else {
		return (T*)TLS.Allocate(size);
	}
}

static void free_obj(void* p) {
	Span* span = PageCache::GetInstance().MapToSpan(p);
	//找到对应span获取大小
	size_t size = span->_obj_size;
	if (size > MAX_BYTES) {
		PageCache::GetInstance()._page_mutex.lock();
		PageCache::GetInstance().ReleseSpan(span);
		PageCache::GetInstance()._page_mutex.unlock();
	}
	else {
		TLS.Deallocate(p, size);
	}
}

