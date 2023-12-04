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
	// ��ʹ��stlʵ�� �������Ż�

	std::unordered_map<PAGE_ID, Span*> _id_map;

	PageCache() {
		//std::cout << "PageCache called!\n";
	};
	PageCache(const PageCache&) = delete;

public:
	std::mutex _page_mutex;
	static PageCache& GetInstance() {
		static PageCache _ins;//Ψһ����
		return _ins;
	}
	//obj��span��ӳ��
	Span* MapToSpan(void* obj) {//�ⲿδ����
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
	//�ͷſ���span��cache
	//Span�� usecount==0�˼��ֳ�ȥ��Сobj����������,�黹Span�����Ժϲ�
	void ReleseSpan(Span* free_span) {
		if (free_span->_page_num > NPAGES - 1) {
			void* ptr = (void*)(free_span->_page_id << PAGE_SHIFT);
			SystemFree(ptr);
			_span_pool.Delete(free_span);
			return;
		}
		//��ǰ�ϲ�
		while (1) {
			PAGE_ID pre_id = free_span->_page_id - 1;
			//Span* pre_span = (Span*)_id_map.Get(pre_id);
			//if (!pre_span) break;

			auto ret = _id_map.find(pre_id);
			if (ret == _id_map.end()) break;
			Span* pre_span = ret->second;



			if (pre_span->_is_used) break;//ǰһ��ҳ��ʹ��

			//�������ҳ������ ���ϲ�
			if (pre_span->_page_num + free_span->_page_num > NPAGES - 1) break;

			free_span->_page_id = pre_span->_page_id;
			free_span->_page_num += pre_span->_page_num;
			
			//ɾ�����ϲ���ǰһ��span
			_span_lists[pre_span->_page_num].Erase(pre_span);
			_span_pool.Delete(pre_span);

		}
		//���ϲ�
		while (1) {
			PAGE_ID pre_id = free_span->_page_id +free_span->_page_num;
			
			//Span* next_span = (Span*)_id_map.Get(pre_id);
			//if (!next_span) break;

			auto ret = _id_map.find(pre_id);
			if (ret == _id_map.end()) break;
			Span* next_span = ret->second;

			if (next_span->_is_used) break;
			if (next_span->_page_num + free_span->_page_num > NPAGES - 1) break;
			
			//���ϲ�ҳ�Ų���
			free_span->_page_num += next_span->_page_num;
			_span_lists[next_span->_page_num].Erase(next_span);
			_span_pool.Delete(next_span);
		}
		_span_lists[free_span->_page_num].PushFront(free_span);
		free_span->_is_used = false;
		//��¼��βҳ��
		_id_map[free_span->_page_id] = free_span;
		_id_map[free_span->_page_id + free_span->_page_num - 1] = free_span;
		//_id_map.Set(free_span->_page_id, free_span);
		//_id_map.Set(free_span->_page_id+free_span->_page_num-1, free_span);
	}
	//��ȡkҳ��span
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
		//����K��Ͱ
		if (!_span_lists[k].Empty()) {
			Span* t_span = _span_lists[k].PopFront();
			//�������С�ڴ�ʱ���Ҷ�Ӧspan
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
				//nSpan�����и��Ժ�ʣ�µ�span������δʹ�õ�
				Span* k_span = _span_pool.New();
				//�����ĳ�����ڴ�n+k Span���г�����kSpan
				//Ҫ����central֮�����obj�и������thread��

				//��nspanͷ���и��СΪk��ҳ
				k_span->_page_id = n_span->_page_id;
				k_span->_page_num = k;

				n_span->_page_id += k;
				n_span->_page_num -= k;
				_span_lists[n_span->_page_num].PushFront(n_span);
				
				//cout << n_span->_page_id << endl;
				
				// �洢nSpan����βҳ�Ÿ�n_spanӳ�䣬����page cache�����ڴ�ʱ���еĺϲ�����
				_id_map[n_span->_page_id] = n_span;
				_id_map[n_span->_page_id + n_span->_page_num - 1] = n_span;
				//_id_map.Set(n_span->_page_id, n_span);
				//_id_map.Set(n_span->_page_id + n_span->_page_num - 1, n_span);

				//���е�ҳȫ���Ž�ȥ
				for (size_t i = 0; i < k; i++) {
					//cout << i << endl;
					//_id_map.Set(k_span->_page_id + i, k_span);
					_id_map[k_span->_page_id + i] = k_span;
				}
				return k_span;
			}
		}
		//��ʱ�Ѿ�û�д�ҳ��span �Ӷ���������һ��
		Span* big_span = _span_pool.New();
		void* ptr = SystemAlloc(NPAGES - 1);
		//cout << "ptr" << (PAGE_ID)ptr << endl;

		big_span->_page_id = (PAGE_ID)ptr >> PAGE_SHIFT;
		big_span->_page_num = NPAGES - 1;
		_span_lists[NPAGES - 1].PushFront(big_span);
		//�����˴�ҳ ���µ���һ��
		return NewSpan(k);
	}
};