# malloc
模仿Google的tcmalloc的简易实现，内部结构类似于多级缓存的设计。有thread，center，page三个等级。
- 定长内存池：一次性申请一个大块空间来维护/
- thread cache：使用TLS技术，线程独占该部分缓存
- central cache：中心缓存是所有线程所共享，合适的时机回收thread cache中的对象（当thread cache某个桶中对象过多的时候）
