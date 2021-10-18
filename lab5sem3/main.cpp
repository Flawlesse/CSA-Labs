#include <iostream>
#include <omp.h>
#include <vector>
#include <time.h>
#include <iomanip>

using std::cout;
using std::endl;
using std::setw;

constexpr int size_x = 8;
constexpr int size_h = 6;
typedef unsigned long long  ulong;

// in case you want bigger than 0x7fff, max 0xffffffff
int my_rand() {
	int a = rand(), b = rand();
	int result = (a * (RAND_MAX + 1) + b) * 4 + a % 4;
	return result;
}


void display_all(const std::vector<int>& x, const std::vector<int>& h) {
	cout << "X: ";
	for (size_t i = 0; i < x.size(); i++) {
		cout << setw(12) << x[i];
	}
	cout << endl;

	cout << "H: ";
	for (size_t i = 0; i < h.size(); i++) {
		cout << setw(12) << h[i];
	}
	cout << endl;
}

void fill_random(std::vector<int>& x, std::vector<int>& h) {
	const int size = std::min(x.size(), h.size());
	for (size_t i = 0; i < size; ++i) {
		x[i] = my_rand();
		h[i] = my_rand();
	}
	if (size == x.size())
		for (size_t i = size; i < h.size(); ++i)
			h[i] = my_rand();
	else
		for (size_t i = size; i < x.size(); ++i)
			x[i] = my_rand();
}


// REGULAR
std::vector<long long> conversion_regular(const std::vector<int>& x, const std::vector<int>& h) {
	if (!x.size() && !h.size()) {
		return std::vector<long long>();
	}

	std::vector<int> a;
	std::vector<int> b;
	if (x.size() < h.size()) {
		a = x;
		b = h;
	}
	else {
		a = h;
		b = x;
	}

	std::vector<long long> result(a.size() + b.size() - 1, 0);
	for (size_t i = 0; i < a.size(); ++i) {
		for (size_t j = 0; j < b.size(); ++j) {
			result[i + j] += (long long)a[i] * b[j];
		}
	}
	return result;
}



// OMP
std::vector<long long> conversion_omp(const std::vector<int>& x, const std::vector<int>& h) {
	if (!x.size() && !h.size()) {
		return std::vector<long long>();
	}

	std::vector<int> a;
	std::vector<int> b;
	if (x.size() < h.size()) {
		a = x;
		b = h;
	}
	else {
		a = h;
		b = x;
	}

	int i, j;
	std::vector<long long> result(a.size() + b.size() - 1, 0);
	for (i = 0; i < a.size(); ++i) {
		#pragma omp parallel for shared(a, b, result, i) private(j)
		for (j = 0; j < b.size(); ++j) {
			result[i + j] += (long long)a[i] * b[j];
			printf("Thread ID: %d, result a[%d] * b[%d] = %lld\n", omp_get_thread_num(), i, j, result[i + j]);
		}
		printf("\n\n");
	}
	return result;
}

bool are_equal(const std::vector<long long>& a, const std::vector<long long>& b) {
	if (a.size() != b.size())
		return false;
	for (size_t i = 0; i < a.size(); i++)
		if (a[i] != b[i])
			return false;
	return true;
}

void output(const std::vector<long long>& v) {
	for (auto& elem : v)
		cout << setw(22) << elem;
	cout << endl;
}

// ENTRYPOINT
int main()
{
	srand(time(NULL));
	omp_set_num_threads(4);

	std::vector<int> X(size_x);
	std::vector<int> H(size_h);
	fill_random(X, H);
	display_all(X, H);

	std::vector<long long> result_reg = conversion_regular(X, H);
	std::vector<long long> result_omp = conversion_omp(X, H);

	cout << "\nResult regular:\n";
	output(result_reg);
	cout << "\nResult OMP:\n";
	output(result_omp);

	cout << "\n\nAre both results equal? : " << std::boolalpha << are_equal(result_reg, result_omp) << endl;
	return 0;
}