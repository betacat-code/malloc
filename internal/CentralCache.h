#pragma once
#include"Common.h"
#include"Span.h"
#include"PageCache.h"
//单例模式
class CentralCache {
private:
	CentralCache() {
		//cout << "CentralCache called!\n";
	};
	CentralCache(CentralCache&) = delete;
	SpanList _span_lists[NFREELIST];
public:
	static CentralCache& GetInstance() {
		static CentralCache _ins;

		return _ins;
	}
	//获取一个size大小的span
	Span* GetOneSpan(SpanList& list,size_t size) {
		//查看是否存在未分配的span
		Span* temp = list.Begin();
		Span* list_end = list.End();
		while (temp != list_end) {
			if (temp->_free_list) {
				return temp;
			}
			else temp = temp->_next;
		}

		list._mtx.unlock();//解锁 以下过程和单例中的list无关了

		PageCache::GetInstance()._page_mutex.lock();
		//计算出要几页
		//cout << "GET NEW SPAN\n";
		Span* span = PageCache::GetInstance().NewSpan(SizeAlignment::NumMovePage(size));
		span->_is_used = true;
		span->_obj_size = size;
		PageCache::GetInstance()._page_mutex.unlock();
		
		//对该span进行划分
		char* start = (char*)(span->_page_id << PAGE_SHIFT);
		size_t bytes = span->_page_num << PAGE_SHIFT;//总共申请字节数
		char* end = start + bytes;

		//尾插法 每size个字节为一块
		span->_free_list = start;
		start += size;
		void* tail = span->_free_list;
		while (start < end) {
			NextObj(tail) = start;
			tail = start;
			start += size;
		}
		//cout << "GetOneSpan " <<tail <<" "<< NextObj(tail) << endl;
		NextObj(tail) = nullptr;
		list._mtx.lock();
		list.PushFront(span);
		return span;

	}
	//从中心缓存获取一定数量的对象给thread cache
	size_t FetchRangeObj(void*& start, void*& end, size_t batch_num, size_t size) {
		size_t index = SizeAlignment::Index(size);
		_span_lists[index]._mtx.lock();
		Span* span = GetOneSpan(_span_lists[index], size);
		
		assert(span->_free_list);
		// 从span中获取batchNum个对象
		// 如果不够batchNum个，有多少拿多少
		start = span->_free_list;
		end = start;
		int n = 1;//默认给start
		for (int i = 1; i <= batch_num - 1; ++i) {
			if (NextObj(end) == nullptr) break;
			end=NextObj(end);
			++n;
		}
		//span切出去给obj使用
		span->_used_counts += n;
		span->_free_list = NextObj(end);
		//cout <<"FetchRangeObj  " << end << " " << NextObj(end) << endl;
		NextObj(end) = nullptr;
		_span_lists[index]._mtx.unlock();
		return n;

	}
	// 将一定数量的obj释放到span obj大小为size
	void ReleseList(void* start, size_t size) {
		size_t index = SizeAlignment::Index(size);
		_span_lists[index]._mtx.lock();

		while (start) {
			void* next = NextObj(start);
			Span* span = PageCache::GetInstance().MapToSpan(start);

			assert(span);
			//头插到此时span的自由链表中 start对应的span
			NextObj(start) = span->_free_list;
			
			//assert(span->_free_list);

			span->_free_list = start;
			--span->_used_counts;

			if (span->_used_counts == 0) {
				//该span所有的obj对象都回到了freelist中
				//回收给pagecache做合并尝试
				_span_lists[index].Erase(span);
				span->_pre = nullptr;
				span->_next = nullptr;
				span->_free_list = nullptr;

				_span_lists[index]._mtx.unlock();//此时交给pagecache

				PageCache::GetInstance()._page_mutex.lock();
				PageCache::GetInstance().ReleseSpan(span);
				PageCache::GetInstance()._page_mutex.unlock();

				_span_lists[index]._mtx.lock();
			}
			start = next;
		}
		_span_lists[index]._mtx.unlock();
	}

};