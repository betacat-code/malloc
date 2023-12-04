#pragma once
#include"CentralCache.h"
#include"Common.h"

class ThreadCache {
private:
	FreeList _thread_memory_lists[NFREELIST];
public:
	void* Allocate(size_t size) {
		assert(size <= MAX_BYTES);
		size_t align_size = SizeAlignment::RoundUp(size);
		size_t index = SizeAlignment::Index(size);

		//_thread_memory_lists[index].test();

		if (!_thread_memory_lists[index].Empty()) {
			return _thread_memory_lists[index].Pop();
		}
		else {
			return FetchFromCentralCache(index, align_size);
		}
	}
	void Deallocate(void* ptr, size_t size) {
		assert(ptr);
		size_t index = SizeAlignment::Index(size);
		_thread_memory_lists[index].Push(ptr);

		//_thread_memory_lists[index].test();

		//插入后超过最大限制 归还内存
		if (_thread_memory_lists[index].size() >=
			_thread_memory_lists[index].maxsize()) {

			ReclaimMemory(_thread_memory_lists[index], size);
		}

	}
	// 从中心缓存获取对象
	void* FetchFromCentralCache(size_t index, size_t size) {
		//慢开始反馈算法
		auto& max_size = _thread_memory_lists[index].maxsize();
		size_t batch_num = min(max_size, SizeAlignment::NumMoveSize(size));
		//cout << "batch_num" << batch_num << endl;
		if (batch_num == max_size) {
			++max_size;
		}
		void* start = nullptr;
		void* end = nullptr;
		size_t actual_num = CentralCache::GetInstance().FetchRangeObj(start, end, batch_num, size);
		//cout << "actual_num" << actual_num << endl;
		assert(actual_num > 0);
		if (actual_num != 1) {
			assert(NextObj(start));
			//cout << "FetchFromCentralCache " << end <<"  "<< &NextObj (end)<< endl;
			_thread_memory_lists[index].PushRange(NextObj(start), end, actual_num - 1);
		}

		//_thread_memory_lists[index].test();
		return start;
	}

	// 释放对象时，链表过长时，回收内存回到中心缓存
	void ReclaimMemory(FreeList& list, size_t size) {
		void* start = nullptr;
		void* end = nullptr;
		list.PopRange(start, end, list.maxsize());
		CentralCache::GetInstance().ReleseList(start, size);
	}
};