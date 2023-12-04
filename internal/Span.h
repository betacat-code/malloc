#pragma once
#include"Common.h"
struct Span
{
	PAGE_ID _page_id=0;
	size_t _page_num=0;//页数量
	Span* _next = nullptr;
	Span* _pre = nullptr;
	size_t _used_counts;//已使用的数量,减为0归还给page cache
	void* _free_list;//自由指针
	//表示span这个大内存空间切分成一个个小的内存块对象挂在该自由链表上
	bool _is_used = false;
	size_t _obj_size=0;//span一个小对象的大小
};
//带头 双向链表
class SpanList {
public:
	std::mutex _mtx;//锁
private:
	Span* _head;
public:
	SpanList() {
		_head = new Span;
		_head->_next = _head;//带头双向循环链表 插入删除都O(1)
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

		//在pos位置前面插入
		pos->_pre->_next = new_span;
		new_span->_pre = pos->_pre;
		new_span->_next = pos;
		pos->_pre = new_span;
	}
	void Erase(Span* cur) {// 把节点摘下来但不回收空间
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