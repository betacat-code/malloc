# malloc
模仿Google的tcmalloc的简易实现，内部结构类似于多级缓存的设计。有thread，center，page三个等级。
- 定长内存池：一次性申请一个大块空间来维护
- thread cache：使用TLS技术，线程独占该部分缓存
- central cache：中心缓存是所有线程所共享，合适的时机回收thread cache中的对象（当thread cache某个桶中对象过多的时候）
- page cache：存储的内存是以页为单位存储及分 配的，central cache没有内存对象时，从page cache分配出一定数量的page，并切割成定长大小的小块内存


# 涉及到的知识点

侵入式链表：侵入式单链表的结点链接域存的是下一个结点的链接域成员变量的内存首地址）（而不是节点地址，这样可以兼容各种数据类型）
<pre><code>
static void*& NextObj(void* obj) {
  assert(obj);
  return *(void**)obj;
}
</code></pre>
 * (void**)obj，为在32或者64位下都有准确的空间存储链接地址，void**obj是一个二级指针，解引用后就是一个一级指针，并且32位系统或者64位系统都可以使用。

单例模式:central cache，page cache全局只能出现一个，设计为单例。
<pre><code>
class PageCache {
private:
  PageCache() {
	//std::cout << "PageCache called!\n";
};
	PageCache(const PageCache&) = delete;
public:
  	static PageCache& GetInstance() {
		static PageCache _ins;
		return _ins;
	}
</code></pre>
在第一次被使用时才进行初始化，只有GetInstance()可以调用构造函数。

# 一个潜在bug

当一个线程申请空间过多时，侵入式链表有时候会突然断链。排查很长时间都不知道为什么，大概会有20%的概率出现问题。感叹技不如人
