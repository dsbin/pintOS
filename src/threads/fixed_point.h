#define F (1 << 14)
//fixed point 1
#define INT_MAX ((1 << 31) - 1)
#define INT_MIN (-(1 << 31))
// x and y denote fixed_point numbers in 17.14 format
// n is an integer


/*integer를 fixed point로 전환*/
int int_to_fp(int n);
/*FP를 int로 전환(반올림)*/	
int fp_to_int_round(int x);
/*FP를 int로 전환(버림)*/
int fp_to_int(int x);
/*FP와 덧셈*/
int add_fp(int x, int y);
/*FP와 int의 덧셈*/
int add_mixed(int x, int n);
/*FP의 뺄셈(x-y)*/
int sub_fp(int x, int y);
/*FP와 int의 뺄셈(x-n)*/	
int sub_mixed(int x, int n);
/*FP의 곱셈*/
int mult_fp(int x, int y);
/*FP의 int의 곱셈*/
int mult_mixed(int x, int y);
/*FP이의 나눗셈(x/y)*/
int div_fp(int x, int y);
/*FP와 int 나눗셈 (x/n)*/
int div_mixed(int x, int n);


/*integer를 fixed point로 전환*/
int int_to_fp(int n) {return n*F;}
/*FP를 int로 전환(반올림)*/	
int fp_to_int_round(int x) {return (x+(F/2))/F;}
/*FP를 int로 전환(버림)*/
int fp_to_int(int x) { return x/F; }		
/*FP와 덧셈*/
int add_fp(int x, int y) { return x+y; }	
/*FP와 int의 덧셈*/
int add_mixed(int x, int n) { return x+int_to_fp(n); }	
/*FP의 뺄셈(x-y)*/
int sub_fp(int x, int y) { return x-y; }
/*FP와 int의 뺄셈(x-n)*/	
int sub_mixed(int x, int n) { return x-int_to_fp(n); }
/*FP의 곱셈*/
int mult_fp(int x, int y) { return x*y; }
/*FP의 int의 곱셈*/
int mult_mixed(int x, int y) { return x+int_to_fp(y); }
/*FP이의 나눗셈(x/y)*/
int div_fp(int x, int y) { return x/y; }
/*FP와 int 나눗셈 (x/n)*/
int div_mixed(int x, int n) { return x/int_to_fp(n); }




