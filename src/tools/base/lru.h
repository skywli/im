#ifndef _Lru_H_
#define _Lru_H_
#include <map>
#include <stdlib.h>
#include <iostream>
using namespace std;
template< typename K ,typename V>
class Lru {
public:

	Lru(void) : m_head(NULL), m_tail(NULL) {}
	~Lru(void) {
		clear();
	}

	// 向首部压入
	void push(const K& key, const V& value)
	{
		if (m_nodemap.find(key) != m_nodemap.end()) {
			return;
		}
		m_head = new Node(key, value, NULL, m_head);
		if (m_head->m_next)
			m_head->m_next->m_prev = m_head;
		else
			m_tail = m_head;

		m_nodemap[key] = m_head;
	}


	// 获取尾元素
	pair<K, V>& back(void) {
		if (empty())
			throw underflow_error("链表下溢！");
		return m_tail->m_pair;
	}
	const pair<K, V>& back(void) const {
		return const_cast<Lru*> (this)->back();
	}

	// 从尾部弹出
	void pop(void) {
		if (empty()) {
			return;
		}
		Node* prev = m_tail->m_prev;
		m_nodemap.erase(m_tail->m_pair.first);
		delete m_tail;
		m_tail = prev;
		if (m_tail)
			m_tail->m_next = NULL;
		else
			m_head = NULL;
	}

	void clear(void) {
		for (Node* next; m_head; m_head = next) {
			next = m_head->m_next;
			delete m_head;
		}
		m_head = NULL;
		m_tail = NULL;
		m_nodemap.clear();
	}

	bool empty(void) const {
		return m_head == NULL && m_tail == NULL;
	}

	size_t size(void) const {
		size_t counter = 0;
		for (Node* node = m_head; node;
			node = node->m_next)
			++counter;
		return counter;
	}

	friend ostream& operator<< (ostream& os,
		const Lru& list) {
		for (Node* node = list.m_head; node;
			node = node->m_next)
			os << *node;
		return os;
	}
public:
	// 节点模板
	class Node {
	public:
		Node(const K& key, const V& value, Node* prev = NULL,
			Node* next = NULL) :
			m_pair(key, value), m_prev(prev),
			m_next(next) {}
		friend ostream& operator<< (ostream& os,
			const Node& node) {
			return os << '(' << node.m_pair.first << " : " << node.m_pair.second << ')';
		}
		pair<K, V> m_pair; // 数据
		Node*      m_prev; // 前指针
		Node*      m_next; // 后指针
	};

	Node* m_head;
	Node* m_tail;
	std::map<K, Node*>    m_nodemap;

public:
	// 正向迭代器
	class iterator {
	public:
		iterator(typename std::map<K, Node*>::iterator it) :m_it(it) {}

		bool operator== (const iterator& it) const {
			return m_it == it.m_it;
		}

		bool operator!= (const iterator& it) const {
			return !(*this == it);
		}
		iterator operator++ (void) {
			m_it++;
			return *this;
		}
		const iterator operator++ (int) {
			iterator old = *this;
			++*this;
			return old;
		}
		iterator operator-- (void) {
			m_it--;
			return *this;
		}
		const iterator operator-- (int) {
			iterator old = *this;
			--*this;
			return old;
		}
		pair<K, V>& operator* (void) const {
			return  m_it->second->m_pair;
		}
		pair<K, V>* operator-> (void) const {
			return &**this;
		}
	private:
		typename map<K, Node*>::iterator m_it;
		friend class Lru;
	};

	iterator begin(void) {
		return iterator(m_nodemap.begin());
	}

	iterator end(void) {
		return iterator(m_nodemap.end());
	}

	/**
	* 删除
	*/
	void erase(iterator loc) {

		if (loc == end()) {
			return;
		}
		Node* node = loc.m_it->second;
		if (node->m_prev) {
			node->m_prev->m_next = node->m_next;
		}
		else {
			m_head = node->m_next;
		}

		if (node->m_next) {
			node->m_next->m_prev = node->m_prev;
		}
		else {
			m_tail = node->m_prev;
		}
		m_nodemap.erase(loc.m_it);
        delete node;

	}

	/**
	* 查找
	*/
	iterator find(const K& key) {
		iterator it(m_nodemap.find(key));
		if (it != end()) {
			Node* node = it.m_it->second;
			if (node == m_head) {
				return it;
			}
			if (node->m_prev) {
				node->m_prev->m_next = node->m_next;
			}
			else {
				m_head = node->m_next;
			}

			if (node->m_next) {
				node->m_next->m_prev = node->m_prev;
			}
			else {
				m_tail = node->m_prev;
			}

			//插入头部
			node->m_next = m_head;
			node->m_prev = NULL;
			m_head->m_prev = node;
			m_head = node;
		}
		return it;
	}

public:
	// 反向迭代器
	class reverse_iterator {
	public:
		reverse_iterator(Node* it) :m_it(it) {}

		bool operator== (const reverse_iterator& it) const {
			return m_it == it.m_it;
		}

		bool operator!= (const reverse_iterator& it) const {
			return !(*this == it);
		}
		reverse_iterator operator++ (void) {
			m_it = m_it->m_prev;
			return *this;
		}
		const reverse_iterator operator++ (int) {
			reverse_iterator old = *this;
			++*this;
			return old;
		}
		reverse_iterator operator-- (void) {
			m_it = m_it->m_next;
			return *this;
		}
		const reverse_iterator operator-- (int) {
			reverse_iterator old = *this;
			--*this;
			return old;
		}
		pair<K, V>& operator* (void) const {
			return  m_it->m_pair;
		}
		pair<K, V>* operator-> (void) const {
			return &**this;
		}
	private:
		Node*  m_it;
		friend class Lru;
	};
	reverse_iterator rbegin(void) {
		return reverse_iterator(m_tail);
	}

	reverse_iterator rend(void) {
		return reverse_iterator(NULL);
	}
	/**
	* 删除
	*/
	void erase(reverse_iterator loc) {

		if (loc == rend()) {
			return;
		}
		Node* node = loc.m_it;
		if (node->m_prev) {
			node->m_prev->m_next = node->m_next;
		}
		else {
			m_head = node->m_next;
		}

		if (node->m_next) {
			node->m_next->m_prev = node->m_prev;
		}
		else {
			m_tail = node->m_prev;
		}
		m_nodemap.erase(node->m_pair.first);
		delete node;

	}
};

#endif
