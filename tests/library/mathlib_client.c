/* uses the library MathLib (tests/library/mathlib.pas) from C */
#include <stdio.h>
#include "MathLib.h"

int main(void) {
    printf("Add: %d\n", Add(2, 3));
    printf("Area: %.4f\n", mathlib_area(2.0));
    printf("Fib(30): %lld\n", (long long)Fib(30));
    printf("CountChar: %d\n", CountChar("banana", 'a'));
    printf("Shout: %d\n", Shout("perfect"));
    TRect r = {{0, 0}, {10, 20}, true};
    TPoint m = Middle(r);
    printf("Middle: %d %d\n", m.X, m.Y);
    printf("Width: %d\n", Width(&r));
    printf("Next: %d %d\n", Next(TColor_Green), Next(TColor_Blue));
    printf("IsEven: %d %d\n", IsEven(4), IsEven(7));
    Hello();
    fflush(stdout);
    printf("Calls: %d\n", Calls());
    return 0;
}
