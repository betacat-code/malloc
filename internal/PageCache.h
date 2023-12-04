#pragma once
#include"PageMap.h"
#include"ObjectPool.h"
#include"Common.h"
#include"Span.h"

class PageCache {
private:
	SpanList _span_lists[NPAGES];
	ObjectPool<Span> _span_pool;
	//RadixTree<64 - PAGE_SHIFT> _id_map;
	// 先使用stl实现 后续再优化

	std::unordered_map<PAGE_ID, Span*> _id_map;

	PageCache() {
		//std::cout << "PageCache called!\n";
	};
	PageCache(const PageCache&) = delete;

public:
	std::mutex _page_mutex;
	static PageCache& GetInstance() {
		static PageCache _ins;//唯一单例
		return _ins;
	}
	//obj到span的映射
	Span* MapToSpan(void* obj) {//外部未加锁
		PAGE_ID id = ((PAGE_ID)obj >> PAGE_SHIFT);
		//Span* res =(Span *) _id_map.Get(id);
		//assert(res);
		_page_mutex.lock();
		auto temp = _id_map.find(id);
		_page_mutex.unlock();
		Span* res=temp->second;
		assert(res!=nullptr);
		return res;
	}
	//释放空余span到cache
	//Span的 usecount==0了即分出去的小obj都还回来了,归还Span并尝试合并
	void ReleseSpan(Span* free_span) {
		if (free_span->_page_num > NPAGES - 1) {
			void* ptr = (void*)(free_span->_page_id << PAGE_SHIFT);
			SystemFree(ptr);
			_span_pool.Delete(free_span);
			return;
		}
		//向前合并
		while (1) {
			PAGE_ID pre_id = free_span->_page_id - 1;
			//Span* pre_span = (Span*)_id_map.Get(pre_id);
			//if (!pre_span) break;

			auto ret = _id_map.find(pre_id);
			if (ret == _id_map.end()) break;
			Span* pre_span = ret->second;



			if (pre_span->_is_used) break;//前一个页在使用

			//超出最大页的限制 不合并
			if (pre_span->_page_num + free_span->_page_num > NPAGES - 1) break;

			free_span->_page_id = pre_span->_page_id;
			free_span->_page_num += pre_span->_page_num;
			
			//删除被合并的前一个span
			_span_lists[pre_span->_page_num].Erase(pre_span);
			_span_pool.Delete(pre_span);

		}
		//向后合并
		while (1) {
			PAGE_ID pre_id = free_span->_page_id +free_span->_page_num;
			
			//Span* next_span = (Span*)_id_map.Get(pre_id);
			//if (!next_span) break;

			auto ret = _id_map.find(pre_id);
			if (ret == _id_map.end()) break;
			Span* next_span = ret->second;

			if (next_span->_is_used) break;
			if (next_span->_page_num + free_span->_page_num > NPAGES - 1) break;
			
			//向后合并页号不变
			free_span->_page_num += next_span->_page_num;
			_span_lists[next_span->_page_num].Erase(next_span);
			_span_pool.Delete(next_span);
		}
		_span_lists[free_span->_page_num].PushFront(free_span);
		free_span->_is_used = false;
		//记录首尾页号
		_id_map[free_span->_page_id] = free_span;
		_id_map[free_span->_page_id + free_span->_page_num - 1] = free_span;
		//_id_map.Set(free_span->_page_id, free_span);
		//_id_map.Set(free_span->_page_id+free_span->_page_num-1, free_span);
	}
	//获取k页的span
	Span* NewSpan(size_t k){
		assert(k >0);
		if (k > NPAGES - 1) {
			void* ptr = SystemAlloc(k);
			Span* t_span = _span_pool.New();
			t_span->_page_id = (PAGE_ID)ptr >> PAGE_SHIFT;
			t_span->_page_num = k;

			//_id_map.Set(t_span->_page_id, t_span);
			_id_map[t_span->_page_id] = t_span;
			return t_span;
		}
		//检查第K个桶
		if (!_span_lists[k].Empty()) {
			Span* t_span = _span_lists[k].PopFront();
			//方便回收小内存时查找对应span
			for (PAGE_ID i = 0; i < t_span->_page_num; ++i) {
				_id_map[t_span->_page_num + i] = t_span;

				//_id_map.Set(t_span->_page_num + i, t_span);
			}
			return t_span;
		}

		for (size_t i = k + 1; i < NPAGES; ++i) {
			if (!_span_lists[i].Empty()) {
				//cout << i << endl;
				Span* n_span = _span_lists[i].PopFront();
				//nSpan代表切割以后剩下的span，还是未使用的
				Span* k_span = _span_pool.New();
				//代表从某个大内存n+k Span中切出来的kSpan
				//要给到central之后进行obj切割，进而往thread给

				//在nspan头部切割大小为k的页
				k_span->_page_id = n_span->_page_id;
				k_span->_page_num = k;

				n_span->_page_id += k;
				n_span->_page_num -= k;
				_span_lists[n_span->_page_num].PushFront(n_span);
				
				//cout << n_span->_page_id << endl;
				
				// 存储nSpan的首尾页号跟n_span映射，方便page cache回收内存时进行的合并查找
				_id_map[n_span->_page_id] = n_span;
				_id_map[n_span->_page_id + n_span->_page_num - 1] = n_span;
				//_id_map.Set(n_span->_page_id, n_span);
				//_id_map.Set(n_span->_page_id + n_span->_page_num - 1, n_span);

				//其中的页全部放进去
				for (size_t i = 0; i < k; i++) {
					//cout << i << endl;
					//_id_map.Set(k_span->_page_id + i, k_span);
					_id_map[k_span->_page_id + i] = k_span;
				}
				return k_span;
			}
		}
		//此时已经没有大页的span 从堆中再申请一个
		Span* big_span = _span_pool.New();
		void* ptr = SystemAlloc(NPAGES - 1);
		//cout << "ptr" << (PAGE_ID)ptr << endl;

		big_span->_page_id = (PAGE_ID)ptr >> PAGE_SHIFT;
		big_span->_page_num = NPAGES - 1;
		_span_lists[NPAGES - 1].PushFront(big_span);
		//申请了大页 重新调用一次
		return NewSpan(k);
	}
};