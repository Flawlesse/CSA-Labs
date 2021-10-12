#include <iostream>
#include <mmintrin.h>
#include <immintrin.h>
#include <chrono>
#include <iomanip>


using namespace std::chrono;
using std::cout;
using std::cin;
using std::cerr;
using std::endl;

// must be multiple of 4 !!!
constexpr size_t SIZE = 1000000;


void display_all(__int8* A, __int8* B, __int8* C, __int16* D) {
	cout << "\nA: ";
	for (size_t i = 0; i < SIZE; i++)
		cout << std::setw(10) << (int)A[i];
	cout << endl;

	cout << "\nB: ";
	for (size_t i = 0; i < SIZE; i++)
		cout << std::setw(10) << (int)B[i];
	cout << endl;

	cout << "\nC: ";
	for (size_t i = 0; i < SIZE; i++)
		cout << std::setw(10) << (int)C[i];
	cout << endl;

	cout << "\nD: ";
	for (size_t i = 0; i < SIZE; i++)
		cout << std::setw(10) << (int)D[i];
	cout << endl;
}

void output(__int32* F) {
	for (size_t i = 0; i < SIZE; i++)
		cout << F[i] << "\t";
	cout << endl;
}

void regular(__int8* A, __int8* B, __int8* C, __int16* D, __int32* F) {
	for (size_t i = 0; i < SIZE; i++)
		F[i] = (__int32)((__int16)A[i] - B[i]) + (__int32)(C[i] * D[i]);
}


// 32      8   9max 8     25max=>32bit   8   24max  16     => 32bit result, no overflow
// F[i] = (A[i] -   B[i])     +         (C[i] *     D[i]); i=1..8
// max 4 iteations at a time
void simd(__int8* A, __int8* B, __int8* C, __int16* D, __int32* F) {
	__m64 zero = _mm_set_pi32(0, 0);
	__m64 allones = _mm_set_pi32(-1, -1);
	__int32 res12[2] = { 0, 0 };
	__int32 res34[2] = { 0, 0 };

	__int32 part32a;
	__int32 part32b;
	__int32 part32c;
	__int64 part64d;
	for (int i = 0; i < SIZE; i += 4){
		part32a = (*(__int32*)(A + i));
		part32b = (*(__int32*)(B + i));
		part32c = (*(__int32*)(C + i));
		part64d = (*(__int64*)(D + i));

		__asm {
			; for A[i..i + 3]
			movd mm0, part32a
			movq mm1, mm0
			movq mm2, mm0
			; cmp vect <= 0
			pcmpgtb mm1, zero
			pcmpeqb mm2, zero
			por mm1, mm2
			pxor mm1, allones
			; unpack ssb to ssw
			punpcklbw mm0, mm1
			movq mm3, mm0
			; RESULT IN MM3

			; for B[i..i + 3]
			movd mm0, part32b
			movq mm1, mm0
			movq mm2, mm0
			; cmp vect <= 0
			pcmpgtb mm1, zero
			pcmpeqb mm2, zero
			por mm1, mm2
			pxor mm1, allones
			; unpack ssb to ssw
			punpcklbw mm0, mm1
			movq mm4, mm0
			; RESULT IN MM4

			; A[i..i + 3] - B[i..i + 3]
			psubw mm3, mm4
			; 16bit RESULTS IN MM3
			pxor mm4, mm4

			; UNPACK 8 to 16 bit C
			movd mm0, part32c
			movq mm1, mm0
			movq mm2, mm0
			; cmp vect <= 0
			pcmpgtb mm1, zero
			pcmpeqb mm2, zero
			por mm1, mm2
			pxor mm1, allones
			; unpack ssb to ssw
			punpcklbw mm0, mm1
			movq mm4, mm0
			; RESULT IN MM4

			; DO MULTIPLICATION
			; MM3 is reserved
			movq mm0, part64d
			movq mm1, part64d

			pmullw mm0, mm4
			movq mm2, mm0
			pmulhw mm1, mm4

			punpckhwd mm0, mm1
			punpcklwd mm2, mm1
			; 32bit RESULTS IN MM0 & MM2


			; UNPACK 16 to 32 bit MM3 result
			; MM0, MM2, MM3 are reserved
			; cmp vect <= 0
			movq mm4, mm3
			movq mm5, mm3
			psraw mm4, 15
			; in MM4, we have addable left parts
			movq mm5, mm3
			punpckhwd mm3, mm4
			punpcklwd mm5, mm4
			; RESULTS IN MM3& MM5


			; DO ADDITION
			; MM0, MM2, MM3, MM5 are reserved
			paddd mm0, mm3
			paddd mm2, mm5

			; PUT RESULTS IN MEMORY
			movq res12, mm2
			movq res34, mm0
		}

		F[i] = res12[0];
		F[i + 1] = res12[1];
		F[i + 2] = res34[0];
		F[i + 3] = res34[1];
	}
}

void clear(__int32* F) {
	for (size_t i = 0; i < SIZE; i++)
		F[i] = 0;
}

void fill_random(__int8* A, __int8* B, __int8* C, __int16* D) {
	for (size_t i = 0; i < SIZE; i++){
		A[i] = rand() % (UINT8_MAX + 1);
		B[i] = rand() % (UINT8_MAX + 1);
		C[i] = rand() % (UINT8_MAX + 1);
		D[i] = rand() % (UINT16_MAX + 1);
	}
}

int main() {
	srand(steady_clock::now().time_since_epoch().count());

	__int8* A = (__int8*)calloc(SIZE, 1);
	__int8* B = (__int8*)calloc(SIZE, 1);
	__int8* C = (__int8*)calloc(SIZE, 1);
	__int16* D = (__int16*)calloc(SIZE, 2);
	__int32* F = (__int32*)calloc(SIZE, 4);
	fill_random(A, B, C, D);
	//display_all(A, B, C, D);


	steady_clock::time_point start, stop;
	// REGULAR
	start = steady_clock::now();
	regular(A, B, C, D, F);
	stop = steady_clock::now();
	cout << "\nRegular result:\n";
	//output(F);
	cout << "Executed in " << duration_cast<nanoseconds>(stop - start).count() << "ns" << endl;
	clear(F);
	
	// SIMD
	start = steady_clock::now();
	simd(A, B, C, D, F);
	steady_clock::now(); // for some reason it is always 0
	stop = steady_clock::now();
	cout << "\nSIMD MMX result:\n";
	//output(F);
	cout << "Executed in " << duration_cast<nanoseconds>(stop - start).count() << "ns" << endl;
	clear(F);

	free(A);
	free(B);
	free(C);
	free(D);
	free(F);
}