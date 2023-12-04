#include<iostream>
#include<vector>
#include"internal/ThreadCache.h"
#include"internal/MemoryAlloc.h"
void BenchmarkMalloc(size_t ntimes, size_t nworks, size_t rounds);
void BenchmarkMemoryAlloc(size_t ntimes, size_t nworks, size_t rounds);
struct node {
	int x, y;
	char s;
	std::string t;
};
int main()
{
	//BenchmarkMalloc(100, 10, 10);
	BenchmarkMemoryAlloc(1000, 10, 10);
	/*std::atomic<size_t> malloc_costtime = 0;
	std::atomic<size_t> free_costtime = 0;
	int ntimes = 10;
	int rounds = 10000;
	std::vector<node*> v(ntimes);
	for (int i = 0; i < rounds; i++) {
		size_t begin1 = clock();
		for (int j = 0; j < ntimes; j++) {
			v[j] = alloca_obj<node>();
		}
		size_t end1 = clock();

		size_t begin2 = clock();
		for (int j = 0; j < ntimes; j++) {
			free_obj(v[j]);
		}
		size_t end2 = clock();
		malloc_costtime += (end1 - begin1);
		free_costtime += (end2 - begin2);
	}
	printf("花费：%u ms\n",malloc_costtime.load());

	printf("花费：%u ms\n",  free_costtime.load());
	
	return 0;

	*/
}

void test_centralcache() {
	SpanList temp;
	auto t = CentralCache::GetInstance().GetOneSpan(temp, 100);
	cout << t << endl;
	cout << temp.Begin() << endl;
	cout << SizeAlignment::NumMovePage(100) << endl;
	Span* s = temp.Begin();
	cout << s->_page_num << endl;
	CentralCache::GetInstance().ReleseList((void*)(s->_free_list), 100);
}

void BenchmarkMalloc(size_t ntimes, size_t nworks, size_t rounds) {
	std::vector<std::thread> vthread(nworks);
	std::atomic<size_t> malloc_costtime = 0;
	std::atomic<size_t> free_costtime = 0;

	for (size_t k = 0; k < nworks; ++k) {
		vthread[k] = std::thread([&, k] {
			std::vector<node*> v(ntimes);

			for (int i = 0; i < rounds; i++) {
				size_t begin1 = clock();
				for (int j = 0; j < ntimes; j++) {
					v[j] = new node;
				}
				size_t end1 = clock();

				size_t begin2 = clock();
				for (int j = 0; j < ntimes; j++) {
					free(v[j]);
				}
				size_t end2 = clock();
				malloc_costtime += (end1 - begin1);
				free_costtime += (end2 - begin2);
			}
		});
	}
	for (auto& t : vthread) {
		t.join();
	}
	printf("%u个线程并发执行%u轮次，每轮次malloc %u次: 花费：%u ms\n",
		nworks, rounds, ntimes, malloc_costtime.load());

	printf("%u个线程并发执行%u轮次，每轮次free %u次: 花费：%u ms\n",
		nworks, rounds, ntimes, free_costtime.load());

	printf("%u个线程并发malloc&free %u次，总计花费：%u ms\n",
		nworks, nworks * rounds * ntimes, malloc_costtime.load() + free_costtime.load());
}

void BenchmarkMemoryAlloc(size_t ntimes, size_t nworks, size_t rounds) {
	std::vector<std::thread> vthread(nworks);
	std::atomic<size_t> malloc_costtime = 0;
	std::atomic<size_t> free_costtime = 0;

	for (size_t k = 0; k < nworks; ++k) {
		vthread[k] = std::thread([&] {
			std::vector<node*> v(ntimes);
			for (int i = 0; i < rounds; i++) {
				size_t begin1 = clock();
				for (int j = 0; j < ntimes; j++) {
					v[j] = alloca_obj<node>();
				}
				size_t end1 = clock();

				size_t begin2 = clock();
				for (int j = 0; j < ntimes; j++) {
					free_obj(v[j]);
				}
				size_t end2 = clock();
				malloc_costtime += (end1 - begin1);
				free_costtime += (end2 - begin2);
			}
			});
	}
	for (auto& t : vthread) {
		t.join();
	}
	printf("%u个线程并发执行%u轮次，每轮次malloc %u次: 花费：%u ms\n",
		nworks, rounds, ntimes, malloc_costtime.load());

	printf("%u个线程并发执行%u轮次，每轮次free %u次: 花费：%u ms\n",
		nworks, rounds, ntimes, free_costtime.load());

	printf("%u个线程并发malloc&free %u次，总计花费：%u ms\n",
		nworks, nworks * rounds * ntimes, malloc_costtime.load() + free_costtime.load());
}