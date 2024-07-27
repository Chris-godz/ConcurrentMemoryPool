//#include "ConcurrentAlloc.h"
//
//void MultiThreadAlloc1()
//{
//	for (size_t i = 0; i < 5; ++i)
//	{
//		void* ptr = ConcurrentAlloc(6);
//	}
//}
//
//void MultiThreadAlloc2()
//{
//	for (size_t i = 0; i < 5; ++i)
//	{
//		void* ptr = ConcurrentAlloc(7);
//	}
//}
//
//void TLSTest()
//{
//	std::thread t1(MultiThreadAlloc1);
//	t1.join();
//
//	std::thread t2(MultiThreadAlloc2);
//	t2.join();
//}
//void TestConcurrentAlloc2()
//{
//	for (size_t i = 0; i < 1024; ++i)
//	{
//		void* p1 = ConcurrentAlloc(6);
//		cout << p1 << endl;
//	}
//
//	void* p2 = ConcurrentAlloc(8);
//	cout << p2 << endl;
//}
//void TestMultiThread()
//{
//	std::thread t1(MultiThreadAlloc1);
//	std::thread t2(MultiThreadAlloc2);
//
//	t1.join();
//	t2.join();
//}
//void TestBigAlloc()
//{
//	void* p1 = ConcurrentAlloc(257 * 1024);
//	ConcurrentFree(p1);
//
//	void* p2 = ConcurrentAlloc(129 * 8 * 1024);
//	ConcurrentFree(p2);
//}
//
//int main()
//{
//	TLSTest();
//	TestConcurrentAlloc2();
//	TestMultiThread();
//	TestBigAlloc();
//	//cout << sizeof Span <<"    " << sizeof ThreadCache; //64  4992
//	return 0;
//}