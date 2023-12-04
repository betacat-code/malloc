#pragma once
#include"Common.h"
struct Span
{
	PAGE_ID _page_id=0;
	size_t _page_num=0;//ҳ����
	Span* _next = nullptr;
	Span* _pre = nullptr;
	size_t _used_counts;//��ʹ�õ�����,��Ϊ0�黹��page cache
	void* _free_list;//����ָ��
	//��ʾspan������ڴ�ռ��зֳ�һ����С���ڴ�������ڸ�����������
	bool _is_used = false;
	size_t _obj_size=0;//spanһ��С����Ĵ�С
};
//��ͷ ˫������
class SpanList {
public:
	std::mutex _mtx;//��
private:
	Span* _head;
public:
	SpanList() {
		_head = new Span;
		_head->_next = _head;//��ͷ˫��ѭ������ ����ɾ����O(1)
		_head->_pre = _head;
	}
	inline Span* Begin() {
		return _head->_next;
	}
	inline Span* End(){
		return _head;
	}
	inline bool Empty() {
		return _head == _head->_next;
	}
	void Insert(Span* pos, Span* new_span) {
		assert(pos);
		assert(new_span);

		//��posλ��ǰ�����
		pos->_pre->_next = new_span;
		new_span->_pre = pos->_pre;
		new_span->_next = pos;
		pos->_pre = new_span;
	}
	void Erase(Span* cur) {// �ѽڵ�ժ�����������տռ�
		assert(cur);
		assert(cur != _head);

		cur->_pre->_next = cur->_next;
		cur->_next->_pre = cur->_pre;
	}

	void PushFront(Span* new_span) {
		Insert(_head->_next, new_span);
	}
	Span* PopFront() {
		Span* erase_span = _head->_next;
		Erase(erase_span);
		return erase_span;
	}

};