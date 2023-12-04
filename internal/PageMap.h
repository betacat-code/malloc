#pragma once
#include"ObjectPool.h"
// ʹ�û����� ��Ϊmap
// linux ҳ����ʹ�ø÷���
template <int BITS>

class RadixTree {
private:
	// ���Ϊ3�Ļ�����
	// ÿһ����Ҫ��bits�ͳ���
	static const int INTERIOR_BITS = (BITS + 2) / 3;// �ڲ�bit����
	static const int INTERIOR_LENGTH = 1 << INTERIOR_BITS;// �ڲ�����
	// Ҷ�ӽڵ���Ҫ��bit�ͳ���
	static const int LEAF_BITS = BITS - 2 * INTERIOR_BITS;
	static const int LEAF_LENGTH = 1 << LEAF_BITS;
	struct Node {
		Node* _ptrs[INTERIOR_LENGTH];
	};
	struct Leaf {
		void* _values[LEAF_LENGTH];
	};

	ObjectPool<Node> _node_allocator;// ʹ���Լ����ڴ������node���ͣ�����nodeָ��
	ObjectPool<Leaf> _leaf_allocator;
	Node* _root;
#define GetLevel0(ptr) ptr >> (LEAF_BITS + INTERIOR_BITS)
#define GetLevel1(ptr) (ptr >> LEAF_BITS) & (INTERIOR_LENGTH - 1)
#define GetLeaf(ptr) ptr & (INTERIOR_LENGTH - 1)
public:
	typedef uintptr_t int_ptr;//�洢��ָ����ֵ

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
			//������һ��
			if (_root->_ptrs[p1] == nullptr) {
				Node* temp_node = this->_node_allocator.New();
				assert(temp_node);//���һ��
				_root->_ptrs[p1] = temp_node;
			}
			//Ҷ�ӽڵ�
			if (_root->_ptrs[p1]->_ptrs[p2] == nullptr) {
				Leaf* leaf_temp = this->_leaf_allocator.New();
				//Ҫ�浽��һ���p2��_ptrs ��Ҫ����ת��һ��
				assert(leaf_temp);
				_root->_ptrs[p1]->_ptrs[p2] = reinterpret_cast<Node*>(leaf_temp);
			}
		}
	}
};
/*
ԭ����Ԥ���� ��64λ�ǲ��еģ�64λ���޷�ʹ�û�������
��Ҫ���ٿռ����������ٿ��ٿռ����Ҫ��㣬�������ºܶ���Ѱַ
���ܸ��½�
*/
