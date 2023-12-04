#pragma once
#include"Common.h"
#include"Span.h"
#include"PageCache.h"
//����ģʽ
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
	//��ȡһ��size��С��span
	Span* GetOneSpan(SpanList& list,size_t size) {
		//�鿴�Ƿ����δ�����span
		Span* temp = list.Begin();
		Span* list_end = list.End();
		while (temp != list_end) {
			if (temp->_free_list) {
				return temp;
			}
			else temp = temp->_next;
		}

		list._mtx.unlock();//���� ���¹��̺͵����е�list�޹���

		PageCache::GetInstance()._page_mutex.lock();
		//�����Ҫ��ҳ
		//cout << "GET NEW SPAN\n";
		Span* span = PageCache::GetInstance().NewSpan(SizeAlignment::NumMovePage(size));
		span->_is_used = true;
		span->_obj_size = size;
		PageCache::GetInstance()._page_mutex.unlock();
		
		//�Ը�span���л���
		char* start = (char*)(span->_page_id << PAGE_SHIFT);
		size_t bytes = span->_page_num << PAGE_SHIFT;//�ܹ������ֽ���
		char* end = start + bytes;

		//β�巨 ÿsize���ֽ�Ϊһ��
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
	//�����Ļ����ȡһ�������Ķ����thread cache
	size_t FetchRangeObj(void*& start, void*& end, size_t batch_num, size_t size) {
		size_t index = SizeAlignment::Index(size);
		_span_lists[index]._mtx.lock();
		Span* span = GetOneSpan(_span_lists[index], size);
		
		assert(span->_free_list);
		// ��span�л�ȡbatchNum������
		// �������batchNum�����ж����ö���
		start = span->_free_list;
		end = start;
		int n = 1;//Ĭ�ϸ�start
		for (int i = 1; i <= batch_num - 1; ++i) {
			if (NextObj(end) == nullptr) break;
			end=NextObj(end);
			++n;
		}
		//span�г�ȥ��objʹ��
		span->_used_counts += n;
		span->_free_list = NextObj(end);
		//cout <<"FetchRangeObj  " << end << " " << NextObj(end) << endl;
		NextObj(end) = nullptr;
		_span_lists[index]._mtx.unlock();
		return n;

	}
	// ��һ��������obj�ͷŵ�span obj��СΪsize
	void ReleseList(void* start, size_t size) {
		size_t index = SizeAlignment::Index(size);
		_span_lists[index]._mtx.lock();

		while (start) {
			void* next = NextObj(start);
			Span* span = PageCache::GetInstance().MapToSpan(start);

			assert(span);
			//ͷ�嵽��ʱspan������������ start��Ӧ��span
			NextObj(start) = span->_free_list;
			
			//assert(span->_free_list);

			span->_free_list = start;
			--span->_used_counts;

			if (span->_used_counts == 0) {
				//��span���е�obj���󶼻ص���freelist��
				//���ո�pagecache���ϲ�����
				_span_lists[index].Erase(span);
				span->_pre = nullptr;
				span->_next = nullptr;
				span->_free_list = nullptr;

				_span_lists[index]._mtx.unlock();//��ʱ����pagecache

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