#pragma once
#include"ObjectPool.h"
// 使用基数树 作为map
// linux 页缓存使用该方法
template <int BITS>

class RadixTree {
private:
	// 层高为3的基数树
	// 每一层需要的bits和长度
	static const int INTERIOR_BITS = (BITS + 2) / 3;// 内部bit数量
	static const int INTERIOR_LENGTH = 1 << INTERIOR_BITS;// 内部长度
	// 叶子节点需要的bit和长度
	static const int LEAF_BITS = BITS - 2 * INTERIOR_BITS;
	static const int LEAF_LENGTH = 1 << LEAF_BITS;
	struct Node {
		Node* _ptrs[INTERIOR_LENGTH];
	};
	struct Leaf {
		void* _values[LEAF_LENGTH];
	};

	ObjectPool<Node> _node_allocator;// 使用自己的内存池申请node类型，返回node指针
	ObjectPool<Leaf> _leaf_allocator;
	Node* _root;
#define GetLevel0(ptr) ptr >> (LEAF_BITS + INTERIOR_BITS)
#define GetLevel1(ptr) (ptr >> LEAF_BITS) & (INTERIOR_LENGTH - 1)
#define GetLeaf(ptr) ptr & (INTERIOR_LENGTH - 1)
public:
	typedef uintptr_t int_ptr;//存储的指针数值

	explicit RadixTree() {
		//cout << INTERIOR_BITS << "\n";
		//cout << INTERIOR_LENGTH << "\n";
		//cout << LEAF_BITS << "\n";
		//cout << LEAF_LENGTH << "\n";
		_root = _node_allocator.New();
	}

	void Set(int_ptr ptr, void* v) {
		const int_ptr p1 = GetLevel0(ptr);
		const int_ptr p2 = GetLevel1(ptr);
		const int_ptr p3 = GetLeaf(ptr);
		reinterpret_cast<Leaf*>(_root->_ptrs[p1]->_ptrs[p2])->_values[p3] = v;
	}
	void* Get(int_ptr ptr) {
		const int_ptr p1 = GetLevel0(ptr);
		const int_ptr p2 = GetLevel1(ptr);
		const int_ptr p3 = GetLeaf(ptr);
		if ((ptr >> BITS) > 0 || (_root->_ptrs[p1] == nullptr) ||
			(_root->_ptrs[p1]->_ptrs[p2] == nullptr)) {
			return nullptr;
		}
		return reinterpret_cast<Leaf*>(_root->_ptrs[p1]->_ptrs[p2])->_values[p3];
	}
	void PreAllocate(int_ptr start, size_t n) {
		cout << start << " " << n << endl;
		for (int_ptr key = start; key <= start + n - 1;
			key = ((key >> LEAF_BITS) + 1) << LEAF_BITS) {
			const int_ptr p1 = GetLevel0(key);
			const int_ptr p2 = GetLevel1(key);
			//创建下一层
			if (_root->_ptrs[p1] == nullptr) {
				Node* temp_node = this->_node_allocator.New();
				assert(temp_node);//检测一下
				_root->_ptrs[p1] = temp_node;
			}
			//叶子节点
			if (_root->_ptrs[p1]->_ptrs[p2] == nullptr) {
				Leaf* leaf_temp = this->_leaf_allocator.New();
				//要存到上一层的p2的_ptrs 需要类型转换一下
				assert(leaf_temp);
				_root->_ptrs[p1]->_ptrs[p2] = reinterpret_cast<Node*>(leaf_temp);
			}
		}
	}
};
/*
原来是预分配 但64位是不行的，64位下无法使用基数树，
需要开辟空间过大，如果减少开辟空间就需要多层，这样导致很多间接寻址
性能更下降
*/
